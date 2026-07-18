#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gba::mister_gg {

constexpr std::size_t kRecordSize = 16U;
constexpr std::size_t kGbaMaxRecords = 32U;

struct EncodeResult {
    bool success = false;
    std::vector<std::uint8_t> data;
    std::string error;
    std::size_t record_count = 0U;
};

struct DecodeResult {
    bool success = false;
    CheatEntry entry;
    std::vector<std::string> warnings;
    std::string error;
    std::size_t record_count = 0U;
};

EncodeResult encode_entry(const CheatEntry& entry);
DecodeResult decode_entry(std::string_view name,
                          const std::vector<std::uint8_t>& data);

} // namespace gba::mister_gg
