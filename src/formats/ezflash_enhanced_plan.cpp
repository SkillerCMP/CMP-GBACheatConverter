#include "formats/ezflash_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::ezflash::detail {
namespace {

std::string_view width_name(std::uint8_t width) {
    switch (width) {
    case 1U: return "W8";
    case 2U: return "W16";
    case 4U: return "W32";
    default: return {};
    }
}

std::uint32_t width_mask(std::uint8_t width) {
    if (width == 1U) return 0xFFU;
    if (width == 2U) return 0xFFFFU;
    return 0xFFFFFFFFU;
}

std::string scalar_value(std::uint32_t value, std::uint8_t width) {
    return text::hex(value & width_mask(width), width * 2U);
}

bool aligned(std::uint32_t address, std::uint8_t width) {
    return (width == 1U) ||
           (width == 2U && (address & 1U) == 0U) ||
           (width == 4U && (address & 3U) == 0U);
}

std::vector<ConditionTerm> all_condition_terms(const Operation& operation) {
    std::vector<ConditionTerm> terms;
    terms.push_back(ConditionTerm{
        operation.address, operation.value, operation.width});
    terms.insert(terms.end(), operation.condition_terms.begin(),
                 operation.condition_terms.end());
    return terms;
}

bool operation_is_rom_condition(const Operation& operation) {
    if (operation.condition_has_mask) return false;
    const auto terms = all_condition_terms(operation);
    return !terms.empty() &&
           std::all_of(terms.begin(), terms.end(), [](const ConditionTerm& term) {
               return is_rom_address(term.address);
           });
}

std::optional<std::string> encode_scalar_write(
    std::uint32_t address,
    std::uint32_t value,
    std::uint8_t width,
    std::string_view entry_name,
    std::vector<std::string>& warnings) {
    const auto compact = compact_write_address(address);
    const std::string_view width_token = width_name(width);
    if (!compact || width_token.empty() || !aligned(address, width)) {
        warnings.push_back(std::string(entry_name) +
            ": EZ-Flash Enhanced E7 cannot encode width-aware write at " +
            text::hex(address, 8));
        return std::nullopt;
    }
    return std::string(width_token) + ':' + text::hex(*compact, 1) + ',' +
           scalar_value(value, width) + ';';
}

struct PayloadEncodingPlan {
    std::vector<std::string> commands;
    std::size_t runtime_records = 0U;
    std::size_t runtime_write_work = 0U;
    std::size_t text_length = 0U;
};

std::uint32_t payload_value(const std::vector<std::uint8_t>& bytes,
                            std::size_t offset,
                            std::uint8_t width) {
    std::uint32_t value = 0U;
    for (std::uint8_t index = 0U; index < width; ++index) {
        value |= static_cast<std::uint32_t>(bytes[offset + index])
                 << (index * 8U);
    }
    return value;
}

bool compact_sequence_is_contiguous(std::uint32_t address,
                                    std::uint8_t width,
                                    std::size_t count) {
    const auto first = compact_write_address(address);
    if (!first || !aligned(address, width)) return false;
    for (std::size_t index = 0U; index < count; ++index) {
        const std::uint64_t full = static_cast<std::uint64_t>(address) +
            static_cast<std::uint64_t>(index) * width;
        if (full > std::numeric_limits<std::uint32_t>::max()) return false;
        const auto compact = compact_write_address(
            static_cast<std::uint32_t>(full));
        const std::uint64_t expected = static_cast<std::uint64_t>(*first) +
            static_cast<std::uint64_t>(index) * width;
        if (!compact || static_cast<std::uint64_t>(*compact) != expected) {
            return false;
        }
    }
    return true;
}

bool plan_is_better(const PayloadEncodingPlan& candidate,
                    const PayloadEncodingPlan& current) {
    if (candidate.runtime_records != current.runtime_records) {
        return candidate.runtime_records < current.runtime_records;
    }
    if (candidate.runtime_write_work != current.runtime_write_work) {
        return candidate.runtime_write_work < current.runtime_write_work;
    }
    return candidate.text_length < current.text_length;
}

std::optional<PayloadEncodingPlan> compact_pattern_plan(
    const Operation& operation,
    const std::vector<std::uint8_t>& bytes,
    const PayloadEncodingPlan& scalar) {
    std::optional<PayloadEncodingPlan> best;
    constexpr std::uint8_t widths[] = {4U, 2U, 1U};

    for (const std::uint8_t width : widths) {
        if (bytes.size() % width != 0U) continue;
        const std::size_t count = bytes.size() / width;
        if (count < 2U || count > 0xFFFFFFFFULL ||
            !compact_sequence_is_contiguous(operation.address, width, count)) {
            continue;
        }
        const auto compact = compact_write_address(operation.address);
        if (!compact) continue;

        std::vector<std::uint32_t> values;
        values.reserve(count);
        for (std::size_t index = 0U; index < count; ++index) {
            values.push_back(payload_value(bytes, index * width, width));
        }

        const bool fill = std::all_of(
            values.begin() + 1U, values.end(),
            [&](std::uint32_t value) { return value == values.front(); });
        if (fill) {
            PayloadEncodingPlan candidate;
            candidate.commands.push_back(
                "FILL:" + std::string(width_name(width)) + ',' +
                text::hex(*compact, 1) + ',' +
                text::hex(static_cast<std::uint32_t>(count), 8) + ',' +
                scalar_value(values.front(), width) + ';');
            candidate.runtime_records = 2U;
            candidate.runtime_write_work = count;
            candidate.text_length = candidate.commands.front().size();
            if (plan_is_better(candidate, scalar) &&
                (!best || plan_is_better(candidate, *best))) {
                best = std::move(candidate);
            }
            continue;
        }

        const std::uint32_t mask = width_mask(width);
        const std::uint32_t delta =
            (values[1U] - values[0U]) & mask;
        bool slide = delta != 0U;
        for (std::size_t index = 2U; slide && index < values.size(); ++index) {
            slide = values[index] == ((values[index - 1U] + delta) & mask);
        }
        if (!slide) continue;

        PayloadEncodingPlan candidate;
        candidate.commands.push_back(
            "SLIDE:" + std::string(width_name(width)) + ',' +
            text::hex(*compact, 1) + ',' +
            text::hex(static_cast<std::uint32_t>(count), 8) + ',' +
            text::hex(static_cast<std::uint32_t>(width), 8) + ',' +
            text::hex(delta, 8) + ',' +
            scalar_value(values.front(), width) + ';');
        candidate.runtime_records = 4U;
        candidate.runtime_write_work = count;
        candidate.text_length = candidate.commands.front().size();
        if (plan_is_better(candidate, scalar) &&
            (!best || plan_is_better(candidate, *best))) {
            best = std::move(candidate);
        }
    }
    return best;
}

bool append_payload_writes(const Operation& operation,
                           EnhancedEncodedOption& encoded,
                           std::vector<std::string>& warnings,
                           std::string_view entry_name) {
    const std::vector<std::uint8_t> bytes = operation_payload(operation);
    if (bytes.empty()) {
        warnings.push_back(std::string(entry_name) +
            ": EZ-Flash Enhanced E7 write has no representable payload");
        return false;
    }

    PayloadEncodingPlan scalar;
    std::size_t offset = 0U;
    while (offset < bytes.size()) {
        const std::uint32_t address = operation.address +
            static_cast<std::uint32_t>(offset);
        const std::size_t remaining = bytes.size() - offset;
        std::uint8_t width = 1U;
        if (remaining >= 4U && aligned(address, 4U)) {
            width = 4U;
        } else if (remaining >= 2U && aligned(address, 2U)) {
            width = 2U;
        }
        const std::uint32_t value = payload_value(bytes, offset, width);
        const auto command = encode_scalar_write(
            address, value, width, entry_name, warnings);
        if (!command) return false;
        scalar.text_length += command->size();
        scalar.commands.push_back(*command);
        ++scalar.runtime_records;
        ++scalar.runtime_write_work;
        offset += width;
    }

    const auto compact = compact_pattern_plan(operation, bytes, scalar);
    const PayloadEncodingPlan& selected = compact ? *compact : scalar;
    encoded.commands.insert(encoded.commands.end(),
                            selected.commands.begin(), selected.commands.end());
    encoded.runtime_records += selected.runtime_records;
    encoded.runtime_write_work += selected.runtime_write_work;
    return true;
}

bool append_action(const Operation& operation,
                   EnhancedEncodedOption& encoded,
                   std::vector<std::string>& warnings,
                   std::string_view entry_name,
                   bool inside_runtime_condition) {
    const std::uint32_t repeat = operation.repeat == 0U ? 1U : operation.repeat;

    if (operation.kind == OperationKind::Write) {
        const std::vector<std::uint8_t> bytes = operation_payload(operation);
        if (bytes.empty()) return append_payload_writes(
            operation, encoded, warnings, entry_name);

        if (repeat == 1U) {
            return append_payload_writes(operation, encoded, warnings, entry_name);
        }

        if (bytes.size() != 1U && bytes.size() != 2U && bytes.size() != 4U) {
            warnings.push_back(std::string(entry_name) +
                ": repeated Enhanced E7 write requires a W8/W16/W32 payload");
            return false;
        }
        const std::uint8_t width = static_cast<std::uint8_t>(bytes.size());
        const auto compact = compact_write_address(operation.address);
        if (!compact || !aligned(operation.address, width)) {
            warnings.push_back(std::string(entry_name) +
                ": repeated Enhanced E7 write address is not representable");
            return false;
        }
        std::uint32_t value = 0U;
        for (std::uint8_t index = 0U; index < width; ++index) {
            value |= static_cast<std::uint32_t>(bytes[index]) << (index * 8U);
        }
        const std::int32_t address_step = operation.address_step == 0
            ? static_cast<std::int32_t>(width)
            : operation.address_step;
        const bool fill = operation.value_step == 0 &&
            address_step == static_cast<std::int32_t>(width);
        std::ostringstream command;
        command << (fill ? "FILL:" : "SLIDE:") << width_name(width) << ','
                << text::hex(*compact, 1) << ',' << text::hex(repeat, 8);
        if (!fill) {
            command << ',' << text::hex(
                static_cast<std::uint32_t>(address_step), 8)
                    << ',' << text::hex(
                static_cast<std::uint32_t>(operation.value_step), 8);
        }
        command << ',' << scalar_value(value, width) << ';';
        encoded.commands.push_back(command.str());
        encoded.runtime_records += fill ? 2U : 4U;
        encoded.runtime_write_work += repeat;
        return true;
    }

    if (operation.kind == OperationKind::Add ||
        operation.kind == OperationKind::Subtract) {
        const std::vector<std::uint8_t> bytes = operation_payload(operation);
        if (bytes.size() != 1U && bytes.size() != 2U && bytes.size() != 4U) {
            warnings.push_back(std::string(entry_name) +
                ": Enhanced E7 ADD/SUB requires a W8/W16/W32 value");
            return false;
        }
        const std::uint8_t width = static_cast<std::uint8_t>(bytes.size());
        const auto compact = compact_write_address(operation.address);
        if (!compact || !aligned(operation.address, width)) {
            warnings.push_back(std::string(entry_name) +
                ": Enhanced E7 arithmetic address is not representable");
            return false;
        }
        std::uint32_t value = 0U;
        for (std::uint8_t index = 0U; index < width; ++index) {
            value |= static_cast<std::uint32_t>(bytes[index]) << (index * 8U);
        }
        encoded.commands.push_back(
            std::string(operation.kind == OperationKind::Add ? "ADD:" : "SUB:") +
            std::string(width_name(width)) + ',' + text::hex(*compact, 1) + ',' +
            scalar_value(value, width) + ';');
        ++encoded.runtime_records;
        ++encoded.runtime_write_work;
        return true;
    }

    if (operation.kind == OperationKind::PointerWrite) {
        const std::vector<std::uint8_t> bytes = operation_payload(operation);
        if (bytes.size() != 1U && bytes.size() != 2U && bytes.size() != 4U) {
            warnings.push_back(std::string(entry_name) +
                ": Enhanced E7 PTR requires a W8/W16/W32 payload");
            return false;
        }
        const std::uint8_t width = static_cast<std::uint8_t>(bytes.size());
        const auto compact = compact_write_address(operation.address);
        if (!compact || (operation.address & 3U) != 0U) {
            warnings.push_back(std::string(entry_name) +
                ": Enhanced E7 pointer base is not representable");
            return false;
        }
        std::uint32_t value = 0U;
        for (std::uint8_t index = 0U; index < width; ++index) {
            value |= static_cast<std::uint32_t>(bytes[index]) << (index * 8U);
        }
        encoded.commands.push_back(
            "PTR:" + std::string(width_name(width)) + ',' +
            text::hex(*compact, 1) + ',' + text::hex(operation.pointer_offset, 8) +
            ',' + scalar_value(value, width) + ';');
        encoded.runtime_records += 2U;
        ++encoded.runtime_write_work;
        return true;
    }

    if (operation.kind == OperationKind::RomPatch &&
        rom_patch_is_direct_image_write(operation)) {
        if (inside_runtime_condition) {
            warnings.push_back(std::string(entry_name) +
                ": ROM patches cannot be controlled by a runtime IF branch");
            return false;
        }
        Operation normalized = operation;
        const auto canonical = canonical_rom_address(operation.address);
        if (!canonical) return false;
        normalized.address = *canonical;
        std::vector<ByteWrite> bytes;
        append_expanded_write(bytes, normalized);
        const auto tokens = emit_rom_byte_run_tokens(bytes, warnings);
        if (tokens.empty()) return false;
        for (const std::string& token : tokens) {
            encoded.commands.push_back("ROM:" + token);
        }
        return true;
    }

    warnings.push_back(std::string(entry_name) +
        ": operation has no exact EZ-Flash Enhanced E7 representation");
    return false;
}

bool append_rom_guards(const Operation& operation,
                       EnhancedEncodedOption& encoded,
                       std::vector<std::string>& warnings,
                       std::string_view entry_name) {
    std::vector<ByteWrite> bytes;
    for (const ConditionTerm& term : all_condition_terms(operation)) {
        if (term.width == 0U || term.width > 4U || !is_rom_address(term.address)) {
            warnings.push_back(std::string(entry_name) +
                ": ROMIF condition is not representable");
            return false;
        }
        const auto part = flatten(term.address, term.value, term.width);
        bytes.insert(bytes.end(), part.begin(), part.end());
    }
    const auto tokens = emit_rom_byte_run_tokens(bytes, warnings);
    if (tokens.empty()) return false;
    for (const std::string& token : tokens) {
        encoded.commands.push_back("ROMIF:" + token);
    }
    return true;
}

bool append_runtime_condition_header(
    const Operation& operation,
    EnhancedEncodedOption& encoded,
    std::vector<std::string>& warnings,
    std::string_view entry_name,
    std::size_t& condition_count) {
    const bool logical_mask =
        operation.kind == OperationKind::IfAnd ||
        operation.kind == OperationKind::IfNand;
    const bool masked = operation.condition_has_mask || logical_mask;
    const std::string_view key = logical_mask
        ? (operation.kind == OperationKind::IfAnd
               ? std::string_view("IFNE")
               : std::string_view("IF"))
        : condition_key_for_kind(operation.kind);
    if (key.empty()) {
        warnings.push_back(std::string(entry_name) +
            ": condition type has no Enhanced E7 equivalent");
        return false;
    }

    const std::vector<ConditionTerm> terms = all_condition_terms(operation);
    if (terms.empty()) return false;
    if (masked && terms.size() != 1U) {
        warnings.push_back(std::string(entry_name) +
            ": compound masked condition has no exact Enhanced E7 form");
        return false;
    }
    if (terms.size() > 1U && operation.kind != OperationKind::IfEqual) {
        warnings.push_back(std::string(entry_name) +
            ": compound non-equality condition has no exact Enhanced E7 form");
        return false;
    }
    if (terms.size() > 1U &&
        (operation.condition_has_else || operation.condition_else_span != 0U)) {
        warnings.push_back(std::string(entry_name) +
            ": compound condition with ELSE has no compact Enhanced E7 form");
        return false;
    }

    for (const ConditionTerm& term : terms) {
        if (term.width != 1U && term.width != 2U && term.width != 4U) {
            warnings.push_back(std::string(entry_name) +
                ": runtime condition requires W8/W16/W32 width");
            return false;
        }
        const auto compact = compact_condition_address(term.address);
        if (!compact || !aligned(term.address, term.width)) {
            warnings.push_back(std::string(entry_name) +
                ": runtime condition address is not representable");
            return false;
        }
        const std::uint32_t mask = logical_mask
            ? term.value : operation.condition_mask;
        const std::uint32_t comparison_value = logical_mask
            ? 0U : term.value;
        std::string command_key(key);
        if (masked) command_key += 'M';
        std::string command = command_key + ':' +
            std::string(width_name(term.width)) + ',' +
            text::hex(*compact, 1) + ',';
        if (masked) {
            command += scalar_value(mask, term.width) + ',';
        }
        command += scalar_value(comparison_value, term.width) + ';';
        encoded.commands.push_back(std::move(command));
        encoded.runtime_records += masked ? 2U : 1U;
        ++condition_count;
    }
    return true;
}

bool encode_range(const CheatEntry& entry,
                  std::size_t first,
                  std::size_t count,
                  EnhancedEncodedOption& encoded,
                  std::vector<std::string>& warnings,
                  bool inside_runtime_condition) {
    if (first > entry.operations.size() ||
        count > entry.operations.size() - first) {
        warnings.push_back(entry.name +
            ": condition span exceeds the available operations");
        return false;
    }

    const std::size_t end = first + count;
    std::size_t index = first;
    while (index < end) {
        const Operation& operation = entry.operations[index];
        if (operation.kind == OperationKind::EncryptionSeed ||
            operation.kind == OperationKind::GameId) {
            ++index;
            continue;
        }
        if (operation.kind == OperationKind::Hook) {
            warnings.push_back(entry.name +
                ": hook/master dependency cannot be represented by EZ-Flash");
            return false;
        }

        if (!is_condition(operation.kind)) {
            if (!append_action(operation, encoded, warnings, entry.name,
                               inside_runtime_condition)) {
                return false;
            }
            ++index;
            continue;
        }

        const std::size_t true_span = operation.condition_span == 0U
            ? 1U : static_cast<std::size_t>(operation.condition_span);
        const std::size_t false_span =
            static_cast<std::size_t>(operation.condition_else_span);
        const std::size_t controlled = true_span + false_span;
        if (controlled == 0U || controlled > end - index - 1U) {
            warnings.push_back(entry.name +
                ": condition does not contain all controlled operations");
            return false;
        }

        if (operation_is_rom_condition(operation)) {
            if (operation.kind != OperationKind::IfEqual ||
                operation.condition_has_else || false_span != 0U) {
                warnings.push_back(entry.name +
                    ": ROMIF supports equality without ELSE only");
                return false;
            }
            if (!append_rom_guards(operation, encoded, warnings, entry.name) ||
                !encode_range(entry, index + 1U, true_span,
                              encoded, warnings, false)) {
                return false;
            }
            index += 1U + controlled;
            continue;
        }

        std::size_t condition_count = 0U;
        if (!append_runtime_condition_header(
                operation, encoded, warnings, entry.name, condition_count) ||
            !encode_range(entry, index + 1U, true_span,
                          encoded, warnings, true)) {
            return false;
        }

        if (operation.condition_has_else || false_span != 0U) {
            if (condition_count != 1U || false_span == 0U) {
                warnings.push_back(entry.name +
                    ": invalid or non-compact ELSE condition");
                return false;
            }
            encoded.commands.push_back("ELSE;");
            ++encoded.runtime_records;
            if (!encode_range(entry, index + 1U + true_span, false_span,
                              encoded, warnings, true)) {
                return false;
            }
        }
        for (std::size_t term = 0U; term < condition_count; ++term) {
            encoded.commands.push_back("ENDIF;");
            ++encoded.runtime_records;
        }
        index += 1U + controlled;
    }
    return true;
}

} // namespace

