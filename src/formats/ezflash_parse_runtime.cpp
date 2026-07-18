#include "formats/ezflash_parse_internal.hpp"

#include "core/text.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::ezflash::parse_detail {

Operation make_payload_operation(OperationKind kind,
                                 std::uint32_t address,
                                 std::vector<std::uint8_t> bytes,
                                 std::size_t source_line,
                                 std::string_view source_text,
                                 std::string_view note) {
    Operation operation;
    operation.kind = kind;
    operation.address = address;
    operation.source_line = source_line;
    operation.source_text = std::string(source_text);
    operation.note = std::string(note);
    operation.byte_payload = bytes;
    if (bytes.size() <= 4U) {
        operation.width = static_cast<std::uint8_t>(bytes.size());
        operation.value = little_endian_value(bytes, 0U, bytes.size());
    }
    return operation;
}

std::optional<std::vector<std::string>> split_csv_tokens(
    std::string_view payload) {
    std::vector<std::string> tokens;
    std::size_t start = 0U;
    while (start <= payload.size()) {
        const std::size_t end = payload.find(',', start);
        const std::size_t count = end == std::string_view::npos
            ? payload.size() - start
            : end - start;
        std::string token = text::trim(payload.substr(start, count));
        if (token.empty()) return std::nullopt;
        tokens.push_back(std::move(token));
        if (end == std::string_view::npos) break;
        start = end + 1U;
    }
    return tokens;
}

std::optional<std::vector<std::uint8_t>> parse_byte_tokens(
    const std::vector<std::string>& tokens,
    std::size_t first,
    std::vector<std::string>& warnings,
    std::size_t line_number,
    std::string_view label) {
    if (first >= tokens.size()) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": " + std::string(label) + " has no byte payload");
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(tokens.size() - first);
    for (std::size_t index = first; index < tokens.size(); ++index) {
        const auto value = text::parse_hex_u32(tokens[index]);
        if (!value || *value > 0xFFU) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": invalid " + std::string(label) + " byte '" +
                tokens[index] + "'");
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>(*value));
    }
    return bytes;
}

namespace {

std::optional<std::uint8_t> width_for_name(std::string_view name) {
    if (name == "W8") return static_cast<std::uint8_t>(1U);
    if (name == "W16") return static_cast<std::uint8_t>(2U);
    if (name == "W32") return static_cast<std::uint8_t>(4U);
    return std::nullopt;
}

bool value_fits_width(std::uint32_t value, std::uint8_t width) {
    if (width == 1U) return value <= 0xFFU;
    if (width == 2U) return value <= 0xFFFFU;
    return width == 4U;
}

std::optional<std::uint32_t> parse_value_for_width(
    std::string_view token, std::uint8_t width) {
    const auto value = text::parse_hex_u32(token);
    if (!value || !value_fits_width(*value, width)) return std::nullopt;
    return value;
}

std::optional<std::uint32_t> parse_aligned_address(
    std::string_view token, std::uint8_t width, bool condition) {
    const auto compact = text::parse_hex_u32(token);
    if (!compact) return std::nullopt;
    const auto address = expand_compact_address(*compact, condition);
    if (!address) return std::nullopt;
    if ((width == 2U && (*address & 1U) != 0U) ||
        (width == 4U && (*address & 3U) != 0U)) {
        return std::nullopt;
    }
    const auto end = expand_compact_address(
        *compact + static_cast<std::uint32_t>(width - 1U), condition);
    if (!end || *end != *address + width - 1U) return std::nullopt;
    return address;
}

Operation make_scalar_operation(OperationKind kind,
                                std::uint32_t address,
                                std::uint32_t value,
                                std::uint8_t width,
                                std::size_t line_number,
                                std::string_view source_text,
                                std::string_view note) {
    Operation operation;
    operation.kind = kind;
    operation.address = address;
    operation.value = value;
    operation.width = width;
    operation.source_line = line_number;
    operation.source_text = std::string(source_text);
    operation.note = std::string(note);
    return operation;
}

} // namespace

std::optional<Operation> parse_width_write_operation(
    std::string_view name,
    std::string_view payload,
    std::vector<std::string>& warnings,
    std::size_t line_number,
    std::string_view source_text) {
    const auto width = width_for_name(name);
    const auto token_list = split_csv_tokens(payload);
    if (!width || !token_list || token_list->size() != 2U) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": " + std::string(name) + " requires address and value");
        return std::nullopt;
    }
    const auto address = parse_aligned_address((*token_list)[0], *width, false);
    const auto value = parse_value_for_width((*token_list)[1], *width);
    if (!address || !value) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": invalid or unaligned " + std::string(name) + " operation");
        return std::nullopt;
    }
    return make_scalar_operation(OperationKind::Write, *address, *value, *width,
        line_number, source_text, "EZ-Flash Enhanced width-aware write");
}

