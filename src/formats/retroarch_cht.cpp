#include "formats/retroarch_cht.hpp"

#include "core/text.hpp"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace gba::retroarch_cht {
namespace {

constexpr std::size_t kMaximumRecords = 1000000U;

struct Assignment {
    std::string key;
    std::string value;
};

std::string unescape_quoted(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    bool escaped = false;
    for (char ch : value) {
        if (escaped) {
            switch (ch) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            default: out.push_back(ch); break;
            }
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

std::string escape_quoted(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 8U);
    for (char ch : value) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

std::optional<Assignment> parse_assignment(std::string_view raw) {
    std::string line = text::trim(raw);
    if (line.empty() || line.front() == '#') return std::nullopt;

    const std::size_t equals = line.find('=');
    if (equals == std::string::npos) return std::nullopt;
    Assignment assignment;
    assignment.key = text::trim(std::string_view(line).substr(0U, equals));
    std::string value = text::trim(
        std::string_view(line).substr(equals + 1U));
    if (assignment.key.empty()) return std::nullopt;

    if (!value.empty() && value.front() == '"') {
        bool escaped = false;
        std::size_t close = std::string::npos;
        for (std::size_t index = 1U; index < value.size(); ++index) {
            const char ch = value[index];
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                close = index;
                break;
            }
        }
        if (close == std::string::npos) return std::nullopt;
        const std::string tail = text::trim(
            std::string_view(value).substr(close + 1U));
        if (!tail.empty() && tail.front() != '#') return std::nullopt;
        assignment.value = unescape_quoted(
            std::string_view(value).substr(1U, close - 1U));
    } else {
        const std::size_t comment = value.find('#');
        if (comment != std::string::npos) value.resize(comment);
        assignment.value = text::trim(value);
    }
    return assignment;
}

std::optional<std::uint32_t> parse_u32(std::string_view raw) {
    const std::string value = text::trim(raw);
    if (value.empty() || value.front() == '-') return std::nullopt;
    errno = 0;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value.c_str(), &end, 0);
    if (errno != 0 || end == value.c_str() || *end != '\0' ||
        parsed > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(parsed);
}

bool parse_bool(std::string_view raw) {
    return raw == "true" || raw == "1";
}

std::optional<std::pair<std::size_t, std::string>> indexed_key(
    std::string_view raw) {
    if (raw.rfind("cheat", 0U) != 0U || raw.size() <= 5U) {
        return std::nullopt;
    }
    std::size_t cursor = 5U;
    if (std::isdigit(static_cast<unsigned char>(raw[cursor])) == 0) {
        return std::nullopt;
    }
    std::size_t index = 0U;
    while (cursor < raw.size() &&
           std::isdigit(static_cast<unsigned char>(raw[cursor])) != 0) {
        const std::size_t digit = static_cast<std::size_t>(raw[cursor] - '0');
        if (index > (kMaximumRecords - digit) / 10U) return std::nullopt;
        index = index * 10U + digit;
        ++cursor;
    }
    if (cursor >= raw.size() || raw[cursor] != '_') return std::nullopt;
    return std::make_pair(index, std::string(raw.substr(cursor + 1U)));
}

bool recognized_suffix(std::string_view suffix) {
    static constexpr std::string_view suffixes[] = {
        "desc", "code", "enable", "big_endian", "handler",
        "memory_search_size", "cheat_type", "value", "address",
        "address_bit_position", "rumble_type", "rumble_value",
        "rumble_port", "rumble_primary_strength",
        "rumble_primary_duration", "rumble_secondary_strength",
        "rumble_secondary_duration", "repeat_count",
        "repeat_add_to_value", "repeat_add_to_address"
    };
    for (std::string_view candidate : suffixes) {
        if (suffix == candidate) return true;
    }
    return false;
}

bool assign_numeric(Record& record, std::string_view suffix,
                    std::uint32_t value) {
    if (suffix == "handler") record.handler = value;
    else if (suffix == "memory_search_size") record.memory_search_size = value;
    else if (suffix == "cheat_type") record.cheat_type = value;
    else if (suffix == "value") record.value = value;
    else if (suffix == "address") record.address = value;
    else if (suffix == "address_bit_position") record.address_mask = value;
    else if (suffix == "rumble_type") record.rumble_type = value;
    else if (suffix == "rumble_value") record.rumble_value = value;
    else if (suffix == "rumble_port") record.rumble_port = value;
    else if (suffix == "rumble_primary_strength") {
        record.rumble_primary_strength = value;
    } else if (suffix == "rumble_primary_duration") {
        record.rumble_primary_duration = value;
    } else if (suffix == "rumble_secondary_strength") {
        record.rumble_secondary_strength = value;
    } else if (suffix == "rumble_secondary_duration") {
        record.rumble_secondary_duration = value;
    } else if (suffix == "repeat_count") record.repeat_count = value;
    else if (suffix == "repeat_add_to_value") {
        record.repeat_add_to_value = value;
    } else if (suffix == "repeat_add_to_address") {
        record.repeat_add_to_address = value;
    } else {
        return false;
    }
    return true;
}

void write_string(std::ostringstream& out, std::size_t index,
                  std::string_view suffix, std::string_view value) {
    out << "cheat" << index << '_' << suffix << " = \""
        << escape_quoted(value) << "\"\n";
}

void write_uint(std::ostringstream& out, std::size_t index,
                std::string_view suffix, std::uint32_t value) {
    out << "cheat" << index << '_' << suffix << " = " << value << '\n';
}

} // namespace

ParseResult parse(std::string_view raw_text) {
    ParseResult result;
    const std::string normalized = text::normalize_newlines_lf(
        text::strip_utf8_bom(raw_text));

    std::vector<Assignment> assignments;
    assignments.reserve(32U);
    bool has_count_key = false;
    bool has_indexed_key = false;
    std::optional<std::uint32_t> declared_count;

    for (const std::string& raw : text::split_lines(normalized)) {
        const auto assignment = parse_assignment(raw);
        if (!assignment) continue;
        assignments.push_back(*assignment);
        if (assignment->key == "cheats") {
            has_count_key = true;
            declared_count = parse_u32(assignment->value);
        } else if (const auto indexed = indexed_key(assignment->key)) {
            if (recognized_suffix(indexed->second)) has_indexed_key = true;
        }
    }

    result.recognized = has_count_key && has_indexed_key;
    if (!result.recognized) return result;
    if (!declared_count || *declared_count == 0U) {
        result.warnings.push_back(
            "RetroArch cheat count is missing, invalid, or zero.");
        return result;
    }
    if (*declared_count > kMaximumRecords) {
        result.warnings.push_back(
            "RetroArch cheat count is too large to import safely.");
        return result;
    }

    result.records.resize(static_cast<std::size_t>(*declared_count));
    for (const Assignment& assignment : assignments) {
        const auto indexed = indexed_key(assignment.key);
        if (!indexed || indexed->first >= result.records.size() ||
            !recognized_suffix(indexed->second)) {
            continue;
        }
        Record& record = result.records[indexed->first];
        const std::string& suffix = indexed->second;
        if (suffix == "desc") record.desc = assignment.value;
        else if (suffix == "code") record.code = assignment.value;
        else if (suffix == "enable") record.enabled = parse_bool(assignment.value);
        else if (suffix == "big_endian") {
            record.big_endian = parse_bool(assignment.value);
        } else {
            const auto value = parse_u32(assignment.value);
            if (!value) {
                result.warnings.push_back(
                    "RetroArch field '" + assignment.key +
                    "' has an invalid unsigned value; RetroArch would read it as zero.");
                assign_numeric(record, suffix, 0U);
            } else {
                assign_numeric(record, suffix, *value);
            }
        }
    }

    result.success = true;
    return result;
}

std::string serialize(const std::vector<Record>& records) {
    std::ostringstream out;
    out << "cheats = " << records.size() << "\n\n";
    for (std::size_t index = 0U; index < records.size(); ++index) {
        const Record& record = records[index];
        write_string(out, index, "desc",
                     record.desc.empty() ? record.code : record.desc);
        write_string(out, index, "code", record.code);
        out << "cheat" << index << "_enable = "
            << (record.enabled ? "true" : "false") << '\n';
        out << "cheat" << index << "_big_endian = "
            << (record.big_endian ? "true" : "false") << '\n';
        write_uint(out, index, "handler", record.handler);
        write_uint(out, index, "memory_search_size", record.memory_search_size);
        write_uint(out, index, "cheat_type", record.cheat_type);
        write_uint(out, index, "value", record.value);
        write_uint(out, index, "address", record.address);
        write_uint(out, index, "address_bit_position", record.address_mask);
        write_uint(out, index, "rumble_type", record.rumble_type);
        write_uint(out, index, "rumble_value", record.rumble_value);
        write_uint(out, index, "rumble_port", record.rumble_port);
        write_uint(out, index, "rumble_primary_strength",
                   record.rumble_primary_strength);
        write_uint(out, index, "rumble_primary_duration",
                   record.rumble_primary_duration);
        write_uint(out, index, "rumble_secondary_strength",
                   record.rumble_secondary_strength);
        write_uint(out, index, "rumble_secondary_duration",
                   record.rumble_secondary_duration);
        write_uint(out, index, "repeat_count", record.repeat_count);
        write_uint(out, index, "repeat_add_to_value",
                   record.repeat_add_to_value);
        write_uint(out, index, "repeat_add_to_address",
                   record.repeat_add_to_address);
        out << '\n';
    }
    return out.str();
}

} // namespace gba::retroarch_cht
