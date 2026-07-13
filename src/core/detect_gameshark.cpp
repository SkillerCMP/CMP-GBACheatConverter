#include "core/detect_internal.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gba::detect::internal {

Candidate score_gameshark(const std::vector<Line88>& rows, Format format) {
    Candidate result;
    result.format = format;

    int recognized = 0;
    int strong_addresses = 0;

    for (std::size_t index = 0; index < rows.size(); ++index) {
        const Line88& row = rows[index];

        if (row.right == 0x001DC0DEU) {
            result.score += 18;
            ++recognized;
            result.reasons.push_back(
                "contains a GameShark/AR GBX game-ID row");
            continue;
        }

        if (row.left == 0xDEADFACEU) {
            result.score += 16;
            ++recognized;
            result.reasons.push_back(
                "contains a GameShark/AR GBX DEADFACE seed row");
            continue;
        }

        const std::uint8_t type =
            static_cast<std::uint8_t>(row.left >> 28U);
        const std::uint32_t address = row.left & 0x0FFFFFFFU;

        switch (type) {
        case 0x0:
            result.score += 7 + address_score(address);
            result.score += row.right <= 0xFFU ? 4 : -4;
            ++recognized;
            break;
        case 0x1:
            result.score += 7 + address_score(address);
            result.score += row.right <= 0xFFFFU ? 4 : -4;
            ++recognized;
            break;
        case 0x2:
            result.score += 7 + address_score(address);
            ++recognized;
            break;
        case 0x3: {
            const std::uint32_t count = row.left & 0xFFFFU;
            const std::size_t continuation_rows = count / 2U;
            if ((row.left & 0x0FFF0000U) != 0U ||
                count == 0U ||
                index + continuation_rows >= rows.size()) {
                result.score -= 18;
                break;
            }

            result.score += 16 + address_score(row.right);
            ++recognized;
            if (address_score(row.right) >= 5) {
                ++strong_addresses;
            }

            std::uint32_t remaining = count - 1U;
            for (std::size_t continuation = 0;
                 continuation < continuation_rows;
                 ++continuation) {
                const Line88& parameter = rows[index + 1U + continuation];
                result.score += 5 + address_score(parameter.left);
                if (address_score(parameter.left) >= 5) {
                    ++strong_addresses;
                }
                if (remaining > 0U) {
                    --remaining;
                }

                if (remaining > 0U) {
                    result.score += 5 + address_score(parameter.right);
                    if (address_score(parameter.right) >= 5) {
                        ++strong_addresses;
                    }
                    --remaining;
                } else if (parameter.right != 0U) {
                    result.score -= 3;
                }
                ++recognized;
            }

            result.reasons.push_back(
                "contains a structurally valid GameShark assignment list");
            index += continuation_rows;
            break;
        }
        case 0x6: {
            const std::uint32_t mode = row.right >> 28U;
            const bool canonical_address =
                (row.left & 0x0F000000U) == 0U;
            const bool canonical_flags =
                (row.right & 0x0FFF0000U) == 0U;
            result.score +=
                canonical_address && canonical_flags ? 15 : -12;
            result.score += mode <= 2U ? 3 : -3;
            ++recognized;
            result.reasons.push_back(
                canonical_address && canonical_flags
                    ? "contains a structurally valid GameShark ROM patch"
                    : "contains a noncanonical GameShark type-6 row");
            break;
        }
        case 0x8: {
            const std::uint32_t subtype =
                row.left & 0x00F00000U;
            if (subtype == 0x00100000U ||
                subtype == 0x00200000U) {
                const std::uint32_t target =
                    row.left & 0x0F0FFFFFU;
                result.score += 12 + address_score(target);
                result.score +=
                    subtype == 0x00100000U
                        ? (row.right <= 0xFFU ? 4 : -4)
                        : (row.right <= 0xFFFFU ? 4 : -4);
                if (address_score(target) >= 5) {
                    ++strong_addresses;
                }
                result.reasons.push_back(
                    "contains a GameShark physical-button write");
            } else if (row.left == 0x80F00000U) {
                result.score +=
                    row.right <= 0xFFFFU ? 11 : -7;
                result.reasons.push_back(
                    "contains a GameShark physical-button slowdown row");
            } else {
                result.score -= 10;
            }
            ++recognized;
            break;
        }
        case 0xD: {
            const std::uint32_t subtype = row.right >> 20U;
            result.score += 7 + address_score(address);
            result.score += subtype <= 3U ? 5 : -8;
            ++recognized;
            break;
        }
        case 0xE: {
            const std::uint32_t span = (row.left >> 16U) & 0xFFU;
            const std::uint32_t condition_address =
                row.right & 0x0FFFFFFFU;
            result.score += 10 + address_score(condition_address);
            result.score += span > 0U ? 5 : -10;
            ++recognized;
            break;
        }
        case 0xF:
            result.score += 10;
            result.score +=
                (address >= 0x08000000U &&
                 address <= 0x09FFFFFFU)
                    ? 4
                    : -4;
            ++recognized;
            break;
        default:
            result.score -= 10;
            break;
        }

        if (type != 0x3 && address_score(address) >= 5) {
            ++strong_addresses;
        }
    }

    if (recognized == static_cast<int>(rows.size())) {
        result.score += 8;
        result.reasons.push_back(
            "all 8+8 rows use known GameShark/AR GBX structures");
    }
    if (strong_addresses > 0) {
        result.reasons.push_back(
            std::to_string(strong_addresses) +
            " decoded address(es) are in primary GBA RAM/I/O ranges");
    }
    return result;
}

} // namespace gba::detect::internal