std::string_view condition_key_for_kind(OperationKind kind) {
    switch (kind) {
    case OperationKind::IfEqual: return "IF";
    case OperationKind::IfNotEqual: return "IFNE";
    case OperationKind::IfLess: return "IFLT";
    case OperationKind::IfGreater: return "IFGT";
    case OperationKind::IfLessOrEqual: return "IFLE";
    case OperationKind::IfGreaterOrEqual: return "IFGE";
    default: return {};
    }
}

std::vector<std::uint8_t> operation_payload(const Operation& operation) {
    if (!operation.byte_payload.empty()) return operation.byte_payload;
    if (operation.width == 0U || operation.width > 4U) return {};
    std::vector<std::uint8_t> bytes;
    bytes.reserve(operation.width);
    for (std::uint8_t index = 0U; index < operation.width; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(
            operation.value >> (index * 8U)));
    }
    return bytes;
}

std::optional<EnhancedEncodedOption> encode_enhanced_v4_option(
    const CheatEntry& entry,
    std::vector<std::string>& warnings) {
    const CheatEntry optimized = optimize_enhanced_v4_entry(entry);
    EnhancedEncodedOption encoded;
    if (!encode_range(optimized, 0U, optimized.operations.size(), encoded,
                      warnings, false) ||
        encoded.commands.empty()) {
        return std::nullopt;
    }
    return encoded;
}

} // namespace gba::ezflash::detail
