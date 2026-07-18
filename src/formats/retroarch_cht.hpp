#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gba::retroarch_cht {

struct Record {
    std::string desc;
    std::string code;
    bool enabled = false;
    bool big_endian = false;
    std::uint32_t handler = 0U;
    std::uint32_t memory_search_size = 3U;
    std::uint32_t cheat_type = 1U;
    std::uint32_t value = 0U;
    std::uint32_t address = 0U;
    std::uint32_t address_mask = 0U;
    std::uint32_t rumble_type = 0U;
    std::uint32_t rumble_value = 0U;
    std::uint32_t rumble_port = 0U;
    std::uint32_t rumble_primary_strength = 0U;
    std::uint32_t rumble_primary_duration = 0U;
    std::uint32_t rumble_secondary_strength = 0U;
    std::uint32_t rumble_secondary_duration = 0U;
    std::uint32_t repeat_count = 1U;
    std::uint32_t repeat_add_to_value = 0U;
    std::uint32_t repeat_add_to_address = 1U;
};

struct ParseResult {
    bool recognized = false;
    bool success = false;
    std::vector<Record> records;
    std::vector<std::string> warnings;
};

ParseResult parse(std::string_view text);
std::string serialize(const std::vector<Record>& records);

} // namespace gba::retroarch_cht
