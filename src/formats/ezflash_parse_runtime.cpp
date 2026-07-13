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
        if (token.empty()) {
            return std::nullopt;
        }
        tokens.push_back(std::move(token));
        if (end == std::string_view::npos) {
            break;
        }
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
        if (tokens.size() < 2U) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": " + std::string(name) + " requires address and bytes");
            return std::nullopt;
        }
        const auto compact = text::parse_hex_u32(tokens[0]);
        if (!compact) return std::nullopt;
        const auto address = expand_compact_address(*compact, false);
        const auto bytes = parse_byte_tokens(
            tokens, 1U, warnings, line_number, name);
        if (!address || !bytes) return std::nullopt;
        return make_payload_operation(
            name == "ADD" ? OperationKind::Add : OperationKind::Subtract,
            *address, *bytes, line_number, source_text,
            name == "ADD" ? "EZ-Flash Enhanced ADD" :
                            "EZ-Flash Enhanced SUB");
    }

    if (name == "PTR") {
        if (tokens.size() < 3U) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": PTR requires pointer address, 32-bit offset, and bytes");
            return std::nullopt;
        }
        const auto compact = text::parse_hex_u32(tokens[0]);
        const auto offset = text::parse_hex_u32(tokens[1]);
        if (!compact || !offset) return std::nullopt;
        const auto address = expand_compact_address(*compact, false);
        const auto bytes = parse_byte_tokens(
            tokens, 2U, warnings, line_number, "PTR");
        if (!address || !bytes) return std::nullopt;
        Operation operation = make_payload_operation(
            OperationKind::PointerWrite, *address, *bytes,
            line_number, source_text, "EZ-Flash Enhanced pointer write");
        operation.pointer_offset = *offset;
        return operation;
    }

    if (name == "FILL") {
        if (tokens.size() < 3U) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": FILL requires address, count, and pattern bytes");
            return std::nullopt;
        }
        const auto compact = text::parse_hex_u32(tokens[0]);
        const auto count = text::parse_hex_u32(tokens[1]);
        if (!compact || !count || *count == 0U) return std::nullopt;
        const auto address = expand_compact_address(*compact, false);
        const auto bytes = parse_byte_tokens(
            tokens, 2U, warnings, line_number, "FILL");
        if (!address || !bytes) return std::nullopt;
        Operation operation = make_payload_operation(
            OperationKind::Write, *address, *bytes,
            line_number, source_text, "EZ-Flash Enhanced fill");
        operation.repeat = *count;
        operation.address_step = static_cast<std::int32_t>(bytes->size());
        return operation;
    }

    if (name == "SLIDE") {
        if (tokens.size() < 5U) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": SLIDE requires address, count, address step, value step, and bytes");
            return std::nullopt;
        }
        const auto compact = text::parse_hex_u32(tokens[0]);
        const auto count = text::parse_hex_u32(tokens[1]);
        const auto address_step = text::parse_hex_u32(tokens[2]);
        const auto value_step = text::parse_hex_u32(tokens[3]);
        if (!compact || !count || !address_step || !value_step ||
            *count == 0U) {
            return std::nullopt;
        }
        const auto address = expand_compact_address(*compact, false);
        const auto bytes = parse_byte_tokens(
            tokens, 4U, warnings, line_number, "SLIDE");
        if (!address || !bytes) return std::nullopt;
        Operation operation = make_payload_operation(
            OperationKind::Write, *address, *bytes,
            line_number, source_text, "EZ-Flash Enhanced slide");
        operation.repeat = *count;
        operation.address_step = static_cast<std::int32_t>(*address_step);
        operation.value_step = static_cast<std::int32_t>(*value_step);
        return operation;
    }

    return std::nullopt;
}

} // namespace gba::ezflash::parse_detail
