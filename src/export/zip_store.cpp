#include "export/output_modes_internal.hpp"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {
namespace {

std::uint32_t crc32(std::string_view bytes) {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (char raw_byte : bytes) {
        const auto byte = static_cast<unsigned char>(raw_byte);
        crc ^= byte;
        for (unsigned bit = 0; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (0xEDB88320U &
                (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

} // namespace

std::vector<std::uint8_t> store_zip(const std::vector<ZipItem>& items) {
    struct Central { ZipItem item; std::uint32_t crc; std::uint32_t offset; };
    std::vector<Central> central;
    std::vector<std::uint8_t> out;
    for (const ZipItem& item : items) {
        Central record{item, crc32(item.data),
                       static_cast<std::uint32_t>(out.size())};
        append_u32(out, 0x04034B50U);
        append_u16(out, 20U);
        append_u16(out, 0x0800U);
        append_u16(out, 0U);
        append_u16(out, 0U);
        append_u16(out, 0U);
        append_u32(out, record.crc);
        append_u32(out, static_cast<std::uint32_t>(item.data.size()));
        append_u32(out, static_cast<std::uint32_t>(item.data.size()));
        append_u16(out, static_cast<std::uint16_t>(item.name.size()));
        append_u16(out, 0U);
        append_bytes(out, item.name);
        append_bytes(out, item.data);
        central.push_back(std::move(record));
    }
    const std::uint32_t central_offset = static_cast<std::uint32_t>(out.size());
    for (const Central& record : central) {
        append_u32(out, 0x02014B50U);
        append_u16(out, 20U);
        append_u16(out, 20U);
        append_u16(out, 0x0800U);
        append_u16(out, 0U);
        append_u16(out, 0U);
        append_u16(out, 0U);
        append_u32(out, record.crc);
        append_u32(out, static_cast<std::uint32_t>(record.item.data.size()));
        append_u32(out, static_cast<std::uint32_t>(record.item.data.size()));
        append_u16(out, static_cast<std::uint16_t>(record.item.name.size()));
        append_u16(out, 0U);
        append_u16(out, 0U);
        append_u16(out, 0U);
        append_u16(out, 0U);
        append_u32(out, 0U);
        append_u32(out, record.offset);
        append_bytes(out, record.item.name);
    }
    const std::uint32_t central_size =
        static_cast<std::uint32_t>(out.size()) - central_offset;
    append_u32(out, 0x06054B50U);
    append_u16(out, 0U);
    append_u16(out, 0U);
    append_u16(out, static_cast<std::uint16_t>(central.size()));
    append_u16(out, static_cast<std::uint16_t>(central.size()));
    append_u32(out, central_size);
    append_u32(out, central_offset);
    append_u16(out, 0U);
    return out;
}

} // namespace gba::output_modes::detail
