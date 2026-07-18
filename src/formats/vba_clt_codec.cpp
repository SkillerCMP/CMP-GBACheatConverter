#include "formats/vba_clt_codec.hpp"

#include <algorithm>
#include <limits>

namespace gba::vba_clt {
namespace {

std::optional<std::uint32_t> read_u32(
    const std::vector<std::uint8_t>& data,
    std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4U) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void patch_u32(std::vector<std::uint8_t>& out,
               std::size_t offset,
               std::uint32_t value) {
    out[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    out[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    out[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    out[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

std::string fixed_text(const std::vector<std::uint8_t>& data,
                       std::size_t offset,
                       std::size_t size) {
    if (offset > data.size() || data.size() - offset < size) return {};
    std::size_t count = 0U;
    while (count < size && data[offset + count] != 0U) ++count;
    return std::string(reinterpret_cast<const char*>(data.data() + offset),
                       count);
}

void write_fixed(std::vector<std::uint8_t>& out,
                 std::size_t offset,
                 std::size_t size,
                 std::string_view value) {
    if (size == 0U) return;
    const std::size_t count = std::min(size - 1U, value.size());
    for (std::size_t index = 0U; index < count; ++index) {
        out[offset + index] = static_cast<std::uint8_t>(
            static_cast<unsigned char>(value[index]));
    }
}

std::int32_t as_i32(std::uint32_t value) {
    if (value <= static_cast<std::uint32_t>(
                     std::numeric_limits<std::int32_t>::max())) {
        return static_cast<std::int32_t>(value);
    }
    const std::int64_t signed_value = static_cast<std::int64_t>(value) -
        (std::int64_t{1} << 32U);
    return static_cast<std::int32_t>(signed_value);
}

std::optional<Record> decode_current_record(
    const std::vector<std::uint8_t>& data,
    std::size_t offset) {
    if (offset > data.size() ||
        data.size() - offset < kCurrentRecordSize) {
        return std::nullopt;
    }
    const auto code = read_u32(data, offset + 0U);
    const auto size = read_u32(data, offset + 4U);
    const auto status = read_u32(data, offset + 8U);
    const auto raw_address = read_u32(data, offset + 16U);
    const auto address = read_u32(data, offset + 20U);
    const auto value = read_u32(data, offset + 24U);
    const auto old_value = read_u32(data, offset + 28U);
    if (!code || !size || !status || !raw_address || !address ||
        !value || !old_value) {
        return std::nullopt;
    }

    Record record;
    record.code = as_i32(*code);
    record.size = as_i32(*size);
    record.status = as_i32(*status);
    record.enabled = data[offset + 12U] != 0U;
    record.raw_address = *raw_address;
    record.address = *address;
    record.value = *value;
    record.old_value = *old_value;
    record.code_string = fixed_text(data, offset + 32U, 20U);
    record.description = fixed_text(data, offset + 52U, 32U);
    return record;
}

std::optional<Record> decode_legacy_record(
    const std::vector<std::uint8_t>& data,
    std::size_t offset) {
    if (offset > data.size() ||
        data.size() - offset < kLegacyRecordSize) {
        return std::nullopt;
    }
    const auto code = read_u32(data, offset + 0U);
    const auto size = read_u32(data, offset + 4U);
    const auto status = read_u32(data, offset + 8U);
    const auto enabled = read_u32(data, offset + 12U);
    const auto address = read_u32(data, offset + 16U);
    const auto value = read_u32(data, offset + 20U);
    const auto old_value = read_u32(data, offset + 24U);
    if (!code || !size || !status || !enabled || !address || !value ||
        !old_value) {
        return std::nullopt;
    }

    Record record;
    record.code = as_i32(*code);
    record.size = as_i32(*size);
    record.status = as_i32(*status);
    record.enabled = *enabled != 0U;
    record.raw_address = *address;
    record.address = *address;
    record.value = *value;
    record.old_value = *old_value;
    record.code_string = fixed_text(data, offset + 28U, 20U);
    record.description = fixed_text(data, offset + 48U, 32U);
    return record;
}

} // namespace

bool has_supported_header(const std::vector<std::uint8_t>& data) {
    const auto version = read_u32(data, 0U);
    const auto type = read_u32(data, 4U);
    return version && type && *version == kVersion &&
           (*type == kLegacyType || *type == kCurrentType);
}

std::optional<DecodedFile> decode(const std::vector<std::uint8_t>& data,
                                  std::string& error) {
    error.clear();
    const auto version = read_u32(data, 0U);
    const auto type = read_u32(data, 4U);
    const auto count = read_u32(data, 8U);
    if (!version || !type || !count) {
        error = "The VisualBoy Advance-M .clt header is truncated.";
        return std::nullopt;
    }
    if (*version != kVersion) {
        error = "The VisualBoy Advance-M .clt version is unsupported.";
        return std::nullopt;
    }
    if (*type != kLegacyType && *type != kCurrentType) {
        error = "The VisualBoy Advance-M .clt record type is unsupported.";
        return std::nullopt;
    }
    if (*count > kMaximumRecords) {
        error = "The VisualBoy Advance-M .clt record count exceeds 16,384.";
        return std::nullopt;
    }

    const std::size_t record_size = *type == kCurrentType
        ? kCurrentRecordSize : kLegacyRecordSize;
    if (static_cast<std::size_t>(*count) >
        (std::numeric_limits<std::size_t>::max() - kHeaderSize) /
            record_size) {
        error = "The VisualBoy Advance-M .clt record table is too large.";
        return std::nullopt;
    }
    const std::size_t required = kHeaderSize +
        static_cast<std::size_t>(*count) * record_size;
    if (data.size() < required) {
        error = "The VisualBoy Advance-M .clt record table is truncated.";
        return std::nullopt;
    }

    DecodedFile decoded;
    decoded.type = *type;
    decoded.records.reserve(*count);
    for (std::uint32_t index = 0U; index < *count; ++index) {
        const std::size_t offset = kHeaderSize +
            static_cast<std::size_t>(index) * record_size;
        const auto record = *type == kCurrentType
            ? decode_current_record(data, offset)
            : decode_legacy_record(data, offset);
        if (!record) {
            error = "A VisualBoy Advance-M .clt record is truncated.";
            return std::nullopt;
        }
        decoded.records.push_back(*record);
    }
    return decoded;
}

std::vector<std::uint8_t> encode_current(
    const std::vector<Record>& records) {
    if (records.size() > kMaximumRecords) return {};
    const std::size_t count = records.size();
    std::vector<std::uint8_t> out;
    out.reserve(kHeaderSize + count * kCurrentRecordSize);
    append_u32(out, kVersion);
    append_u32(out, kCurrentType);
    append_u32(out, static_cast<std::uint32_t>(count));

    for (std::size_t index = 0U; index < count; ++index) {
        const Record& record = records[index];
        const std::size_t start = out.size();
        out.resize(start + kCurrentRecordSize, 0U);
        patch_u32(out, start + 0U,
                  static_cast<std::uint32_t>(record.code));
        patch_u32(out, start + 4U,
                  static_cast<std::uint32_t>(record.size));
        patch_u32(out, start + 8U,
                  static_cast<std::uint32_t>(record.status));
        out[start + 12U] = record.enabled ? 1U : 0U;
        patch_u32(out, start + 16U, record.raw_address);
        patch_u32(out, start + 20U, record.address);
        patch_u32(out, start + 24U, record.value);
        patch_u32(out, start + 28U, record.old_value);
        write_fixed(out, start + 32U, 20U, record.code_string);
        write_fixed(out, start + 52U, 32U, record.description);
    }
    return out;
}

} // namespace gba::vba_clt
