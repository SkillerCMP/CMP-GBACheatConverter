#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gba::mednafen_cht {

enum class ConditionOperator {
    GreaterOrEqual,
    LessOrEqual,
    Greater,
    Less,
    Equal,
    NotEqual,
    AndNonzero,
    AndZero,
    XorNonzero,
    XorZero,
    OrNonzero,
    OrZero
};

struct Condition {
    std::uint8_t length = 1U;
    bool big_endian = false;
    std::uint32_t address = 0U;
    ConditionOperator operation = ConditionOperator::Equal;
    std::uint64_t value = 0U;
};

struct Record {
    std::string rom_md5;
    std::string game_name;
    std::string name;
    char type = 'R';
    bool enabled = false;
    std::uint8_t length = 1U;
    bool big_endian = false;
    std::uint32_t instance_count = 0U;
    std::uint32_t address = 0U;
    std::uint64_t value = 0U;
    std::uint64_t compare = 0U;
    std::uint32_t repeat_count = 1U;
    std::uint32_t repeat_address_increment = 0U;
    std::uint64_t repeat_value_increment = 0U;
    std::uint32_t copy_source_address = 0U;
    std::uint32_t copy_source_increment = 0U;
    std::string conditions_text;
    std::vector<Condition> conditions;
};

struct ParseResult {
    bool recognized = false;
    bool success = false;
    std::vector<Record> records;
    std::vector<std::string> warnings;
};

ParseResult parse(std::string_view text);
bool looks_like(std::string_view text);
std::string serialize(const std::vector<Record>& records);
std::string serialize_conditions(const std::vector<Condition>& conditions);

} // namespace gba::mednafen_cht
