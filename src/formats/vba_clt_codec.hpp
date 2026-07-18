#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::vba_clt {

constexpr std::uint32_t kVersion = 1U;
constexpr std::uint32_t kLegacyType = 0U;
constexpr std::uint32_t kCurrentType = 1U;
constexpr std::size_t kHeaderSize = 12U;
constexpr std::size_t kLegacyRecordSize = 80U;
constexpr std::size_t kCurrentRecordSize = 84U;
constexpr std::size_t kMaximumRecords = 16384U;

struct Record {
    std::int32_t code = 0;
    std::int32_t size = -1;
    std::int32_t status = 0;
    bool enabled = false;
    std::uint32_t raw_address = 0U;
    std::uint32_t address = 0U;
    std::uint32_t value = 0U;
    std::uint32_t old_value = 0U;
    std::string code_string;
    std::string description;
};

struct DecodedFile {
    std::uint32_t type = kCurrentType;
    std::vector<Record> records;
};

bool has_supported_header(const std::vector<std::uint8_t>& data);
std::optional<DecodedFile> decode(const std::vector<std::uint8_t>& data,
                                  std::string& error);
std::vector<std::uint8_t> encode_current(const std::vector<Record>& records);

} // namespace gba::vba_clt
