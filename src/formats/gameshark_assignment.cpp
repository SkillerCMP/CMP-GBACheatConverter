#include "formats/gameshark_internal.hpp"

namespace gba::gameshark::detail {

bool is_assignment_member(const Operation& operation) {
    return operation.kind == OperationKind::Write &&
           operation.width == 4U &&
           operation.repeat == 1U &&
           operation.address_step == 0 &&
           operation.value_step == 0 &&
           operation.encoding_hint ==
               EncodingHint::GameSharkAssignmentList &&
           operation.encoding_group != 0U &&
           operation.encoding_count != 0U;
}

std::size_t assignment_group_length(const CheatEntry& entry,
                                    std::size_t first,
                                    std::size_t maximum) {
    if (first >= entry.operations.size()) {
        return 0U;
    }

    const Operation& head = entry.operations[first];
    if (!is_assignment_member(head) ||
        head.encoding_index != 0U ||
        head.encoding_count < 2U ||
        head.encoding_count > maximum ||
        head.address != head.value) {
        return 0U;
    }

    const std::size_t count = head.encoding_count;
    if (first + count > entry.operations.size()) {
        return 0U;
    }

    for (std::size_t index = 0; index < count; ++index) {
        const Operation& member = entry.operations[first + index];
        if (!is_assignment_member(member) ||
            member.encoding_group != head.encoding_group ||
            member.encoding_index != index ||
            member.encoding_count != head.encoding_count ||
            member.value != head.value) {
            return 0U;
        }
    }

    return count;
}

std::vector<RawLine> encode_assignment_group(const CheatEntry& entry,
                                             std::size_t first,
                                             std::size_t count) {
    std::vector<RawLine> lines;
    const Operation& head = entry.operations[first];
    lines.push_back(RawLine{
        0x30000000U | static_cast<std::uint32_t>(count),
        head.value,
        head.source_line,
        {}});

    for (std::size_t index = 1U; index < count; index += 2U) {
        const std::uint32_t first_address =
            entry.operations[first + index].address;
        const std::uint32_t second_address =
            index + 1U < count
                ? entry.operations[first + index + 1U].address
                : 0U;
        lines.push_back(RawLine{
            first_address,
            second_address,
            entry.operations[first + index].source_line,
            {}});
    }

    return lines;
}

} // namespace gba::gameshark::detail