std::optional<Operation> parse_width_condition_operation(
    std::string_view name,
    std::string_view payload,
    std::vector<std::string>& warnings,
    std::size_t line_number,
    std::string_view source_text) {
    const auto kind = condition_kind_for_key(name);
    const bool masked = condition_key_is_masked(name);
    const auto token_list = split_csv_tokens(payload);
    const std::size_t expected_tokens = masked ? 4U : 3U;
    if (!kind || !token_list || token_list->size() != expected_tokens) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": " + std::string(name) +
            (masked
                ? " requires width, address, mask, and value"
                : " requires width, address, and value"));
        return std::nullopt;
    }
    const auto width = width_for_name((*token_list)[0]);
    if (!width) return std::nullopt;
    const auto address = parse_aligned_address((*token_list)[1], *width, true);
    const std::size_t value_index = masked ? 3U : 2U;
    const auto value = parse_value_for_width((*token_list)[value_index], *width);
    std::optional<std::uint32_t> mask;
    if (masked) {
        mask = parse_value_for_width((*token_list)[2], *width);
    }
    if (!address || !value || (masked && !mask)) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": invalid or unaligned " + std::string(name) + " condition");
        return std::nullopt;
    }
    Operation operation = make_scalar_operation(*kind, *address, *value, *width,
        line_number, source_text,
        masked ? "EZ-Flash Enhanced masked width-aware condition"
               : "EZ-Flash Enhanced width-aware condition");
    operation.condition_span = 0U;
    operation.condition_has_mask = masked;
    operation.condition_mask = masked ? *mask : 0U;
    return operation;
}

std::optional<Operation> parse_named_runtime_operation(
    std::string_view name,
    std::string_view payload,
    std::vector<std::string>& warnings,
    std::size_t line_number,
    std::string_view source_text) {
    const auto token_list = split_csv_tokens(payload);
    if (!token_list) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": malformed " + std::string(name) + " operation");
        return std::nullopt;
    }
    const std::vector<std::string>& tokens = *token_list;

    if (name == "ADD" || name == "SUB") {
        if (tokens.size() != 3U) return std::nullopt;
        const auto width = width_for_name(tokens[0]);
        if (!width) return std::nullopt;
        const auto address = parse_aligned_address(tokens[1], *width, false);
        const auto value = parse_value_for_width(tokens[2], *width);
        if (!address || !value) return std::nullopt;
        return make_scalar_operation(
            name == "ADD" ? OperationKind::Add : OperationKind::Subtract,
            *address, *value, *width, line_number, source_text,
            name == "ADD" ? "EZ-Flash Enhanced ADD" :
                            "EZ-Flash Enhanced SUB");
    }

    if (name == "PTR") {
        if (tokens.size() != 4U) return std::nullopt;
        const auto width = width_for_name(tokens[0]);
        const auto compact = text::parse_hex_u32(tokens[1]);
        const auto offset = text::parse_hex_u32(tokens[2]);
        if (!width || !compact || !offset) return std::nullopt;
        const auto address = expand_compact_address(*compact, false);
        const auto value = parse_value_for_width(tokens[3], *width);
        if (!address || !value || (*address & 3U) != 0U) return std::nullopt;
        Operation operation = make_scalar_operation(
            OperationKind::PointerWrite, *address, *value, *width,
            line_number, source_text, "EZ-Flash Enhanced pointer write");
        operation.pointer_offset = *offset;
        return operation;
    }

    if (name == "FILL") {
        if (tokens.size() != 4U) return std::nullopt;
        const auto width = width_for_name(tokens[0]);
        const auto count = text::parse_hex_u32(tokens[2]);
        if (!width || !count || *count == 0U) return std::nullopt;
        const auto address = parse_aligned_address(tokens[1], *width, false);
        const auto value = parse_value_for_width(tokens[3], *width);
        if (!address || !value) return std::nullopt;
        Operation operation = make_scalar_operation(
            OperationKind::Write, *address, *value, *width,
            line_number, source_text, "EZ-Flash Enhanced compact fill");
        operation.repeat = *count;
        operation.address_step = static_cast<std::int32_t>(*width);
        return operation;
    }

    if (name == "SLIDE") {
        if (tokens.size() != 6U) return std::nullopt;
        const auto width = width_for_name(tokens[0]);
        const auto count = text::parse_hex_u32(tokens[2]);
        const auto address_step = text::parse_hex_u32(tokens[3]);
        const auto value_step = text::parse_hex_u32(tokens[4]);
        if (!width || !count || !address_step || !value_step || *count == 0U) {
            return std::nullopt;
        }
        const auto address = parse_aligned_address(tokens[1], *width, false);
        const auto value = parse_value_for_width(tokens[5], *width);
        if (!address || !value) return std::nullopt;
        Operation operation = make_scalar_operation(
            OperationKind::Write, *address, *value, *width,
            line_number, source_text, "EZ-Flash Enhanced compact slide");
        operation.repeat = *count;
        operation.address_step = static_cast<std::int32_t>(*address_step);
        operation.value_step = static_cast<std::int32_t>(*value_step);
        return operation;
    }

    return std::nullopt;
}

} // namespace gba::ezflash::parse_detail
