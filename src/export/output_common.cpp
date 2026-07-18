#include "export/output_modes_internal.hpp"

#include "core/text.hpp"
#include "formats/armax.hpp"
#include "formats/codebreaker.hpp"
#include "formats/gameshark.hpp"

#include <algorithm>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {

std::string single_line_name(std::string_view raw) {
    std::string name;
    name.reserve(raw.size());
    bool pending_space = false;
    for (char ch : raw) {
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            pending_space = true;
            continue;
        }
        if (pending_space && !name.empty() && name.back() != ' ') {
            name += " - ";
        }
        pending_space = false;
        name.push_back(ch);
    }
    name = text::trim(name);
    return name.empty() ? "Unnamed Cheat" : name;
}

std::string xml_escape(std::string_view raw) {
    std::string out;
    out.reserve(raw.size() + 16U);
    for (char ch : raw) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        case '\r': break;
        case '\n': out += "&#10;"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string quote_escape(std::string_view raw) {
    std::string out;
    for (char ch : raw) {
        if (ch == '\\' || ch == '"') {
            out.push_back('\\');
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            out += " - ";
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void patch_u32(std::vector<std::uint8_t>& out,
               std::size_t offset,
               std::uint32_t value) {
    if (offset + 4U > out.size()) {
        return;
    }
    for (unsigned index = 0; index < 4U; ++index) {
        out[offset + index] = static_cast<std::uint8_t>(
            (value >> (index * 8U)) & 0xFFU);
    }
}

void append_bytes(std::vector<std::uint8_t>& out,
                  std::string_view value) {
    for (char ch : value) {
        out.push_back(static_cast<std::uint8_t>(
            static_cast<unsigned char>(ch)));
    }
}

std::vector<std::uint8_t> bytes_from_text(std::string_view value) {
    std::vector<std::uint8_t> out;
    out.reserve(value.size());
    append_bytes(out, value);
    return out;
}

CheatDocument one_entry_document(const CheatEntry& entry) {
    CheatDocument document;
    document.entries.push_back(entry);
    return document;
}

std::vector<std::string> code_lines(std::string_view output,
                                    bool want8x8) {
    std::vector<std::string> lines;
    for (const std::string& raw : text::split_lines(output)) {
        const std::string line = text::trim(raw);
        if ((want8x8 && text::is_code_line_8x8(line)) ||
            (!want8x8 && text::is_code_line_8x4(line))) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::optional<std::vector<std::pair<std::uint32_t, std::uint16_t>>>
exact_fcd(const CheatEntry& entry) {
    const auto converted = codebreaker::export_document(
        one_entry_document(entry), {false, std::nullopt});
    if (!converted.success || !converted.warnings.empty()) {
        return std::nullopt;
    }
    const auto lines = code_lines(converted.text, false);
    if (lines.empty()) {
        return std::nullopt;
    }
    std::vector<std::pair<std::uint32_t, std::uint16_t>> result;
    for (const std::string& line : lines) {
        const auto first = text::parse_hex_u32(line.substr(0U, 8U));
        const auto second = text::parse_hex_u16(line.substr(9U, 4U));
        if (!first || !second) {
            return std::nullopt;
        }
        result.emplace_back(*first, *second);
    }
    return result;
}

std::optional<std::vector<std::pair<std::uint32_t, std::uint32_t>>>
exact_gameshark(const CheatEntry& entry, bool encrypted) {
    const auto converted = gameshark::export_document(
        one_entry_document(entry), {encrypted});
    if (!converted.success || !converted.warnings.empty()) {
        return std::nullopt;
    }
    const auto lines = code_lines(converted.text, true);
    if (lines.empty()) {
        return std::nullopt;
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> result;
    for (const std::string& line : lines) {
        const auto first = text::parse_hex_u32(line.substr(0U, 8U));
        const auto second = text::parse_hex_u32(line.substr(9U, 8U));
        if (!first || !second) return std::nullopt;
        result.emplace_back(*first, *second);
    }
    return result;
}

std::optional<std::vector<std::pair<std::uint32_t, std::uint32_t>>>
exact_armax(const CheatEntry& entry, bool encrypted) {
    const auto converted = armax::export_document(
        one_entry_document(entry), {encrypted});
    if (!converted.success || !converted.warnings.empty()) {
        return std::nullopt;
    }
    const auto lines = code_lines(converted.text, true);
    if (lines.empty()) {
        return std::nullopt;
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> result;
    for (const std::string& line : lines) {
        const auto first = text::parse_hex_u32(line.substr(0U, 8U));
        const auto second = text::parse_hex_u32(line.substr(9U, 8U));
        if (!first || !second) {
            return std::nullopt;
        }
        result.emplace_back(*first, *second);
    }
    return result;
}

std::string format_8x4(
    const std::vector<std::pair<std::uint32_t, std::uint16_t>>& lines) {
    std::ostringstream out;
    for (const auto& line : lines) {
        out << text::hex(line.first, 8U) << ' '
            << text::hex(line.second, 4U) << '\n';
    }
    return out.str();
}

std::string format_8x8(
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& lines) {
    std::ostringstream out;
    for (const auto& line : lines) {
        out << text::hex(line.first, 8U) << ' '
            << text::hex(line.second, 8U) << '\n';
    }
    return out.str();
}

void warn_omitted(Result& result,
                  const CheatEntry& entry,
                  std::string_view target) {
    result.warnings.push_back(
        "Omitted '" + single_line_name(entry.name) +
        "': it cannot be represented safely in " + std::string(target) + ".");
}

bool direct_writes_only(const CheatEntry& entry) {
    if (entry.operations.empty()) {
        return false;
    }
    return std::all_of(entry.operations.begin(), entry.operations.end(),
        [](const Operation& operation) {
            return operation.kind == OperationKind::Write &&
                   (operation.width == 1U || operation.width == 2U ||
                    operation.width == 4U);
        });
}

std::string lower_hex(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << value;
    return out.str();
}

} // namespace gba::output_modes::detail
