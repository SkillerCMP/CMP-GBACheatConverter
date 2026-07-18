#include "formats/mednafen_cht.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::mednafen_cht {
namespace {

bool is_hex_string(std::string_view value, std::size_t count) {
    return value.size() == count &&
        std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isxdigit(ch) != 0;
        });
}

std::optional<std::uint64_t> parse_unsigned(std::string_view value,
                                            int base) {
    if (value.empty()) return std::nullopt;
    std::string copy(value);
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(copy.c_str(), &end, base);
    if (errno != 0 || end == copy.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(parsed);
}

std::optional<std::uint64_t> parse_condition_number(
    std::string_view value) {
    if (value.size() >= 2U && value[0] == '0' &&
        (value[1] == 'x' || value[1] == 'X')) {
        return parse_unsigned(value.substr(2U), 16);
    }
    return parse_unsigned(value, 0);
}

std::optional<std::uint32_t> parse_u32_hex(std::string_view value) {
    const auto parsed = parse_unsigned(value, 16);
    if (!parsed || *parsed > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*parsed);
}

std::optional<std::uint32_t> parse_u32_decimal(std::string_view value) {
    const auto parsed = parse_unsigned(value, 10);
    if (!parsed || *parsed > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*parsed);
}

std::optional<std::uint32_t> parse_condition_address(
    std::string_view value) {
    const auto parsed = parse_condition_number(value);
    if (!parsed || *parsed > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*parsed);
}

std::optional<ConditionOperator> parse_operator(std::string_view value) {
    if (value == ">=") return ConditionOperator::GreaterOrEqual;
    if (value == "<=") return ConditionOperator::LessOrEqual;
    if (value == ">") return ConditionOperator::Greater;
    if (value == "<") return ConditionOperator::Less;
    if (value == "==") return ConditionOperator::Equal;
    if (value == "!=") return ConditionOperator::NotEqual;
    if (value == "&") return ConditionOperator::AndNonzero;
    if (value == "!&") return ConditionOperator::AndZero;
    if (value == "^") return ConditionOperator::XorNonzero;
    if (value == "!^") return ConditionOperator::XorZero;
    if (value == "|") return ConditionOperator::OrNonzero;
    if (value == "!|") return ConditionOperator::OrZero;
    return std::nullopt;
}

std::string operator_text(ConditionOperator operation) {
    switch (operation) {
    case ConditionOperator::GreaterOrEqual: return ">=";
    case ConditionOperator::LessOrEqual: return "<=";
    case ConditionOperator::Greater: return ">";
    case ConditionOperator::Less: return "<";
    case ConditionOperator::Equal: return "==";
    case ConditionOperator::NotEqual: return "!=";
    case ConditionOperator::AndNonzero: return "&";
    case ConditionOperator::AndZero: return "!&";
    case ConditionOperator::XorNonzero: return "^";
    case ConditionOperator::XorZero: return "!^";
    case ConditionOperator::OrNonzero: return "|";
    case ConditionOperator::OrZero: return "!|";
    }
    return "==";
}

bool parse_conditions(std::string_view raw,
                      std::vector<Condition>& conditions,
                      std::string& error) {
    const std::string trimmed = text::trim(raw);
    if (trimmed.empty()) return true;

    std::size_t start = 0U;
    while (start <= trimmed.size()) {
        const std::size_t comma = trimmed.find(',', start);
        const std::string part = text::trim(std::string_view(trimmed).substr(
            start, comma == std::string::npos
                ? std::string::npos
                : comma - start));
        if (part.empty()) {
            error = "Mednafen condition list contains an empty condition.";
            return false;
        }

        std::istringstream input(part);
        std::string length_text;
        std::string endian_text;
        std::string address_text;
        std::string operator_value;
        std::string value_text;
        std::string extra;
        if (!(input >> length_text >> endian_text >> address_text >>
              operator_value >> value_text) || (input >> extra)) {
            error = "Mednafen condition has an invalid field layout.";
            return false;
        }
        const auto length = parse_u32_decimal(length_text);
        const auto address = parse_condition_address(address_text);
        const auto value = parse_condition_number(value_text);
        const auto operation = parse_operator(operator_value);
        if (!length || *length < 1U || *length > 8U ||
            (endian_text != "L" && endian_text != "B") || !address ||
            !value || !operation) {
            error = "Mednafen condition contains an invalid length, endian, "
                    "address, operator, or value.";
            return false;
        }
        Condition condition;
        condition.length = static_cast<std::uint8_t>(*length);
        condition.big_endian = endian_text == "B";
        condition.address = *address;
        condition.operation = *operation;
        condition.value = *value;
        conditions.push_back(condition);

        if (comma == std::string::npos) break;
        start = comma + 1U;
    }
    return true;
}

bool parse_header(std::string_view line,
                  std::string& md5,
                  std::string& game_name) {
    const std::string trimmed = text::trim(line);
    if (trimmed.size() < 34U || trimmed.front() != '[') return false;
    const std::size_t close = trimmed.find(']');
    if (close != 33U || !is_hex_string(
            std::string_view(trimmed).substr(1U, 32U), 32U)) {
        return false;
    }
    md5 = std::string(std::string_view(trimmed).substr(1U, 32U));
    game_name = text::trim(std::string_view(trimmed).substr(close + 1U));
    return true;
}

bool valid_type(char type) {
    return type == 'R' || type == 'A' || type == 'T' ||
           type == 'S' || type == 'C';
}

bool parse_record_line(std::string_view raw,
                       const std::string& md5,
                       const std::string& game_name,
                       Record& record,
                       std::string& error) {
    std::string line = text::trim(raw);
    bool extended = false;
    if (!line.empty() && line.front() == '!') {
        extended = true;
        line = text::trim(std::string_view(line).substr(1U));
    }
    if (line.empty()) {
        error = "Mednafen cheat record is empty.";
        return false;
    }

    std::istringstream input(line);
    std::string type_text;
    std::string status_text;
    std::string length_text;
    std::string endian_text;
    std::string instance_text;
    std::string address_text;
    std::string value_text;
    if (!(input >> type_text >> status_text >> length_text >> endian_text >>
          instance_text >> address_text >> value_text)) {
        error = "Mednafen cheat record has too few fields.";
        return false;
    }
    if (type_text.size() != 1U || !valid_type(type_text.front()) ||
        (status_text != "A" && status_text != "I") ||
        (endian_text != "L" && endian_text != "B")) {
        error = "Mednafen cheat record has an invalid type, status, or endian.";
        return false;
    }
    const auto length = parse_u32_decimal(length_text);
    const auto instance_count = parse_u32_decimal(instance_text);
    const auto address = parse_u32_hex(address_text);
    const auto value = parse_unsigned(value_text, 16);
    if (!length || *length < 1U || *length > 8U || !instance_count ||
        !address || !value) {
        error = "Mednafen cheat record has an invalid numeric field.";
        return false;
    }

    record.rom_md5 = md5;
    record.game_name = game_name;
    record.type = type_text.front();
    record.enabled = status_text == "A";
    record.length = static_cast<std::uint8_t>(*length);
    record.big_endian = endian_text == "B";
    record.instance_count = *instance_count;
    record.address = *address;
    record.value = *value;

    if (record.type == 'C') {
        std::string compare_text;
        if (!(input >> compare_text)) {
            error = "Mednafen compare-substitute record is missing compare data.";
            return false;
        }
        const auto compare = parse_unsigned(compare_text, 16);
        if (!compare) {
            error = "Mednafen compare-substitute record has an invalid compare value.";
            return false;
        }
        record.compare = *compare;
    } else if (extended) {
        std::string repeat_text;
        std::string address_increment_text;
        std::string value_increment_text;
        std::string copy_source_text;
        std::string copy_increment_text;
        if (!(input >> repeat_text >> address_increment_text >>
              value_increment_text >> copy_source_text >>
              copy_increment_text)) {
            error = "Mednafen extended record is missing extension fields.";
            return false;
        }
        const auto repeat = parse_u32_hex(repeat_text);
        const auto address_increment = parse_u32_hex(address_increment_text);
        const auto value_increment = parse_unsigned(value_increment_text, 16);
        const auto copy_source = parse_u32_hex(copy_source_text);
        const auto copy_increment = parse_u32_hex(copy_increment_text);
        if (!repeat || !address_increment || !value_increment ||
            !copy_source || !copy_increment) {
            error = "Mednafen extended record has an invalid extension field.";
            return false;
        }
        record.repeat_count = *repeat;
        record.repeat_address_increment = *address_increment;
        record.repeat_value_increment = *value_increment;
        record.copy_source_address = *copy_source;
        record.copy_source_increment = *copy_increment;
    }

    std::string name;
    std::getline(input, name);
    record.name = text::trim(name);
    if (record.name.empty()) record.name = "Imported Cheat";
    return true;
}

std::string hex_value(std::uint64_t value, unsigned width) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0')
        << std::setw(static_cast<int>(width)) << value;
    return out.str();
}

