#pragma once

#include <cstdint>
#include <optional>

namespace gba::memory {

// GBA EWRAM is 256 KiB mirrored throughout the 0x02xxxxxx bank.
inline std::optional<std::uint32_t> canonical_ewram_address(
    std::uint32_t address) {
    if ((address & 0xFF000000U) != 0x02000000U) {
        return std::nullopt;
    }
    return 0x02000000U | (address & 0x0003FFFFU);
}

// GBA IWRAM is 32 KiB mirrored throughout the 0x03xxxxxx bank.
inline std::optional<std::uint32_t> canonical_iwram_address(
    std::uint32_t address) {
    if ((address & 0xFF000000U) != 0x03000000U) {
        return std::nullopt;
    }
    return 0x03000000U | (address & 0x00007FFFU);
}

inline std::optional<std::uint32_t> canonical_ram_address(
    std::uint32_t address) {
    if (const auto ewram = canonical_ewram_address(address)) {
        return ewram;
    }
    return canonical_iwram_address(address);
}

} // namespace gba::memory
