#include "export/output_modes_internal.hpp"

#include "core/text.hpp"
#include "formats/ezflash.hpp"
#include "formats/retroarch_cht.hpp"
#include "formats/mgba_cheats.hpp"
#include "formats/mednafen_cht.hpp"

#include <cstdint>
#include <optional>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {

Result export_ezflash(const CheatDocument& document,
                      const Options& options) {
    ezflash::Options ez_options;
    ez_options.mode = options.ezflash_mode == EzFlashMode::Original
        ? ezflash::Mode::Original
        : ezflash::Mode::Enhanced;

    const ezflash::Result converted =
        ezflash::export_document(document, ez_options);

    Result result;
    result.data = bytes_from_text(converted.text);
    result.warnings = converted.warnings;

    if (!converted.text.empty()) {
        const CheatDocument reparsed = ezflash::parse(converted.text);
        result.exported_entries = reparsed.entries.size();
        for (const CheatEntry& entry : reparsed.entries) {
            result.exported_records += entry.operations.size();
        }
        result.warnings.insert(result.warnings.end(),
                               reparsed.warnings.begin(),
                               reparsed.warnings.end());
    }

    // The normal EZ exporter marks the overall conversion unsuccessful when
    // any source entry is incompatible. Native Save Output As follows the
    // other native writers: save all compatible entries and report omissions.
    result.success = !result.data.empty() || document.entries.empty();
    return result;
}

Result export_myboy(const CheatDocument& document) {
    Result result;
    std::ostringstream out;
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<cheats>\n";
    for (const CheatEntry& entry : document.entries) {
        if (const auto fcd = exact_fcd(entry)) {
            out << " <cheat type=\"cb\" name=\"" << xml_escape(entry.name)
                << "\">\n";
            for (const auto& line : *fcd) {
                out << "  <code>" << text::hex(line.first, 8U) << ' '
                    << text::hex(line.second, 4U) << "</code>\n";
                ++result.exported_records;
            }
            out << " </cheat>\n";
            ++result.exported_entries;
        } else if (const auto ar = exact_armax(entry, false)) {
            out << " <cheat type=\"gs3\" name=\"" << xml_escape(entry.name)
                << "\">\n";
            for (const auto& line : *ar) {
                out << "  <code>" << text::hex(line.first, 8U) << ' '
                    << text::hex(line.second, 8U) << "</code>\n";
                ++result.exported_records;
            }
            out << " </cheat>\n";
            ++result.exported_entries;
        } else {
            warn_omitted(result, entry, "My Boy!");
        }
    }
    out << "</cheats>\n";
    result.data = bytes_from_text(out.str());
    result.success = result.exported_entries != 0U || document.entries.empty();
    return result;
}

Result export_mgba(const CheatDocument& document) {
    Result result;
    std::vector<mgba_cheats::Record> records;
    for (const CheatEntry& entry : document.entries) {
        mgba_cheats::Record record;
        record.name = single_line_name(entry.name);
        record.enabled = entry.enabled;

        if (entry.mgba) {
            record.family = entry.mgba->family;
            record.code_lines = entry.mgba->code_lines;
            records.push_back(std::move(record));
            ++result.exported_entries;
            result.exported_records += entry.mgba->code_lines.size();
            continue;
        }

        if (const auto fcd = exact_fcd(entry)) {
            record.family = MgbaCodeFamily::AutoDetect;
            for (const auto& line : *fcd) {
                record.code_lines.push_back(
                    text::hex(line.first, 8U) + " " +
                    text::hex(line.second, 4U));
            }
        } else if (const auto ar = exact_armax(entry, true)) {
            // Retain the historical exporter preference while now declaring
            // the family explicitly and preserving the entry's toggle state.
            record.family = MgbaCodeFamily::ProActionReplayV3Encrypted;
            for (const auto& line : *ar) {
                record.code_lines.push_back(
                    text::hex(line.first, 8U) + " " +
                    text::hex(line.second, 8U));
            }
        } else if (const auto gs = exact_gameshark(entry, true)) {
            record.family = MgbaCodeFamily::GameSharkV1Encrypted;
            for (const auto& line : *gs) {
                record.code_lines.push_back(
                    text::hex(line.first, 8U) + " " +
                    text::hex(line.second, 8U));
            }
        } else if (const auto ar_raw = exact_armax(entry, false)) {
            record.family = MgbaCodeFamily::ProActionReplayV3Raw;
            for (const auto& line : *ar_raw) {
                record.code_lines.push_back(
                    text::hex(line.first, 8U) + " " +
                    text::hex(line.second, 8U));
            }
        } else if (const auto gs_raw = exact_gameshark(entry, false)) {
            record.family = MgbaCodeFamily::GameSharkV1Raw;
            for (const auto& line : *gs_raw) {
                record.code_lines.push_back(
                    text::hex(line.first, 8U) + " " +
                    text::hex(line.second, 8U));
            }
        } else {
            warn_omitted(result, entry, "mGBA");
            continue;
        }

        result.exported_records += record.code_lines.size();
        ++result.exported_entries;
        records.push_back(std::move(record));
    }
    result.data = bytes_from_text(mgba_cheats::serialize(records));
    result.success = !records.empty() || document.entries.empty();
    return result;
}