unsigned normal_value_width(std::uint8_t length) {
    if (length == 1U) return 2U;
    if (length == 2U) return 4U;
    return 16U;
}

bool needs_extended(const Record& record) {
    return record.type != 'C' &&
        (record.repeat_count != 1U ||
         record.repeat_address_increment != 0U ||
         record.repeat_value_increment != 0U ||
         record.copy_source_address != 0U ||
         record.copy_source_increment != 0U);
}

} // namespace

ParseResult parse(std::string_view value) {
    ParseResult result;
    const std::string normalized = text::normalize_newlines_lf(
        text::strip_utf8_bom(value));
    const std::vector<std::string> lines = text::split_lines(normalized);

    std::string current_md5;
    std::string current_game_name;
    bool saw_header = false;
    for (std::size_t index = 0U; index < lines.size();) {
        const std::string line = text::trim(lines[index]);
        if (line.empty()) {
            ++index;
            continue;
        }

        std::string md5;
        std::string game_name;
        if (parse_header(line, md5, game_name)) {
            saw_header = true;
            result.recognized = true;
            current_md5 = std::move(md5);
            current_game_name = std::move(game_name);
            ++index;
            continue;
        }

        if (!saw_header || current_md5.empty()) {
            ++index;
            continue;
        }

        Record record;
        std::string error;
        if (!parse_record_line(line, current_md5, current_game_name,
                               record, error)) {
            result.success = false;
            result.warnings.push_back(
                error + " (line " + std::to_string(index + 1U) + ")");
            return result;
        }
        if (index + 1U >= lines.size()) {
            result.success = false;
            result.warnings.push_back(
                "Mednafen cheat record is missing its conditions line "
                "(line " + std::to_string(index + 1U) + ").");
            return result;
        }
        record.conditions_text = text::trim(lines[index + 1U]);
        if (!parse_conditions(record.conditions_text, record.conditions,
                              error)) {
            result.success = false;
            result.warnings.push_back(
                error + " (line " + std::to_string(index + 2U) + ")");
            return result;
        }
        result.records.push_back(std::move(record));
        index += 2U;
    }

    result.success = result.recognized && !result.records.empty();
    if (result.recognized && result.records.empty()) {
        result.warnings.push_back(
            "Mednafen cheat file contains no valid cheat records.");
    }
    return result;
}

