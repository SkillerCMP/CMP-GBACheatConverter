#include "formats/mister_gg.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::mister_gg {
namespace {

struct Record {
    std::uint32_t flags = 0U;
    std::uint32_t address = 0U;
    std::uint32_t compare = 0U;
    std::uint32_t value = 0U;
};

std::uint32_t read_u32(const std::vector<std::uint8_t>& data,
                       std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
        (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
        (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

void append_u32(std::vector<std::uint8_t>& data, std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        data.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

std::optional<std::uint8_t> operation_type(OperationKind kind) {
    switch (kind) {
    case OperationKind::IfEqual: return std::uint8_t{1U};
    case OperationKind::IfGreater: return std::uint8_t{2U};
    // The GBA core's labels for operations 3 and 4 are reversed relative to
    // their actual VHDL truth tables. Preserve behavior, not the labels.
    case OperationKind::IfGreaterOrEqual: return std::uint8_t{3U};
    case OperationKind::IfLess: return std::uint8_t{4U};
    case OperationKind::IfLessOrEqual: return std::uint8_t{5U};
    case OperationKind::IfNotEqual: return std::uint8_t{6U};
    default: return std::nullopt;
    }
}

std::optional<OperationKind> condition_kind(std::uint8_t type) {
    switch (type) {
    case 1U: return OperationKind::IfEqual;
    case 2U: return OperationKind::IfGreater;
    case 3U: return OperationKind::IfGreaterOrEqual;
    case 4U: return OperationKind::IfLess;
    case 5U: return OperationKind::IfLessOrEqual;
    case 6U: return OperationKind::IfNotEqual;
    default: return std::nullopt;
    }
}

std::uint32_t expanded_byte_mask(std::uint8_t byte_mask) {
    std::uint32_t result = 0U;
    for (unsigned lane = 0U; lane < 4U; ++lane) {
        if ((byte_mask & (1U << lane)) != 0U) {
            result |= 0xFFU << (lane * 8U);
        }
    }
    return result;
}

struct PositionedValue {
    std::uint32_t base_address = 0U;
    std::uint8_t byte_mask = 0U;
    std::uint32_t value = 0U;
};

std::optional<PositionedValue> position_operation(const Operation& operation,
                                                  bool condition) {
    if (operation.width != 1U && operation.width != 2U &&
        operation.width != 4U) {
        return std::nullopt;
    }
    if (operation.address > 0x0FFFFFFFU) return std::nullopt;

    const std::uint32_t lane = operation.address & 3U;
    if (lane + operation.width > 4U) return std::nullopt;
    if (operation.width == 2U && lane != 0U && lane != 2U) {
        return std::nullopt;
    }
    if (operation.width == 4U && lane != 0U) return std::nullopt;

    std::uint32_t local_mask = operation.width == 4U
        ? 0xFFFFFFFFU
        : ((1U << (operation.width * 8U)) - 1U);
    if (condition && operation.condition_has_mask) {
        local_mask = operation.condition_mask;
        const std::uint32_t width_mask = operation.width == 4U
            ? 0xFFFFFFFFU
            : ((1U << (operation.width * 8U)) - 1U);
        if ((local_mask & ~width_mask) != 0U) return std::nullopt;
    }

    std::uint8_t byte_mask = 0U;
    for (unsigned byte = 0U; byte < operation.width; ++byte) {
        const std::uint32_t byte_value =
            (local_mask >> (byte * 8U)) & 0xFFU;
        if (byte_value == 0xFFU) {
            byte_mask = static_cast<std::uint8_t>(
                byte_mask | (1U << (lane + byte)));
        } else if (byte_value != 0U) {
            return std::nullopt;
        }
    }
    if (byte_mask == 0U) return std::nullopt;

    const std::uint32_t width_mask = operation.width == 4U
        ? 0xFFFFFFFFU
        : ((1U << (operation.width * 8U)) - 1U);
    PositionedValue result;
    result.base_address = operation.address & ~3U;
    result.byte_mask = byte_mask;
    result.value = (operation.value & width_mask) << (lane * 8U);
    return result;
}

std::optional<Record> encode_record(const Operation& operation,
                                    std::uint8_t type) {
    const auto positioned = position_operation(operation, type != 0U);
    if (!positioned) return std::nullopt;
    Record record;
    record.flags = (static_cast<std::uint32_t>(positioned->byte_mask) << 4U) |
        static_cast<std::uint32_t>(type);
    record.address = positioned->base_address;
    record.compare = 0U; // Present in the file layout but unused by GBA RTL.
    record.value = positioned->value;
    return record;
}

void append_record(std::vector<std::uint8_t>& data, const Record& record) {
    append_u32(data, record.flags);
    append_u32(data, record.address);
    append_u32(data, record.compare);
    append_u32(data, record.value);
}

std::vector<Operation> decode_write(std::uint32_t address,
                                    std::uint8_t byte_mask,
                                    std::uint32_t value) {
    std::vector<Operation> operations;
    unsigned lane = 0U;
    while (lane < 4U) {
        if ((byte_mask & (1U << lane)) == 0U) {
            ++lane;
            continue;
        }

        std::uint8_t width = 1U;
        if ((lane == 0U || lane == 2U) &&
            (byte_mask & (3U << lane)) == (3U << lane)) {
            width = 2U;
        }
        if (lane == 0U && byte_mask == 0x0FU) width = 4U;

        Operation operation;
        operation.kind = OperationKind::Write;
        operation.address = address + lane;
        operation.width = width;
        const std::uint32_t width_mask = width == 1U
            ? 0xFFU : (width == 2U ? 0xFFFFU : 0xFFFFFFFFU);
        operation.value = (value >> (lane * 8U)) & width_mask;
        operations.push_back(operation);
        lane += width;
    }
    return operations;
}

Operation decode_condition(std::uint32_t address,
                           std::uint8_t byte_mask,
                           std::uint32_t value,
                           OperationKind kind) {
    Operation operation;
    operation.kind = kind;
    operation.condition_span = 1U;

    const std::uint32_t selected_mask = expanded_byte_mask(byte_mask);
    const bool outside_value_bits = (value & ~selected_mask) != 0U;
    if (!outside_value_bits) {
        switch (byte_mask) {
        case 0x01U:
        case 0x02U:
        case 0x04U:
        case 0x08U: {
            unsigned lane = 0U;
            while ((byte_mask & (1U << lane)) == 0U) ++lane;
            operation.address = address + lane;
            operation.width = 1U;
            operation.value = (value >> (lane * 8U)) & 0xFFU;
            return operation;
        }
        case 0x03U:
            operation.address = address;
            operation.width = 2U;
            operation.value = value & 0xFFFFU;
            return operation;
        case 0x0CU:
            operation.address = address + 2U;
            operation.width = 2U;
            operation.value = (value >> 16U) & 0xFFFFU;
            return operation;
        case 0x0FU:
            operation.address = address;
            operation.width = 4U;
            operation.value = value;
            return operation;
        default:
            break;
        }
    }

    operation.address = address;
    operation.width = 4U;
    operation.value = value;
    operation.condition_has_mask = true;
    operation.condition_mask = selected_mask;
    return operation;
}

std::optional<Record> decode_raw_record(const std::vector<std::uint8_t>& data,
                                        std::size_t offset,
                                        std::string& error) {
    Record record;
    record.flags = read_u32(data, offset);
    record.address = read_u32(data, offset + 4U);
    record.compare = read_u32(data, offset + 8U);
    record.value = read_u32(data, offset + 12U);

    if ((record.flags & 0xFFFFFF00U) != 0U) {
        error = "A MiSTer GBA record has nonzero reserved flag bits.";
        return std::nullopt;
    }
    const std::uint8_t type = static_cast<std::uint8_t>(record.flags & 0x0FU);
    const std::uint8_t mask = static_cast<std::uint8_t>(
        (record.flags >> 4U) & 0x0FU);
    if (type > 6U) {
        error = "A MiSTer GBA record uses an unsupported operation type.";
        return std::nullopt;
    }
    if (mask == 0U) {
        error = "A MiSTer GBA record has an empty byte mask.";
        return std::nullopt;
    }
    if (record.address > 0x0FFFFFFFU || (record.address & 3U) != 0U) {
        error = "A MiSTer GBA record has a noncanonical 28-bit address.";
        return std::nullopt;
    }
    return record;
}

} // namespace

EncodeResult encode_entry(const CheatEntry& entry) {
    EncodeResult result;
    std::vector<Record> records;

    for (std::size_t index = 0U; index < entry.operations.size();) {
        const Operation& operation = entry.operations[index];
        if (operation.kind == OperationKind::Write) {
            const auto record = encode_record(operation, 0U);
            if (!record) {
                result.error = "MiSTer GBA cannot represent one of the writes.";
                return result;
            }
            records.push_back(*record);
            ++index;
            continue;
        }

        const auto type = operation_type(operation.kind);
        if (!type || operation.condition_has_else ||
            operation.condition_else_span != 0U ||
            !operation.condition_terms.empty() ||
            operation.condition_span == 0U ||
            index + operation.condition_span >= entry.operations.size()) {
            result.error = "MiSTer GBA cannot represent this condition block.";
            return result;
        }
        const auto condition = encode_record(operation, *type);
        if (!condition) {
            result.error = "MiSTer GBA cannot represent this condition mask.";
            return result;
        }

        // GBA MiSTer has a one-record skip flag. Repeat the condition before
        // every controlled write so a multi-write semantic block remains exact.
        for (std::uint32_t offset = 1U;
             offset <= operation.condition_span; ++offset) {
            const Operation& controlled = entry.operations[index + offset];
            if (controlled.kind != OperationKind::Write) {
                result.error =
                    "MiSTer GBA conditions can control direct writes only.";
                return result;
            }
            const auto write = encode_record(controlled, 0U);
            if (!write) {
                result.error =
                    "MiSTer GBA cannot represent a controlled write.";
                return result;
            }
            records.push_back(*condition);
            records.push_back(*write);
        }
        index += static_cast<std::size_t>(operation.condition_span) + 1U;
    }

    if (records.empty()) {
        result.error = "MiSTer GBA cheat files cannot be empty.";
        return result;
    }
    if (records.size() > kGbaMaxRecords) {
        result.error = "MiSTer GBA supports at most 32 active records.";
        return result;
    }

    result.data.reserve(records.size() * kRecordSize);
    for (const Record& record : records) append_record(result.data, record);
    result.record_count = records.size();
    result.success = true;
    return result;
}

DecodeResult decode_entry(std::string_view name,
                          const std::vector<std::uint8_t>& data) {
    DecodeResult result;
    result.entry.name = std::string(name);
    if (data.empty() || (data.size() % kRecordSize) != 0U) {
        result.error = "A MiSTer .gg payload has an invalid record size.";
        return result;
    }
    const std::size_t record_count = data.size() / kRecordSize;
    if (record_count > kGbaMaxRecords) {
        result.error = "A MiSTer GBA .gg file exceeds the 32-record core limit.";
        return result;
    }

    std::vector<Record> records;
    records.reserve(record_count);
    bool saw_nonzero_compare = false;
    for (std::size_t index = 0U; index < record_count; ++index) {
        std::string error;
        const auto record = decode_raw_record(
            data, index * kRecordSize, error);
        if (!record) {
            result.error = std::move(error);
            return result;
        }
        if (record->compare != 0U) saw_nonzero_compare = true;
        records.push_back(*record);
    }

    for (std::size_t index = 0U; index < records.size();) {
        const Record& record = records[index];
        const std::uint8_t type = static_cast<std::uint8_t>(record.flags & 0x0FU);
        const std::uint8_t mask = static_cast<std::uint8_t>(
            (record.flags >> 4U) & 0x0FU);
        if (type == 0U) {
            const auto writes = decode_write(record.address, mask, record.value);
            result.entry.operations.insert(result.entry.operations.end(),
                                           writes.begin(), writes.end());
            ++index;
            continue;
        }

        if (index + 1U >= records.size()) {
            result.error =
                "A MiSTer GBA condition has no following record to control.";
            return result;
        }
        const Record& controlled = records[index + 1U];
        if ((controlled.flags & 0x0FU) != 0U) {
            result.error =
                "A MiSTer GBA condition is followed by another condition; "
                "this skip-chain cannot be represented safely.";
            return result;
        }
        const auto kind = condition_kind(type);
        if (!kind) {
            result.error = "A MiSTer GBA condition type is unsupported.";
            return result;
        }

        Operation condition = decode_condition(
            record.address, mask, record.value, *kind);
        const std::uint8_t controlled_mask = static_cast<std::uint8_t>(
            (controlled.flags >> 4U) & 0x0FU);
        const auto writes = decode_write(
            controlled.address, controlled_mask, controlled.value);
        condition.condition_span = static_cast<std::uint32_t>(writes.size());
        result.entry.operations.push_back(condition);
        result.entry.operations.insert(result.entry.operations.end(),
                                       writes.begin(), writes.end());
        index += 2U;
    }

    if (saw_nonzero_compare) {
        result.warnings.push_back(
            "MiSTer GBA ignores the 32-bit compare field; nonzero compare "
            "data was discarded during import.");
    }
    result.record_count = record_count;
    result.success = !result.entry.operations.empty();
    return result;
}

} // namespace gba::mister_gg
