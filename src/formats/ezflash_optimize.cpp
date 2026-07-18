#include "formats/ezflash_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace gba::ezflash::detail {
namespace {

struct OptimizedConditionBlock {
    Operation condition;
    std::vector<Operation> true_operations;
    std::vector<Operation> false_operations;
    std::size_t next_index = 0U;
};

bool condition_term_equal(const ConditionTerm& left,
                          const ConditionTerm& right) {
    return left.address == right.address &&
           left.value == right.value &&
           left.width == right.width;
}

std::vector<ConditionTerm> exact_condition_terms(const Operation& operation) {
    std::vector<ConditionTerm> terms;
    terms.reserve(1U + operation.condition_terms.size());
    terms.push_back(ConditionTerm{
        operation.address,
        operation.value,
        operation.width
    });
    terms.insert(terms.end(), operation.condition_terms.begin(),
                 operation.condition_terms.end());
    return terms;
}

bool same_condition(const Operation& left, const Operation& right) {
    if (left.kind != right.kind ||
        left.encoding_hint != right.encoding_hint ||
        left.encoding_parameter != right.encoding_parameter ||
        left.condition_has_mask != right.condition_has_mask ||
        left.condition_mask != right.condition_mask ||
        left.condition_has_else || right.condition_has_else ||
        left.condition_else_span != 0U || right.condition_else_span != 0U) {
        return false;
    }

    const auto left_terms = exact_condition_terms(left);
    const auto right_terms = exact_condition_terms(right);
    return left_terms.size() == right_terms.size() &&
           std::equal(left_terms.begin(), left_terms.end(), right_terms.begin(),
                      condition_term_equal);
}

bool simple_direct_write(const Operation& operation) {
    if (operation.kind != OperationKind::Write ||
        operation.repeat > 1U ||
        operation.address_step != 0 ||
        operation.value_step != 0) {
        return false;
    }

    const auto bytes = operation_payload(operation);
    if (bytes.empty()) return false;
    if (operation.address >
        std::numeric_limits<std::uint32_t>::max() -
            static_cast<std::uint32_t>(bytes.size() - 1U)) {
        return false;
    }

    const auto first = compact_write_address(operation.address);
    if (!first) return false;
    for (std::size_t offset = 1U; offset < bytes.size(); ++offset) {
        const auto current = compact_write_address(
            operation.address + static_cast<std::uint32_t>(offset));
        if (!current || *current != *first + offset) {
            return false;
        }
    }
    return true;
}

bool write_overlaps_condition(const Operation& operation,
                              const ConditionTerm& term) {
    const auto bytes = operation_payload(operation);
    for (std::size_t write_offset = 0U;
         write_offset < bytes.size(); ++write_offset) {
        const auto write_address = compact_write_address(
            operation.address + static_cast<std::uint32_t>(write_offset));
        if (!write_address) return true;
        for (std::uint8_t condition_offset = 0U;
             condition_offset < term.width; ++condition_offset) {
            const auto condition_address = compact_condition_address(
                term.address + condition_offset);
            if (condition_address && *write_address == *condition_address) {
                return true;
            }
        }
    }
    return false;
}

bool branch_preserves_condition(
    const std::vector<Operation>& operations,
    const std::vector<ConditionTerm>& terms) {
    if (operations.empty()) return false;

    for (const Operation& operation : operations) {
        if (!simple_direct_write(operation)) return false;
        for (const ConditionTerm& term : terms) {
            if (term.width == 0U ||
                write_overlaps_condition(operation, term)) {
                return false;
            }
        }
    }
    return true;
}

Operation merged_write(const std::vector<Operation>& source,
                       std::size_t first,
                       std::size_t count) {
    Operation merged = source[first];
    merged.byte_payload.clear();
    merged.value = 0U;
    merged.width = 0U;
    merged.repeat = 1U;
    merged.address_step = 0;
    merged.value_step = 0;
    merged.encoding_hint = EncodingHint::None;
    merged.encoding_group = 0U;
    merged.encoding_index = 0U;
    merged.encoding_count = 0U;
    merged.encoding_parameter = 0U;
    merged.encoding_auxiliary = 0U;

    for (std::size_t index = 0U; index < count; ++index) {
        const auto bytes = operation_payload(source[first + index]);
        merged.byte_payload.insert(merged.byte_payload.end(),
                                   bytes.begin(), bytes.end());
    }

    if (merged.byte_payload.size() == 1U ||
        merged.byte_payload.size() == 2U ||
        merged.byte_payload.size() == 4U) {
        merged.width = static_cast<std::uint8_t>(merged.byte_payload.size());
        for (std::uint8_t index = 0U; index < merged.width; ++index) {
            merged.value |= static_cast<std::uint32_t>(
                merged.byte_payload[index]) << (index * 8U);
        }
    }
    return merged;
}

std::vector<Operation> merge_adjacent_writes(
    const std::vector<Operation>& operations) {
    std::vector<Operation> result;
    result.reserve(operations.size());

    std::size_t index = 0U;
    while (index < operations.size()) {
        if (!simple_direct_write(operations[index])) {
            result.push_back(operations[index]);
            ++index;
            continue;
        }

        std::size_t count = 1U;
        const auto first_compact = compact_write_address(
            operations[index].address);
        std::uint64_t next_address = first_compact
            ? static_cast<std::uint64_t>(*first_compact) +
                  operation_payload(operations[index]).size()
            : std::numeric_limits<std::uint64_t>::max();
        while (index + count < operations.size() &&
               simple_direct_write(operations[index + count])) {
            const auto current = compact_write_address(
                operations[index + count].address);
            if (!current || next_address != *current) {
                break;
            }
            next_address += operation_payload(
                operations[index + count]).size();
            ++count;
        }

        result.push_back(merged_write(operations, index, count));
        index += count;
    }
    return result;
}

bool optimize_range(const std::vector<Operation>& source,
                    std::size_t first,
                    std::size_t count,
                    std::vector<Operation>& output);

bool optimize_condition_block(const std::vector<Operation>& source,
                              std::size_t index,
                              std::size_t end,
                              OptimizedConditionBlock& block) {
    if (index >= end || !is_condition(source[index].kind)) return false;

    const Operation& condition = source[index];
    const std::size_t true_span = condition.condition_span == 0U
        ? 1U : static_cast<std::size_t>(condition.condition_span);
    const std::size_t false_span =
        static_cast<std::size_t>(condition.condition_else_span);
    const std::size_t controlled = true_span + false_span;
    if (controlled == 0U || controlled > end - index - 1U) return false;

    block.condition = condition;
    if (!optimize_range(source, index + 1U, true_span,
                        block.true_operations) ||
        !optimize_range(source, index + 1U + true_span, false_span,
                        block.false_operations)) {
        return false;
    }
    block.condition.condition_span = static_cast<std::uint32_t>(
        block.true_operations.size());
    block.condition.condition_else_span = static_cast<std::uint32_t>(
        block.false_operations.size());
    block.next_index = index + 1U + controlled;
    return true;
}

bool mergeable_condition_block(const OptimizedConditionBlock& block) {
    if (block.condition.condition_has_else ||
        !block.false_operations.empty() ||
        block.condition.condition_else_span != 0U) {
        return false;
    }

    const auto terms = exact_condition_terms(block.condition);
    if (terms.empty() ||
        std::any_of(terms.begin(), terms.end(), [](const ConditionTerm& term) {
            return is_rom_address(term.address);
        })) {
        return false;
    }
    return branch_preserves_condition(block.true_operations, terms);
}

void append_condition_block(const OptimizedConditionBlock& block,
                            std::vector<Operation>& output) {
    Operation condition = block.condition;
    condition.condition_span = static_cast<std::uint32_t>(
        block.true_operations.size());
    condition.condition_else_span = static_cast<std::uint32_t>(
        block.false_operations.size());
    output.push_back(std::move(condition));
    output.insert(output.end(), block.true_operations.begin(),
                  block.true_operations.end());
    output.insert(output.end(), block.false_operations.begin(),
                  block.false_operations.end());
}

bool optimize_range(const std::vector<Operation>& source,
                    std::size_t first,
                    std::size_t count,
                    std::vector<Operation>& output) {
    if (first > source.size() || count > source.size() - first) return false;

    const std::size_t end = first + count;
    std::vector<Operation> staged;
    staged.reserve(count);

    std::size_t index = first;
    while (index < end) {
        if (!is_condition(source[index].kind)) {
            std::vector<Operation> siblings;
            while (index < end && !is_condition(source[index].kind)) {
                siblings.push_back(source[index]);
                ++index;
            }
            siblings = merge_adjacent_writes(siblings);
            staged.insert(staged.end(), siblings.begin(), siblings.end());
            continue;
        }

        OptimizedConditionBlock block;
        if (!optimize_condition_block(source, index, end, block)) return false;

        if (mergeable_condition_block(block)) {
            const auto terms = exact_condition_terms(block.condition);
            std::size_t scan = block.next_index;
            while (scan < end && is_condition(source[scan].kind)) {
                OptimizedConditionBlock candidate;
                if (!optimize_condition_block(source, scan, end, candidate) ||
                    !same_condition(block.condition, candidate.condition) ||
                    !mergeable_condition_block(candidate) ||
                    !branch_preserves_condition(candidate.true_operations,
                                                terms)) {
                    break;
                }
                block.true_operations.insert(block.true_operations.end(),
                                             candidate.true_operations.begin(),
                                             candidate.true_operations.end());
                block.true_operations = merge_adjacent_writes(
                    block.true_operations);
                block.next_index = candidate.next_index;
                scan = candidate.next_index;
            }
        }

        block.true_operations = merge_adjacent_writes(block.true_operations);
        block.false_operations = merge_adjacent_writes(block.false_operations);
        append_condition_block(block, staged);
        index = block.next_index;
    }

    output = std::move(staged);
    return true;
}

} // namespace

CheatEntry optimize_enhanced_v4_entry(const CheatEntry& entry) {
    CheatEntry optimized = entry;
    std::vector<Operation> operations;
    if (optimize_range(entry.operations, 0U, entry.operations.size(), operations)) {
        optimized.operations = std::move(operations);
    }
    return optimized;
}

} // namespace gba::ezflash::detail