bool looks_like(std::string_view value) {
    const ParseResult parsed = parse(value);
    return parsed.recognized;
}

std::string serialize_conditions(const std::vector<Condition>& conditions) {
    std::ostringstream out;
    for (std::size_t index = 0U; index < conditions.size(); ++index) {
        if (index != 0U) out << ", ";
        const Condition& condition = conditions[index];
        out << static_cast<unsigned>(condition.length) << ' '
            << (condition.big_endian ? 'B' : 'L') << " 0x"
            << hex_value(condition.address, 8U) << ' '
            << operator_text(condition.operation) << " 0x"
            << hex_value(condition.value,
                         normal_value_width(condition.length));
    }
    return out.str();
}

std::string serialize(const std::vector<Record>& records) {
    std::ostringstream out;
    std::string current_md5;
    std::string current_game_name;
    for (const Record& record : records) {
        if (record.rom_md5 != current_md5 ||
            record.game_name != current_game_name) {
            current_md5 = record.rom_md5;
            current_game_name = record.game_name;
            out << '[' << current_md5 << "] " << current_game_name << '\n';
        }

        const bool extended = needs_extended(record);
        if (extended) out << '!';
        out << record.type << ' ' << (record.enabled ? 'A' : 'I') << ' '
            << static_cast<unsigned>(record.length) << ' '
            << (record.big_endian ? 'B' : 'L') << ' '
            << record.instance_count << ' '
            << hex_value(record.address, 8U) << ' '
            << hex_value(record.value,
                         extended ? 16U : normal_value_width(record.length));

        if (record.type == 'C') {
            out << ' ' << hex_value(record.compare,
                                    normal_value_width(record.length));
        } else if (extended) {
            out << ' ' << hex_value(record.repeat_count, 8U)
                << ' ' << hex_value(record.repeat_address_increment, 8U)
                << ' ' << hex_value(record.repeat_value_increment, 16U)
                << ' ' << hex_value(record.copy_source_address, 8U)
                << ' ' << hex_value(record.copy_source_increment, 8U);
        }
        out << ' ' << (record.name.empty() ? "Unnamed Cheat" : record.name)
            << '\n';
        if (!record.conditions_text.empty()) {
            out << record.conditions_text << '\n';
        } else {
            out << serialize_conditions(record.conditions) << '\n';
        }
    }
    return out.str();
}

} // namespace gba::mednafen_cht
