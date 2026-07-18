#include "core/detect_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gba::detect::internal {

std::uint32_t armax_address(std::uint32_t encoded) {
    return (encoded & 0x000FFFFFU) |
           ((encoded << 4U) & 0x0F000000U);
}

bool known_armax_special(std::uint32_t right) {
    switch (right & 0xFE000000U) {
    case 0x00000000U:
    case 0x08000000U:
    case 0x10000000U:
    case 0x12000000U:
    case 0x14000000U:
    case 0x18000000U:
    case 0x1A000000U:
    case 0x1C000000U:
    case 0x1E000000U:
    case 0x40000000U:
    case 0x60000000U:
    case 0x80000000U:
    case 0x82000000U:
    case 0x84000000U:
        return true;
    default:
        return false;
    }
}

Candidate score_armax(const std::vector<Line88>& rows, Format format) {
    Candidate result;
    result.format = format;

    int recognized = 0;
    int strong_addresses = 0;
    bool expects_payload = false;

    for (std::size_t index = 0U; index < rows.size(); ++index) {
        const Line88& row = rows[index];
        if (expects_payload) {
            result.score += 8;
            ++recognized;
            expects_payload = false;
            continue;
        }

        if (row.left == 0xDEADFACEU) {
            result.score += 18;
            ++recognized;
            result.reasons.push_back("contains an Action Replay DEADFACE seed row");
            continue;
        }
        if (row.right == 0x001DC0DEU) {
            result.score += 16;
            ++recognized;
            result.reasons.push_back("contains an Action Replay game-ID row");
            continue;
        }
        if ((row.left >> 24U) == 0xC4U) {
            result.score += 12;
            ++recognized;
            result.reasons.push_back("contains an Action Replay hook/master signature");
            continue;
        }
        if (row.left == 0U) {
            if (known_armax_special(row.right)) {
                result.score += 10;
                ++recognized;
                const std::uint32_t special = row.right & 0xFE000000U;
                if (special == 0x10000000U || special == 0x12000000U ||
                    special == 0x14000000U || special == 0x18000000U ||
                    special == 0x1A000000U || special == 0x1C000000U ||
                    special == 0x1E000000U || special == 0x80000000U ||
                    special == 0x82000000U || special == 0x84000000U) {
                    expects_payload = true;
                }
            } else {
                result.score -= 7;
            }
            continue;
        }

        const unsigned width_exp = (row.left >> 25U) & 0x3U;
        if (width_exp == 3U) {
            result.score -= 12;
            continue;
        }

        const std::uint32_t address = armax_address(row.left);
        const int addr = address_score(address);
        result.score += addr;
        if (addr >= 5) {
            ++strong_addresses;
        }

        const std::uint32_t condition = row.left & 0x38000000U;
        const std::uint32_t base = row.left & 0xC0000000U;
        const bool reserved_special =
            (row.left & 0x01000000U) != 0U &&
            (row.left & 0xFE000000U) != 0xC6000000U;

        if (reserved_special) {
            result.score -= 8;
            continue;
        }

        if (condition != 0U) {
            result.score += 8;
            ++recognized;
            const std::size_t required = base == 0x40000000U ? 2U :
                (base == 0x00000000U ? 1U : 0U);
            if (required != 0U && rows.size() - index - 1U < required) {
                result.score -= 14;
                result.reasons.push_back(
                    "contains a trailing Action Replay condition without "
                    "all controlled row(s)");
            }
        } else if (base == 0x00000000U || base == 0x80000000U) {
            result.score += 7;
            ++recognized;
        } else if (base == 0x40000000U) {
            result.score += 9;
            ++recognized;
            result.reasons.push_back(
                "contains an Action Replay indirect/pointer write");
        } else if (base == 0xC0000000U) {
            const std::uint8_t high = static_cast<std::uint8_t>(row.left >> 24U);
            if (high == 0xC6U || high == 0xC7U) {
                result.score += 7;
                ++recognized;
            } else {
                result.score -= 6;
            }
        }
    }

    if (recognized == static_cast<int>(rows.size())) {
        result.score += 8;
        result.reasons.push_back("all 8+8 rows match known AR MAX/PAR v3 structures");
    }
    if (strong_addresses > 0) {
        result.reasons.push_back(
            std::to_string(strong_addresses) +
            " row(s) decode to primary GBA RAM/I/O ranges");
    }
    return result;
}

} // namespace gba::detect::internal