Result export_libretro(const CheatDocument& document) {
    Result result;
    std::vector<retroarch_cht::Record> records;
    for (const CheatEntry& entry : document.entries) {
        if (entry.retroarch) {
            const RetroArchCheatMetadata& metadata = *entry.retroarch;
            retroarch_cht::Record record;
            record.desc = single_line_name(entry.name);
            record.code = metadata.code;
            record.enabled = entry.enabled;
            record.big_endian = metadata.big_endian;
            record.handler = metadata.handler;
            record.memory_search_size = metadata.memory_search_size;
            record.cheat_type = metadata.cheat_type;
            record.value = metadata.value;
            record.address = metadata.address;
            record.address_mask = metadata.address_mask;
            record.rumble_type = metadata.rumble_type;
            record.rumble_value = metadata.rumble_value;
            record.rumble_port = metadata.rumble_port;
            record.rumble_primary_strength =
                metadata.rumble_primary_strength;
            record.rumble_primary_duration =
                metadata.rumble_primary_duration;
            record.rumble_secondary_strength =
                metadata.rumble_secondary_strength;
            record.rumble_secondary_duration =
                metadata.rumble_secondary_duration;
            record.repeat_count = metadata.repeat_count;
            record.repeat_add_to_value = metadata.repeat_add_to_value;
            record.repeat_add_to_address = metadata.repeat_add_to_address;
            records.push_back(std::move(record));
            ++result.exported_records;
            continue;
        }

        retroarch_cht::Record record;
        record.desc = single_line_name(entry.name);
        record.enabled = entry.enabled;
        if (const auto fcd = exact_fcd(entry)) {
            std::ostringstream code;
            for (std::size_t index = 0; index < fcd->size(); ++index) {
                if (index != 0U) code << '+';
                code << text::hex((*fcd)[index].first, 8U) << ' '
                     << text::hex((*fcd)[index].second, 4U);
            }
            record.code = code.str();
            result.exported_records += fcd->size();
        } else if (const auto ar = exact_armax(entry, false)) {
            std::ostringstream code;
            for (std::size_t index = 0; index < ar->size(); ++index) {
                if (index != 0U) code << '+';
                code << text::hex((*ar)[index].first, 8U) << ' '
                     << text::hex((*ar)[index].second, 8U);
            }
            record.code = code.str();
            result.exported_records += ar->size();
        } else {
            warn_omitted(result, entry, "Libretro / RetroArch");
            continue;
        }
        records.push_back(std::move(record));
    }
    result.exported_entries = records.size();
    result.data = bytes_from_text(retroarch_cht::serialize(records));
    result.success = !records.empty() || document.entries.empty();
    return result;
}

