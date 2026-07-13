#include "formats/ezflash_parse_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace gba::ezflash::parse_detail {

std::vector<ConditionTerm> make_condition_terms(
    const std::vector<ParsedRun>& runs) {
    std::vector<ConditionTerm> terms;
    for (const ParsedRun& run : runs) {
        std::size_t offset = 0U;
        while (offset < run.bytes.size()) {
            const std::size_t remaining = run.bytes.size() - offset;
            const std::uint8_t width = remaining >= 4U
                ? 4U
                : (remaining == 3U ? 3U : (remaining >= 2U ? 2U : 1U));
            terms.push_back(ConditionTerm{
                run.address + static_cast<std::uint32_t>(offset),
                little_endian_value(run.bytes, offset, width),
                width
            });
            offset += width;
        }
    }
    return terms;
}


std::optional<OperationKind> condition_kind_for_key(std::string_view key) {
    if (key == "IF") return OperationKind::IfEqual;
    if (key == "IFNE") return OperationKind::IfNotEqual;
    if (key == "IFLT") return OperationKind::IfLess;
    if (key == "IFGT") return OperationKind::IfGreater;
    if (key == "IFLE") return OperationKind::IfLessOrEqual;
    if (key == "IFGE") return OperationKind::IfGreaterOrEqual;
    return std::nullopt;
}

} // namespace gba::ezflash::parse_detail
