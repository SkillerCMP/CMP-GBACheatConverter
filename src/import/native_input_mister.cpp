#include "import/native_input_internal.hpp"

#include "core/text.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::native_input::detail {
namespace {

std::uint32_t crc32(const std::vector<std::uint8_t>& data,
                    std::size_t offset, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0U; index < size; ++index) {
        crc ^= data[offset + index];
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^
                (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

std::string zip_name(const std::vector<std::uint8_t>& data,
                     std::size_t offset, std::size_t size) {
    if (offset > data.size() || data.size() - offset < size) return {};
    return std::string(data.begin() + static_cast<std::ptrdiff_t>(offset),
                       data.begin() + static_cast<std::ptrdiff_t>(offset + size));
}

bool has_gg_extension(std::string_view name) {
    if (name.size() < 3U) return false;
    const std::string_view extension = name.substr(name.size() - 3U);
    return extension == ".gg" || extension == ".GG" ||
           extension == ".Gg" || extension == ".gG";
}

std::string cheat_name_from_zip(std::string_view name,
                                std::size_t fallback_index) {
    const std::size_t slash = name.find_last_of("/\\");
    const std::size_t begin = slash == std::string_view::npos ? 0U : slash + 1U;
    const std::size_t end = name.size() >= 3U ? name.size() - 3U : name.size();
    std::string result = text::trim(name.substr(begin, end - begin));
    if (result.empty()) result = "Cheat " + std::to_string(fallback_index + 1U);
    return result;
}

std::optional<Operation> decode_record(std::uint32_t flags,
                                       std::uint32_t base_address,
                                       std::uint32_t value,
                                       bool condition) {
    if ((flags & ~0xF1U) != 0U || ((flags & 0x01U) != 0U) != condition) {
        return std::nullopt;
    }
    const std::uint32_t mask = flags & 0xF0U;
    std::uint8_t width = 0U;
    std::uint32_t lane = 0U;
    switch (mask) {
    case 0x10U: width = 1U; lane = 0U; break;
    case 0x20U: width = 1U; lane = 1U; break;
    case 0x40U: width = 1U; lane = 2U; break;
    case 0x80U: width = 1U; lane = 3U; break;
    case 0x30U: width = 2U; lane = 0U; break;
    case 0xC0U: width = 2U; lane = 2U; break;
    case 0xF0U: width = 4U; lane = 0U; break;
    default: return std::nullopt;
    }
    if ((base_address & 3U) != 0U) return std::nullopt;
    Operation operation;
    operation.kind = condition ? OperationKind::IfEqual : OperationKind::Write;
    operation.address = base_address + lane;
    operation.width = width;
    const unsigned shift = static_cast<unsigned>(lane * 8U);
    const std::uint32_t width_mask = width == 1U
        ? 0xFFU : (width == 2U ? 0xFFFFU : 0xFFFFFFFFU);
    operation.value = (value >> shift) & width_mask;
    if (condition) operation.condition_span = 0U;
    return operation;
}

} // namespace

Result import_mister(std::string_view,
                     const std::vector<std::uint8_t>& data) {
    const std::string source_name = "MiSTer .zip";
    const auto first_signature = read_u32(data, 0U);
    if (!first_signature || *first_signature != 0x04034B50U) return {};

    CheatDocument document;
    std::size_t offset = 0U;
    while (offset + 4U <= data.size()) {
        const auto signature = read_u32(data, offset);
        if (!signature) break;
        if (*signature == 0x02014B50U || *signature == 0x06054B50U) break;
        if (*signature != 0x04034B50U) {
            return recognized_error(
                SourceFormat::MisterZip, source_name,
                "The MiSTer ZIP has an invalid local file header.");
        }
        if (data.size() - offset < 30U) {
            return recognized_error(
                SourceFormat::MisterZip, source_name,
                "The MiSTer ZIP local file header is truncated.");
        }
        const auto flags = read_u16(data, offset + 6U);
        const auto compression = read_u16(data, offset + 8U);
        const auto expected_crc = read_u32(data, offset + 14U);
        const auto compressed_size = read_u32(data, offset + 18U);
        const auto uncompressed_size = read_u32(data, offset + 22U);
        const auto name_size = read_u16(data, offset + 26U);
        const auto extra_size = read_u16(data, offset + 28U);
        if (!flags || !compression || !expected_crc || !compressed_size ||
            !uncompressed_size || !name_size || !extra_size) {
            return recognized_error(
                SourceFormat::MisterZip, source_name,
                "The MiSTer ZIP local file header is incomplete.");
        }
        if ((*flags & 0x0008U) != 0U) {
            return recognized_error(
                SourceFormat::MisterZip, source_name,
                "MiSTer ZIP entries using data descriptors are not supported.");
        }
        if (*compression != 0U || *compressed_size != *uncompressed_size) {
            return recognized_error(
                SourceFormat::MisterZip, source_name,
                "Only uncompressed MiSTer ZIP entries are supported.");
        }
        const std::size_t header_size = 30U +
            static_cast<std::size_t>(*name_size) +
            static_cast<std::size_t>(*extra_size);
        if (header_size > data.size() - offset ||
            static_cast<std::size_t>(*compressed_size) >
                data.size() - offset - header_size) {
            return recognized_error(
                SourceFormat::MisterZip, source_name,
                "A MiSTer ZIP entry is truncated.");
        }
        const std::string name = zip_name(data, offset + 30U, *name_size);
        const std::size_t payload_offset = offset + header_size;
        const std::size_t payload_size = *compressed_size;
        if (crc32(data, payload_offset, payload_size) != *expected_crc) {
            return recognized_error(
                SourceFormat::MisterZip, source_name,
                "A MiSTer ZIP entry failed its CRC-32 check.");
        }

        if (has_gg_extension(name)) {
            if (payload_size == 0U || (payload_size % 16U) != 0U) {
                return recognized_error(
                    SourceFormat::MisterZip, source_name,
                    "A MiSTer .gg payload has an invalid record size.");
            }
            CheatEntry entry;
            entry.name = cheat_name_from_zip(name, document.entries.size());
            constexpr std::size_t kNoCondition =
                std::numeric_limits<std::size_t>::max();
            std::size_t active_condition = kNoCondition;
            for (std::size_t record_offset = 0U;
                 record_offset < payload_size; record_offset += 16U) {
                const std::size_t absolute = payload_offset + record_offset;
                const auto record_flags = read_u32(data, absolute);
                const auto address = read_u32(data, absolute + 4U);
                const auto depth = read_u32(data, absolute + 8U);
                const auto value = read_u32(data, absolute + 12U);
                if (!record_flags || !address || !depth || !value) {
                    return recognized_error(
                        SourceFormat::MisterZip, source_name,
                        "A MiSTer .gg record is truncated.");
                }
                const bool condition = (*record_flags & 0x01U) != 0U;
                if (condition) {
                    if (*depth != 0U || active_condition != kNoCondition) {
                        return recognized_error(
                            SourceFormat::MisterZip, source_name,
                            "A MiSTer .gg condition has an invalid depth.");
                    }
                    const auto operation = decode_record(
                        *record_flags, *address, *value, true);
                    if (!operation) {
                        return recognized_error(
                            SourceFormat::MisterZip, source_name,
                            "A MiSTer .gg condition uses an unsupported mask.");
                    }
                    entry.operations.push_back(*operation);
                    active_condition = entry.operations.size() - 1U;
                    continue;
                }
                if (*depth == 0U) {
                    if (active_condition != kNoCondition &&
                        entry.operations[active_condition].condition_span == 0U) {
                        return recognized_error(
                            SourceFormat::MisterZip, source_name,
                            "A MiSTer .gg condition controls no writes.");
                    }
                    active_condition = kNoCondition;
                } else if (*depth == 1U) {
                    if (active_condition == kNoCondition) {
                        return recognized_error(
                            SourceFormat::MisterZip, source_name,
                            "A MiSTer .gg depth-one write has no condition.");
                    }
                } else {
                    return recognized_error(
                        SourceFormat::MisterZip, source_name,
                        "A MiSTer .gg record uses an unsupported depth.");
                }
                const auto operation = decode_record(
                    *record_flags, *address, *value, false);
                if (!operation) {
                    return recognized_error(
                        SourceFormat::MisterZip, source_name,
                        "A MiSTer .gg write uses an unsupported mask.");
                }
                entry.operations.push_back(*operation);
                if (*depth == 1U) {
                    ++entry.operations[active_condition].condition_span;
                }
            }
            if (active_condition != kNoCondition &&
                entry.operations[active_condition].condition_span == 0U) {
                return recognized_error(
                    SourceFormat::MisterZip, source_name,
                    "A MiSTer .gg condition controls no writes.");
            }
            document.entries.push_back(std::move(entry));
        }
        offset = payload_offset + payload_size;
    }

    return render_document(
        SourceFormat::MisterZip, source_name, document,
        {InputFormat::FcdRaw, InputFormat::ActionReplayMaxRaw,
         InputFormat::EzFlash});
}

} // namespace gba::native_input::detail
