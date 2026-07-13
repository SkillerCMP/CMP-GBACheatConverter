#include "import/native_input_internal.hpp"

#include "core/text.hpp"
#include "formats/armax.hpp"
#include "formats/codebreaker.hpp"
#include "formats/ezflash.hpp"
#include "formats/gameshark.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::native_input::detail {
namespace {

std::string clean_name(std::string_view raw, std::string_view fallback) {
    std::string name = text::trim(raw);
    if (name.empty()) name = std::string(fallback);
    for (char& ch : name) {
        if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
    }
    return name;
}

std::optional<std::string> exact_render(
    const CheatDocument& document, InputFormat format) {
    if (format == InputFormat::FcdRaw) {
        const auto converted = codebreaker::export_document(
            document, {false, std::nullopt});
        if (!converted.success || !converted.warnings.empty() ||
            converted.text.empty()) {
            return std::nullopt;
        }
        const CheatDocument reparsed = codebreaker::parse(
            converted.text, {false});
        if (!reparsed.warnings.empty() ||
            reparsed.entries.size() != document.entries.size()) {
            return std::nullopt;
        }
        return converted.text;
    }
    if (format == InputFormat::ActionReplayMaxRaw) {
        const auto converted = armax::export_document(document, {false});
        if (!converted.success || !converted.warnings.empty() ||
            converted.text.empty()) {
            return std::nullopt;
        }
        const CheatDocument reparsed = armax::parse(
            converted.text, {false});
        if (!reparsed.warnings.empty() ||
            reparsed.entries.size() != document.entries.size()) {
            return std::nullopt;
        }
        return converted.text;
    }
    if (format == InputFormat::EzFlash) {
        ezflash::Options options;
        options.mode = ezflash::Mode::Enhanced;
        const auto converted = ezflash::export_document(document, options);
        if (!converted.success || !converted.warnings.empty() ||
            converted.text.empty()) {
            return std::nullopt;
        }
        const CheatDocument reparsed = ezflash::parse(converted.text);
        if (!reparsed.warnings.empty() ||
            reparsed.entries.size() != document.entries.size()) {
            return std::nullopt;
        }
        return converted.text;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> parse_numeric_entity(std::string_view entity) {
    if (entity.size() < 2U || entity.front() != '#') return std::nullopt;
    unsigned base = 10U;
    std::size_t start = 1U;
    if (start < entity.size() &&
        (entity[start] == 'x' || entity[start] == 'X')) {
        base = 16U;
        ++start;
    }
    if (start >= entity.size()) return std::nullopt;
    std::uint32_t value = 0U;
    for (std::size_t index = start; index < entity.size(); ++index) {
        const unsigned char ch = static_cast<unsigned char>(entity[index]);
        unsigned digit = 0U;
        if (ch >= '0' && ch <= '9') digit = ch - '0';
        else if (base == 16U && ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10U;
        else if (base == 16U && ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10U;
        else return std::nullopt;
        if (digit >= base || value > (0x10FFFFU - digit) / base) {
            return std::nullopt;
        }
        value = value * base + digit;
    }
    return value;
}

void append_utf8(std::string& out, std::uint32_t value) {
    if (value <= 0x7FU) {
        out.push_back(static_cast<char>(value));
    } else if (value <= 0x7FFU) {
        out.push_back(static_cast<char>(0xC0U | (value >> 6U)));
        out.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else if (value <= 0xFFFFU) {
        out.push_back(static_cast<char>(0xE0U | (value >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else if (value <= 0x10FFFFU) {
        out.push_back(static_cast<char>(0xF0U | (value >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((value >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    }
}

} // namespace

std::optional<std::uint16_t> read_u16(
    const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2U) {
        return std::nullopt;
    }
    const auto low = static_cast<std::uint16_t>(data[offset]);
    const auto high = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[offset + 1U]) << 8U);
    return static_cast<std::uint16_t>(low | high);
}

std::optional<std::uint32_t> read_u32(
    const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4U) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

std::string fixed_text(const std::vector<std::uint8_t>& data,
                       std::size_t offset,
                       std::size_t size,
                       bool trim_spaces) {
    if (offset > data.size()) return {};
    const std::size_t count = std::min(size, data.size() - offset);
    std::string value;
    value.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const std::uint8_t byte = data[offset + index];
        if (byte == 0U) break;
        value.push_back(byte >= 0x20U && byte < 0x7FU
            ? static_cast<char>(byte) : '?');
    }
    if (trim_spaces) {
        while (!value.empty() &&
               std::isspace(static_cast<unsigned char>(value.back())) != 0) {
            value.pop_back();
        }
    }
    return value;
}

std::string filename_extension(std::string_view filename) {
    const std::size_t slash = filename.find_last_of("/\\");
    const std::size_t dot = filename.find_last_of('.');
    if (dot == std::string_view::npos ||
        (slash != std::string_view::npos && dot < slash)) {
        return {};
    }
    std::string extension(filename.substr(dot));
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension;
}


std::string bytes_as_text(const std::vector<std::uint8_t>& data) {
    return std::string(data.begin(), data.end());
}

bool has_nul_byte(const std::vector<std::uint8_t>& data) {
    return std::find(data.begin(), data.end(), std::uint8_t{0}) != data.end();
}

bool equals_ascii(const std::vector<std::uint8_t>& data,
                  std::size_t offset,
                  std::string_view value) {
    if (offset > data.size() || data.size() - offset < value.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < value.size(); ++index) {
        if (data[offset + index] != static_cast<std::uint8_t>(
                static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

std::string xml_unescape(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t index = 0U; index < value.size();) {
        if (value[index] != '&') {
            out.push_back(value[index++]);
            continue;
        }
        const std::size_t semicolon = value.find(';', index + 1U);
        if (semicolon == std::string_view::npos) {
            out.push_back(value[index++]);
            continue;
        }
        const std::string_view entity = value.substr(
            index + 1U, semicolon - index - 1U);
        if (entity == "amp") out.push_back('&');
        else if (entity == "lt") out.push_back('<');
        else if (entity == "gt") out.push_back('>');
        else if (entity == "quot") out.push_back('"');
        else if (entity == "apos") out.push_back('\'');
        else if (const auto numeric = parse_numeric_entity(entity)) {
            append_utf8(out, *numeric);
        } else {
            out.append(value.substr(index, semicolon - index + 1U));
        }
        index = semicolon + 1U;
    }
    return out;
}

std::string quoted_unescape(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    bool escaped = false;
    for (char ch : value) {
        if (escaped) {
            out.push_back(ch);
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else {
            out.push_back(ch);
        }
    }
    if (escaped) out.push_back('\\');
    return out;
}

std::string normalized_entry_text(const NativeEntry& entry) {
    std::ostringstream out;
    out << text::format_cheat_header(
        clean_name(entry.name, "Imported Cheat")) << '\n';
    for (const std::string& line : entry.code_lines) {
        out << text::trim(line) << '\n';
    }
    return out.str();
}

CheatDocument parse_entry(const NativeEntry& entry) {
    const std::string input = normalized_entry_text(entry);
    switch (entry.format) {
    case InputFormat::FcdRaw:
        return codebreaker::parse(input, {false});
    case InputFormat::FcdEncrypted:
        return codebreaker::parse(input, {true});
    case InputFormat::GameSharkRaw:
        return gameshark::parse(input, {false});
    case InputFormat::GameSharkEncrypted:
        return gameshark::parse(input, {true});
    case InputFormat::ActionReplayMaxRaw:
        return armax::parse(input, {false});
    case InputFormat::ActionReplayMaxEncrypted:
        return armax::parse(input, {true});
    case InputFormat::EzFlash:
        return ezflash::parse(input);
    }
    return {};
}


Result finish_entries(SourceFormat source_format,
                      std::string source_name,
                      const std::vector<NativeEntry>& entries,
                      const std::vector<InputFormat>& mixed_preferences) {
    if (entries.empty()) {
        return recognized_error(source_format, std::move(source_name),
                                "The native file contains no cheat entries.");
    }

    const InputFormat first_format = entries.front().format;
    const bool homogeneous = std::all_of(
        entries.begin(), entries.end(),
        [first_format](const NativeEntry& entry) {
            return entry.format == first_format;
        });
    if (homogeneous) {
        Result result;
        result.recognized = true;
        result.success = true;
        result.source_format = source_format;
        result.input_format = first_format;
        result.source_name = std::move(source_name);
        std::ostringstream out;
        for (std::size_t index = 0U; index < entries.size(); ++index) {
            if (index != 0U) out << '\n';
            out << normalized_entry_text(entries[index]);
        }
        result.text = out.str();
        return result;
    }

    CheatDocument combined;
    for (const NativeEntry& entry : entries) {
        CheatDocument parsed = parse_entry(entry);
        if (!parsed.warnings.empty() || parsed.entries.size() != 1U ||
            parsed.entries.front().operations.empty()) {
            return recognized_error(
                source_format, std::move(source_name),
                "A mixed-format native cheat could not be parsed safely.");
        }
        combined.entries.push_back(std::move(parsed.entries.front()));
    }
    return render_document(source_format, std::move(source_name),
                           combined, mixed_preferences);
}

Result render_document(SourceFormat source_format,
                       std::string source_name,
                       const CheatDocument& document,
                       const std::vector<InputFormat>& preferences) {
    if (document.entries.empty()) {
        return recognized_error(source_format, std::move(source_name),
                                "The native file contains no cheat entries.");
    }
    for (InputFormat format : preferences) {
        if (const auto rendered = exact_render(document, format)) {
            Result result;
            result.recognized = true;
            result.success = true;
            result.source_format = source_format;
            result.input_format = format;
            result.source_name = std::move(source_name);
            result.text = *rendered;
            return result;
        }
    }
    return recognized_error(
        source_format, std::move(source_name),
        "The native file cannot be converted to one Input format without "
        "changing cheat behavior.");
}

Result recognized_error(SourceFormat source_format,
                        std::string source_name,
                        std::string warning) {
    Result result;
    result.recognized = true;
    result.success = false;
    result.source_format = source_format;
    result.source_name = std::move(source_name);
    result.warnings.push_back(std::move(warning));
    return result;
}

} // namespace gba::native_input::detail