Result export_mednafen(const CheatDocument& document,
                       const Options& options) {
    Result result;
    std::vector<mednafen_cht::Record> records;

    const auto valid_md5 = [](std::string_view value) {
        return value.size() == 32U &&
            std::all_of(value.begin(), value.end(), [](unsigned char ch) {
                return std::isxdigit(ch) != 0;
            });
    };
    const auto operation_value = [](const Operation& operation) {
        return operation.has_wide_value
            ? operation.wide_value
            : static_cast<std::uint64_t>(operation.value);
    };
    const auto condition_operator = [](OperationKind kind)
        -> std::optional<mednafen_cht::ConditionOperator> {
        using Operator = mednafen_cht::ConditionOperator;
        switch (kind) {
        case OperationKind::IfGreaterOrEqual:
            return Operator::GreaterOrEqual;
        case OperationKind::IfLessOrEqual:
            return Operator::LessOrEqual;
        case OperationKind::IfGreater: return Operator::Greater;
        case OperationKind::IfLess: return Operator::Less;
        case OperationKind::IfEqual: return Operator::Equal;
        case OperationKind::IfNotEqual: return Operator::NotEqual;
        case OperationKind::IfAnd: return Operator::AndNonzero;
        case OperationKind::IfNand: return Operator::AndZero;
        case OperationKind::IfXor: return Operator::XorNonzero;
        case OperationKind::IfNotXor: return Operator::XorZero;
        case OperationKind::IfOr: return Operator::OrNonzero;
        case OperationKind::IfNotOr: return Operator::OrZero;
        default: return std::nullopt;
        }
    };
    const auto action_type = [](OperationKind kind) -> std::optional<char> {
        switch (kind) {
        case OperationKind::Write: return 'R';
        case OperationKind::Add: return 'A';
        case OperationKind::Transfer: return 'T';
        case OperationKind::ReadSubstitute: return 'S';
        case OperationKind::CompareReadSubstitute: return 'C';
        default: return std::nullopt;
        }
    };

    for (const CheatEntry& entry : document.entries) {
        if (entry.mednafen) {
            const MednafenCheatMetadata& metadata = *entry.mednafen;
            mednafen_cht::Record record;
            record.rom_md5 = metadata.rom_md5;
            record.game_name = metadata.game_name;
            record.name = single_line_name(entry.name);
            record.type = metadata.type;
            record.enabled = entry.enabled;
            record.length = metadata.length;
            record.big_endian = metadata.big_endian;
            record.instance_count = metadata.instance_count;
            record.address = metadata.address;
            record.value = metadata.value;
            record.compare = metadata.compare;
            record.repeat_count = metadata.repeat_count;
            record.repeat_address_increment =
                metadata.repeat_address_increment;
            record.repeat_value_increment = metadata.repeat_value_increment;
            record.copy_source_address = metadata.copy_source_address;
            record.copy_source_increment = metadata.copy_source_increment;
            record.conditions_text = metadata.conditions;
            if (!valid_md5(record.rom_md5) || record.game_name.empty()) {
                if (valid_md5(options.rom_md5) && !options.game_name.empty()) {
                    record.rom_md5 = options.rom_md5;
                    record.game_name = single_line_name(options.game_name);
                } else {
                    warn_omitted(result, entry, "Mednafen");
                    continue;
                }
            }
            records.push_back(std::move(record));
            ++result.exported_entries;
            ++result.exported_records;
            continue;
        }

        if (!valid_md5(options.rom_md5) || options.game_name.empty()) {
            warn_omitted(result, entry, "Mednafen (ROM MD5/game name missing)");
            continue;
        }
        if (entry.operations.empty()) {
            warn_omitted(result, entry, "Mednafen");
            continue;
        }

        std::size_t action_index = 0U;
        std::vector<mednafen_cht::Condition> conditions;
        while (action_index < entry.operations.size()) {
            const Operation& operation = entry.operations[action_index];
            const auto condition = condition_operator(operation.kind);
            if (!condition) break;
            if (operation.width < 1U || operation.width > 8U ||
                operation.condition_has_else ||
                operation.condition_else_span != 0U ||
                operation.condition_has_mask ||
                !operation.condition_terms.empty() ||
                operation.condition_span !=
                    entry.operations.size() - action_index - 1U) {
                conditions.clear();
                break;
            }
            mednafen_cht::Condition converted;
            converted.length = operation.width;
            converted.big_endian = operation.big_endian;
            converted.address = operation.address;
            converted.operation = *condition;
            converted.value = operation_value(operation);
            conditions.push_back(converted);
            ++action_index;
        }
        if (action_index >= entry.operations.size()) {
            warn_omitted(result, entry, "Mednafen");
            continue;
        }

        const std::size_t action_count = entry.operations.size() - action_index;
        const Operation& first = entry.operations[action_index];
        const auto type = action_type(first.kind);
        if (!type || first.width < 1U || first.width > 8U ||
            ((*type == 'S' || *type == 'C') && !conditions.empty())) {
            warn_omitted(result, entry, "Mednafen");
            continue;
        }

        mednafen_cht::Record record;
        record.rom_md5 = options.rom_md5;
        record.game_name = single_line_name(options.game_name);
        record.name = single_line_name(entry.name);
        record.type = *type;
        record.enabled = entry.enabled;
        record.length = first.width;
        record.big_endian = first.big_endian;
        record.address = first.address;
        record.value = operation_value(first);
        record.repeat_count = first.repeat;
        record.repeat_address_increment = first.address_step >= 0
            ? static_cast<std::uint32_t>(first.address_step)
            : 0U;
        record.repeat_value_increment = first.has_wide_value_step
            ? first.wide_value_step
            : (first.value_step >= 0
                ? static_cast<std::uint64_t>(first.value_step)
                : 0U);
        record.copy_source_address = first.source_address;
        record.copy_source_increment = first.source_address_step;
        record.conditions = conditions;
        if (*type == 'C') {
            record.compare = first.has_wide_compare_value
                ? first.wide_compare_value
                : static_cast<std::uint64_t>(first.encoding_auxiliary);
        }

        // Collapse a regular list into Mednafen's extended repeat form so the
        // source entry remains one toggle. Irregular multi-operation entries
        // are rejected rather than split into independent cheats.
        if (action_count > 1U) {
            if (*type == 'S' || *type == 'C' || first.repeat != 1U) {
                warn_omitted(result, entry, "Mednafen");
                continue;
            }
            const Operation& second = entry.operations[action_index + 1U];
            const auto second_type = action_type(second.kind);
            if (!second_type || *second_type != *type ||
                second.width != first.width ||
                second.big_endian != first.big_endian ||
                second.address < first.address ||
                operation_value(second) < operation_value(first) ||
                (*type == 'T' && second.source_address < first.source_address)) {
                warn_omitted(result, entry, "Mednafen");
                continue;
            }
            const std::uint32_t address_increment =
                second.address - first.address;
            const std::uint64_t value_increment =
                operation_value(second) - operation_value(first);
            const std::uint32_t source_increment = *type == 'T'
                ? second.source_address - first.source_address
                : 0U;
            bool regular = true;
            for (std::size_t index = 0U; index < action_count; ++index) {
                const Operation& operation =
                    entry.operations[action_index + index];
                const auto operation_type = action_type(operation.kind);
                const std::uint64_t expected_address =
                    static_cast<std::uint64_t>(first.address) +
                    static_cast<std::uint64_t>(address_increment) * index;
                const std::uint64_t expected_value = operation_value(first) +
                    value_increment * index;
                const std::uint64_t expected_source =
                    static_cast<std::uint64_t>(first.source_address) +
                    static_cast<std::uint64_t>(source_increment) * index;
                if (!operation_type || *operation_type != *type ||
                    operation.width != first.width ||
                    operation.big_endian != first.big_endian ||
                    operation.repeat != 1U ||
                    expected_address > 0xFFFFFFFFULL ||
                    operation.address != expected_address ||
                    operation_value(operation) != expected_value ||
                    (*type == 'T' && operation.source_address !=
                        expected_source)) {
                    regular = false;
                    break;
                }
            }
            if (!regular || action_count > 0xFFFFFFFFULL) {
                warn_omitted(result, entry, "Mednafen");
                continue;
            }
            record.repeat_count = static_cast<std::uint32_t>(action_count);
            record.repeat_address_increment = address_increment;
            record.repeat_value_increment = value_increment;
            record.copy_source_increment = source_increment;
        }

        records.push_back(std::move(record));
        ++result.exported_entries;
        ++result.exported_records;
    }

    result.data = bytes_from_text(mednafen_cht::serialize(records));
    result.success = !records.empty() || document.entries.empty();
    return result;
}

} // namespace gba::output_modes::detail
