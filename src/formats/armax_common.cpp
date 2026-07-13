#include "formats/armax_internal.hpp"

#include "core/text.hpp"
#include "crypto/tea.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace gba::armax::detail {

std::optional<RawLine> parse_line(std::string_view raw,
                                  std::size_t line_number) {
    const std::string line = text::trim(raw);
    if (!text::is_code_line_8x8(line)) {
        return std::nullopt;
    }

    const auto op1 =
        text::parse_hex_u32(std::string_view(line).substr(0, 8));
    const auto op2 =
        text::parse_hex_u32(std::string_view(line).substr(9, 8));
    if (!op1 || !op2) {
        return std::nullopt;
    }

    return RawLine{*op1, *op2, line_number, line};
}

std::string clean_name(std::string line) {
    line = text::trim(line);
    if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
        return line.substr(1, line.size() - 2);
    }
    if (!line.empty() && line.back() == ':') {
        line.pop_back();
        return text::trim(line);
    }
    return line;
}

std::uint8_t decode_width(std::uint32_t op1) {
    const unsigned exponent =
        static_cast<unsigned>((op1 & kWidthMask) >> kWidthShift);
    if (exponent > 2U) {
        return 0;
    }
    return static_cast<std::uint8_t>(1U << exponent);
}

std::uint32_t decode_address(std::uint32_t encoded) {
    return (encoded & 0x000FFFFFU) |
           ((encoded << 4U) & 0x0F000000U);
}

std::optional<std::uint32_t> encode_address(std::uint32_t address) {
    const std::uint32_t region = address & 0x0F000000U;
    if (region != 0x02000000U &&
        region != 0x03000000U &&
        region != 0x04000000U &&
        region != 0x08000000U &&
        region != 0x09000000U) {
        return std::nullopt;
    }

    return (address & 0x000FFFFFU) |
           ((address >> 4U) & 0x00F00000U);
}

std::uint32_t width_mask(std::uint8_t width) {
    switch (width) {
    case 1:
        return 0xFFU;
    case 2:
        return 0xFFFFU;
    case 4:
        return 0xFFFFFFFFU;
    default:
        return 0;
    }
}

std::uint32_t width_bits(std::uint8_t width) {
    switch (width) {
    case 1:
        return 0x00000000U;
    case 2:
        return 0x02000000U;
    case 4:
        return 0x04000000U;
    default:
        return 0xFFFFFFFFU;
    }
}

Operation make_operation(OperationKind kind,
                         const RawLine& line,
                         std::uint32_t address,
                         std::uint32_t value,
                         std::uint8_t width,
                         std::string note) {
    Operation operation;
    operation.kind = kind;
    operation.address = address;
    operation.value = value;
    operation.width = width;
    operation.source_line = line.source_line;
    operation.source_text = line.source_text;
    operation.note = std::move(note);
    return operation;
}

bool is_condition_kind(OperationKind kind) {
    switch (kind) {
    case OperationKind::IfEqual:
    case OperationKind::IfNotEqual:
    case OperationKind::IfGreater:
    case OperationKind::IfLess:
    case OperationKind::IfGreaterOrEqual:
    case OperationKind::IfLessOrEqual:
    case OperationKind::IfAnd:
    case OperationKind::IfNand:
    case OperationKind::IfDeviceButton:
        return true;
    default:
        return false;
    }
}

OperationKind decode_condition_kind(std::uint32_t op1) {
    switch (op1 & kConditionMask) {
    case kCondEqual:
        return OperationKind::IfEqual;
    case kCondNotEqual:
        return OperationKind::IfNotEqual;
    case kCondLessSigned:
    case kCondLessUnsigned:
        return OperationKind::IfLess;
    case kCondGreaterSigned:
    case kCondGreaterUnsigned:
        return OperationKind::IfGreater;
    case kCondAnd:
        return OperationKind::IfAnd;
    default:
        return OperationKind::Unsupported;
    }
}

std::uint32_t condition_bits(OperationKind kind) {
    switch (kind) {
    case OperationKind::IfEqual:
        return kCondEqual;
    case OperationKind::IfNotEqual:
        return kCondNotEqual;
    case OperationKind::IfLess:
    case OperationKind::IfLessOrEqual:
        return kCondLessSigned;
    case OperationKind::IfGreater:
    case OperationKind::IfGreaterOrEqual:
        return kCondGreaterSigned;
    case OperationKind::IfAnd:
        return kCondAnd;
    default:
        return 0;
    }
}

std::string format_line(std::uint32_t op1,
                        std::uint32_t op2,
                        bool encrypted,
                        const crypto::TeaKey& key) {
    if (encrypted) {
        const auto converted = crypto::tea_encrypt(op1, op2, key);
        op1 = converted.first;
        op2 = converted.second;
    }
    return text::hex(op1, 8) + " " + text::hex(op2, 8);
}

void add_warning(CheatDocument& document,
                 const RawLine& line,
                 const std::string& message) {
    document.warnings.push_back(
        "Action Replay MAX: " + message + " at source line " +
        std::to_string(line.source_line));
}

void add_unsupported(CheatEntry& entry,
                     CheatDocument& document,
                     const RawLine& line,
                     std::uint32_t address,
                     std::uint32_t value,
                     std::uint8_t width,
                     const std::string& reason) {
    entry.operations.push_back(
        make_operation(OperationKind::Unsupported,
                       line, address, value, width, reason));
    add_warning(document, line, reason);
}

} // namespace gba::armax::detail
