#include "core/detect_internal.hpp"

#include "core/gba_memory.hpp"
#include "core/text.hpp"

#include <string>
#include <string_view>

namespace gba::detect::internal {

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

std::optional<Line88> parse_8x8(std::string_view raw) {
    const std::string line = text::trim(raw);
    if (!text::is_code_line_8x8(line)) {
        return std::nullopt;
    }

    const auto left = text::parse_hex_u32(line.substr(0, 8));
    const auto right = text::parse_hex_u32(line.substr(9, 8));
    if (!left || !right) {
        return std::nullopt;
    }
    return Line88{*left, *right};
}

bool plausible_gba_address(std::uint32_t address) {
    return memory::canonical_ram_address(address).has_value() ||
           (address >= 0x04000000U && address <= 0x040003FFU) ||
           (address >= 0x05000000U && address <= 0x050003FFU) ||
           (address >= 0x06000000U && address <= 0x06017FFFU) ||
           (address >= 0x07000000U && address <= 0x070003FFU) ||
           (address >= 0x08000000U && address <= 0x09FFFFFFU) ||
           (address >= 0x0E000000U && address <= 0x0E00FFFFU);
}

int address_score(std::uint32_t address) {
    if (memory::canonical_ram_address(address)) {
        return 7;
    }
    if (address >= 0x04000000U && address <= 0x040003FFU) {
        return 5;
    }
    if (address >= 0x08000000U && address <= 0x09FFFFFFU) {
        return 4;
    }
    if (plausible_gba_address(address)) {
        return 2;
    }
    return -5;
}

} // namespace gba::detect::internal
