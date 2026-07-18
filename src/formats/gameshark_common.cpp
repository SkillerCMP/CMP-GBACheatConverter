#include "formats/gameshark_internal.hpp"

#include "core/text.hpp"

#include <utility>

namespace gba::gameshark::detail {

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

void set_assignment_hint(Operation& operation,
                         std::uint32_t group,
                         std::uint32_t index,
                         std::uint32_t count) {
    operation.encoding_hint = EncodingHint::GameSharkAssignmentList;
    operation.encoding_group = group;
    operation.encoding_index = index;
    operation.encoding_count = count;
}

RawLine decode_line(RawLine line,
                    bool encrypted,
                    const crypto::TeaKey& key) {
    if (!encrypted) {
        return line;
    }

    const auto raw = crypto::tea_decrypt(line.op1, line.op2, key);
    line.op1 = raw.first;
    line.op2 = raw.second;
    return line;
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
    case OperationKind::IfXor:
    case OperationKind::IfNotXor:
    case OperationKind::IfOr:
    case OperationKind::IfNotOr:
    case OperationKind::IfDeviceButton:
        return true;
    default:
        return false;
    }
}

std::string format_line(const RawLine& line,
                        bool encrypted,
                        const crypto::TeaKey& key) {
    std::uint32_t op1 = line.op1;
    std::uint32_t op2 = line.op2;
    if (encrypted) {
        const auto converted = crypto::tea_encrypt(op1, op2, key);
        op1 = converted.first;
        op2 = converted.second;
    }

    return text::hex(op1, 8) + " " + text::hex(op2, 8);
}

} // namespace gba::gameshark::detail
