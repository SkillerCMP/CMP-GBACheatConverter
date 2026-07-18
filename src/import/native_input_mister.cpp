#include "import/native_input_internal.hpp"

#include "core/text.hpp"
#include "formats/mister_gg.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::native_input::detail {
namespace {

constexpr std::uint32_t kLocalHeader = 0x04034B50U;
constexpr std::uint32_t kCentralHeader = 0x02014B50U;
constexpr std::uint32_t kEndOfCentralDirectory = 0x06054B50U;
constexpr std::size_t kMaxEocdSearch = 22U + 0xFFFFU;

std::uint32_t crc32(const std::vector<std::uint8_t>& data) {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::uint8_t byte : data) {
        crc ^= byte;
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^
                (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

bool has_gg_extension(std::string_view name) {
    if (name.size() < 3U) return false;
    std::string extension(name.substr(name.size() - 3U));
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".gg";
}

std::string cheat_name(std::string_view path, std::size_t fallback_index) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t begin = slash == std::string_view::npos ? 0U : slash + 1U;
    std::size_t end = path.size();
    if (end >= 3U && has_gg_extension(path)) end -= 3U;
    std::string result = text::trim(path.substr(begin, end - begin));
    if (result.empty()) result = "Cheat " + std::to_string(fallback_index + 1U);
    return result;
}

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size)
        : data_(data), size_(size) {}

    bool read(unsigned count, std::uint32_t& value) {
        if (count > 24U) return false;
        value = 0U;
        for (unsigned bit = 0U; bit < count; ++bit) {
            if (bit_position_ >= size_ * 8U) return false;
            const std::size_t byte_index = bit_position_ / 8U;
            const unsigned bit_index = static_cast<unsigned>(bit_position_ % 8U);
            const std::uint32_t next =
                (static_cast<std::uint32_t>(data_[byte_index]) >> bit_index) & 1U;
            value |= next << bit;
            ++bit_position_;
        }
        return true;
    }

    bool align_byte() {
        const std::size_t aligned = (bit_position_ + 7U) & ~std::size_t{7U};
        if (aligned > size_ * 8U) return false;
        bit_position_ = aligned;
        return true;
    }

    bool read_aligned_u16(std::uint16_t& value) {
        if ((bit_position_ & 7U) != 0U) return false;
        const std::size_t offset = bit_position_ / 8U;
        if (offset > size_ || size_ - offset < 2U) return false;
        value = static_cast<std::uint16_t>(data_[offset]) |
            static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(data_[offset + 1U]) << 8U);
        bit_position_ += 16U;
        return true;
    }

    bool read_aligned_bytes(std::vector<std::uint8_t>& output,
                            std::size_t count,
                            std::size_t expected_size) {
        if ((bit_position_ & 7U) != 0U) return false;
        const std::size_t offset = bit_position_ / 8U;
        if (offset > size_ || size_ - offset < count) return false;
        if (output.size() > expected_size ||
            count > expected_size - output.size()) {
            return false;
        }
        output.insert(output.end(), data_ + offset, data_ + offset + count);
        bit_position_ += count * 8U;
        return true;
    }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0U;
    std::size_t bit_position_ = 0U;
};

class HuffmanTable {
public:
    bool build(const std::vector<std::uint8_t>& lengths,
               unsigned maximum_bits) {
        if (maximum_bits > 15U) return false;
        for (auto& codes : codes_by_length_) codes.clear();
        std::array<unsigned, 16U> counts{};
        bool any = false;
        for (std::uint8_t length : lengths) {
            if (length > maximum_bits) return false;
            if (length != 0U) {
                ++counts[length];
                any = true;
            }
        }
        if (!any) return false;

        std::array<unsigned, 16U> next_code{};
        unsigned code = 0U;
        for (unsigned bits = 1U; bits <= maximum_bits; ++bits) {
            code = (code + counts[bits - 1U]) << 1U;
            next_code[bits] = code;
            if (code + counts[bits] > (1U << bits)) return false;
        }

        for (std::size_t symbol = 0U; symbol < lengths.size(); ++symbol) {
            const unsigned length = lengths[symbol];
            if (length == 0U) continue;
            const unsigned canonical = next_code[length]++;
            unsigned reversed = 0U;
            for (unsigned bit = 0U; bit < length; ++bit) {
                reversed = (reversed << 1U) |
                    ((canonical >> bit) & 1U);
            }
            codes_by_length_[length].push_back({
                reversed, static_cast<unsigned>(symbol)
            });
        }
        maximum_bits_ = maximum_bits;
        return true;
    }

    bool decode(BitReader& reader, unsigned& symbol) const {
        unsigned code = 0U;
        for (unsigned length = 1U; length <= maximum_bits_; ++length) {
            std::uint32_t bit = 0U;
            if (!reader.read(1U, bit)) return false;
            code |= static_cast<unsigned>(bit) << (length - 1U);
            for (const Code& candidate : codes_by_length_[length]) {
                if (candidate.bits == code) {
                    symbol = candidate.symbol;
                    return true;
                }
            }
        }
        return false;
    }

private:
    struct Code {
        unsigned bits = 0U;
        unsigned symbol = 0U;
    };
    std::array<std::vector<Code>, 16U> codes_by_length_{};
    unsigned maximum_bits_ = 0U;
};

bool fixed_tables(HuffmanTable& literals, HuffmanTable& distances) {
    std::vector<std::uint8_t> literal_lengths(288U, 0U);
    for (unsigned index = 0U; index <= 143U; ++index) literal_lengths[index] = 8U;
    for (unsigned index = 144U; index <= 255U; ++index) literal_lengths[index] = 9U;
    for (unsigned index = 256U; index <= 279U; ++index) literal_lengths[index] = 7U;
    for (unsigned index = 280U; index <= 287U; ++index) literal_lengths[index] = 8U;
    std::vector<std::uint8_t> distance_lengths(32U, 5U);
    return literals.build(literal_lengths, 15U) &&
        distances.build(distance_lengths, 15U);
}

bool dynamic_tables(BitReader& reader,
                    HuffmanTable& literals,
                    HuffmanTable& distances) {
    std::uint32_t hlit_bits = 0U;
    std::uint32_t hdist_bits = 0U;
    std::uint32_t hclen_bits = 0U;
    if (!reader.read(5U, hlit_bits) || !reader.read(5U, hdist_bits) ||
        !reader.read(4U, hclen_bits)) {
        return false;
    }
    const unsigned hlit = static_cast<unsigned>(hlit_bits) + 257U;
    const unsigned hdist = static_cast<unsigned>(hdist_bits) + 1U;
    const unsigned hclen = static_cast<unsigned>(hclen_bits) + 4U;
    if (hlit > 286U || hdist > 32U) return false;

    constexpr std::array<unsigned, 19U> order{
        16U, 17U, 18U, 0U, 8U, 7U, 9U, 6U, 10U, 5U,
        11U, 4U, 12U, 3U, 13U, 2U, 14U, 1U, 15U
    };
    std::vector<std::uint8_t> code_lengths(19U, 0U);
    for (unsigned index = 0U; index < hclen; ++index) {
        std::uint32_t length = 0U;
        if (!reader.read(3U, length)) return false;
        code_lengths[order[index]] = static_cast<std::uint8_t>(length);
    }
    HuffmanTable code_table;
    if (!code_table.build(code_lengths, 7U)) return false;

    std::vector<std::uint8_t> lengths;
    lengths.reserve(static_cast<std::size_t>(hlit + hdist));
    while (lengths.size() < static_cast<std::size_t>(hlit + hdist)) {
        unsigned symbol = 0U;
        if (!code_table.decode(reader, symbol)) return false;
        if (symbol <= 15U) {
            lengths.push_back(static_cast<std::uint8_t>(symbol));
            continue;
        }
        std::uint32_t extra = 0U;
        unsigned repeat = 0U;
        std::uint8_t repeated_length = 0U;
        if (symbol == 16U) {
            if (lengths.empty() || !reader.read(2U, extra)) return false;
            repeat = static_cast<unsigned>(extra) + 3U;
            repeated_length = lengths.back();
        } else if (symbol == 17U) {
            if (!reader.read(3U, extra)) return false;
            repeat = static_cast<unsigned>(extra) + 3U;
        } else if (symbol == 18U) {
            if (!reader.read(7U, extra)) return false;
            repeat = static_cast<unsigned>(extra) + 11U;
        } else {
            return false;
        }
        if (repeat > static_cast<std::size_t>(hlit + hdist) - lengths.size()) {
            return false;
        }
        lengths.insert(lengths.end(), repeat, repeated_length);
    }

    const std::vector<std::uint8_t> literal_lengths(
        lengths.begin(), lengths.begin() + static_cast<std::ptrdiff_t>(hlit));
    const std::vector<std::uint8_t> distance_lengths(
        lengths.begin() + static_cast<std::ptrdiff_t>(hlit), lengths.end());
    return literals.build(literal_lengths, 15U) &&
        distances.build(distance_lengths, 15U);
}

bool inflate_codes(BitReader& reader,
                   const HuffmanTable& literals,
                   const HuffmanTable& distances,
                   std::vector<std::uint8_t>& output,
                   std::size_t expected_size) {
    constexpr std::array<unsigned, 29U> length_base{
        3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 13U,
        15U, 17U, 19U, 23U, 27U, 31U, 35U, 43U, 51U, 59U,
        67U, 83U, 99U, 115U, 131U, 163U, 195U, 227U, 258U
    };
    constexpr std::array<unsigned, 29U> length_extra{
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 1U,
        1U, 1U, 2U, 2U, 2U, 2U, 3U, 3U, 3U, 3U,
        4U, 4U, 4U, 4U, 5U, 5U, 5U, 5U, 0U
    };
    constexpr std::array<unsigned, 30U> distance_base{
        1U, 2U, 3U, 4U, 5U, 7U, 9U, 13U, 17U, 25U,
        33U, 49U, 65U, 97U, 129U, 193U, 257U, 385U, 513U, 769U,
        1025U, 1537U, 2049U, 3073U, 4097U, 6145U, 8193U, 12289U,
        16385U, 24577U
    };
    constexpr std::array<unsigned, 30U> distance_extra{
        0U, 0U, 0U, 0U, 1U, 1U, 2U, 2U, 3U, 3U,
        4U, 4U, 5U, 5U, 6U, 6U, 7U, 7U, 8U, 8U,
        9U, 9U, 10U, 10U, 11U, 11U, 12U, 12U, 13U, 13U
    };

    for (;;) {
        unsigned symbol = 0U;
        if (!literals.decode(reader, symbol)) return false;
        if (symbol < 256U) {
            if (output.size() >= expected_size) return false;
            output.push_back(static_cast<std::uint8_t>(symbol));
            continue;
        }
        if (symbol == 256U) return true;
        if (symbol < 257U || symbol > 285U) return false;

        const unsigned length_index = symbol - 257U;
        std::uint32_t extra = 0U;
        if (!reader.read(length_extra[length_index], extra)) return false;
        const std::size_t length = static_cast<std::size_t>(
            length_base[length_index] + static_cast<unsigned>(extra));

        unsigned distance_symbol = 0U;
        if (!distances.decode(reader, distance_symbol) ||
            distance_symbol >= distance_base.size()) {
            return false;
        }
        extra = 0U;
        if (!reader.read(distance_extra[distance_symbol], extra)) return false;
        const std::size_t distance = static_cast<std::size_t>(
            distance_base[distance_symbol] + static_cast<unsigned>(extra));
        if (output.size() > expected_size || distance == 0U ||
            distance > output.size() ||
            length > expected_size - output.size()) {
            return false;
        }
        for (std::size_t index = 0U; index < length; ++index) {
            output.push_back(output[output.size() - distance]);
        }
    }
}

std::optional<std::vector<std::uint8_t>> inflate_raw(
    const std::uint8_t* data,
    std::size_t size,
    std::size_t expected_size) {
    BitReader reader(data, size);
    std::vector<std::uint8_t> output;
    output.reserve(expected_size);
    bool final_block = false;
    while (!final_block) {
        std::uint32_t final = 0U;
        std::uint32_t type = 0U;
        if (!reader.read(1U, final) || !reader.read(2U, type)) {
            return std::nullopt;
        }
        final_block = final != 0U;
        if (type == 0U) {
            if (!reader.align_byte()) return std::nullopt;
            std::uint16_t length = 0U;
            std::uint16_t inverse = 0U;
            if (!reader.read_aligned_u16(length) ||
                !reader.read_aligned_u16(inverse) ||
                static_cast<std::uint16_t>(length ^ 0xFFFFU) != inverse ||
                !reader.read_aligned_bytes(output, length, expected_size)) {
                return std::nullopt;
            }
            continue;
        }
        if (type == 3U) return std::nullopt;

        HuffmanTable literals;
        HuffmanTable distances;
        const bool tables_ok = type == 1U
            ? fixed_tables(literals, distances)
            : dynamic_tables(reader, literals, distances);
        if (!tables_ok || !inflate_codes(
                reader, literals, distances, output, expected_size)) {
            return std::nullopt;
        }
    }
    if (output.size() != expected_size) return std::nullopt;
    return output;
}

struct ExtractedEntry {
    std::string name;
    std::vector<std::uint8_t> data;
};

struct ZipResult {
    bool is_zip = false;
    bool success = false;
    bool found_gg = false;
    std::vector<ExtractedEntry> entries;
    std::string error;
};

std::optional<std::size_t> find_eocd(const std::vector<std::uint8_t>& data) {
    if (data.size() < 22U) return std::nullopt;
    const std::size_t minimum = data.size() > kMaxEocdSearch
        ? data.size() - kMaxEocdSearch : 0U;
    for (std::size_t position = data.size() - 22U;; --position) {
        const auto signature = read_u32(data, position);
        if (signature && *signature == kEndOfCentralDirectory) {
            const auto comment_size = read_u16(data, position + 20U);
            if (comment_size && position + 22U + *comment_size == data.size()) {
                return position;
            }
        }
        if (position == minimum) break;
    }
    return std::nullopt;
}

ZipResult extract_gg_entries(const std::vector<std::uint8_t>& data) {
    ZipResult result;
    const auto eocd_offset = find_eocd(data);
    if (!eocd_offset) return result;
    result.is_zip = true;

    const auto disk = read_u16(data, *eocd_offset + 4U);
    const auto central_disk = read_u16(data, *eocd_offset + 6U);
    const auto disk_entries = read_u16(data, *eocd_offset + 8U);
    const auto total_entries = read_u16(data, *eocd_offset + 10U);
    const auto central_size = read_u32(data, *eocd_offset + 12U);
    const auto central_offset = read_u32(data, *eocd_offset + 16U);
    if (!disk || !central_disk || !disk_entries || !total_entries ||
        !central_size || !central_offset) {
        result.error = "The ZIP end-of-central-directory record is truncated.";
        return result;
    }
    if (*disk != 0U || *central_disk != 0U ||
        *disk_entries != *total_entries) {
        result.error = "Multi-disk ZIP archives are not supported.";
        return result;
    }
    if (*total_entries == 0xFFFFU || *central_size == 0xFFFFFFFFU ||
        *central_offset == 0xFFFFFFFFU) {
        result.error = "ZIP64 MiSTer archives are not supported.";
        return result;
    }
    const std::size_t central_begin = *central_offset;
    const std::size_t central_bytes = *central_size;
    if (central_begin > data.size() ||
        central_bytes > data.size() - central_begin ||
        central_begin + central_bytes > *eocd_offset) {
        result.error = "The ZIP central directory exceeds the file size.";
        return result;
    }

    std::size_t offset = central_begin;
    for (std::uint16_t index = 0U; index < *total_entries; ++index) {
        const auto signature = read_u32(data, offset);
        if (!signature || *signature != kCentralHeader ||
            offset > data.size() || data.size() - offset < 46U) {
            result.error = "The ZIP central directory is malformed.";
            return result;
        }
        const auto flags = read_u16(data, offset + 8U);
        const auto method = read_u16(data, offset + 10U);
        const auto expected_crc = read_u32(data, offset + 16U);
        const auto compressed_size = read_u32(data, offset + 20U);
        const auto uncompressed_size = read_u32(data, offset + 24U);
        const auto name_size = read_u16(data, offset + 28U);
        const auto extra_size = read_u16(data, offset + 30U);
        const auto comment_size = read_u16(data, offset + 32U);
        const auto start_disk = read_u16(data, offset + 34U);
        const auto local_offset = read_u32(data, offset + 42U);
        if (!flags || !method || !expected_crc || !compressed_size ||
            !uncompressed_size || !name_size || !extra_size ||
            !comment_size || !start_disk || !local_offset) {
            result.error = "A ZIP central-directory entry is truncated.";
            return result;
        }
        const std::size_t variable_size = static_cast<std::size_t>(*name_size) +
            static_cast<std::size_t>(*extra_size) +
            static_cast<std::size_t>(*comment_size);
        if (46U + variable_size > data.size() - offset) {
            result.error = "A ZIP central-directory name is truncated.";
            return result;
        }
        const std::string name(
            data.begin() + static_cast<std::ptrdiff_t>(offset + 46U),
            data.begin() + static_cast<std::ptrdiff_t>(
                offset + 46U + *name_size));
        offset += 46U + variable_size;

        if (!has_gg_extension(name) ||
            (!name.empty() && (name.back() == '/' || name.back() == '\\'))) {
            continue;
        }
        result.found_gg = true;
        if ((*flags & 0x0001U) != 0U) {
            result.error = "Encrypted MiSTer ZIP entries are not supported.";
            return result;
        }
        if (*method != 0U && *method != 8U) {
            result.error =
                "MiSTer ZIP entries must use Stored or Deflate compression.";
            return result;
        }
        if (*start_disk != 0U || *compressed_size == 0xFFFFFFFFU ||
            *uncompressed_size == 0xFFFFFFFFU) {
            result.error = "A MiSTer ZIP entry requires unsupported ZIP64 data.";
            return result;
        }
        if (*uncompressed_size == 0U ||
            (*uncompressed_size % mister_gg::kRecordSize) != 0U ||
            *uncompressed_size >
                mister_gg::kGbaMaxRecords * mister_gg::kRecordSize) {
            result.error =
                "A MiSTer .gg entry has an invalid size or exceeds 32 records.";
            return result;
        }

        const std::size_t local = *local_offset;
        const auto local_signature = read_u32(data, local);
        if (!local_signature || *local_signature != kLocalHeader ||
            local > data.size() || data.size() - local < 30U) {
            result.error = "A MiSTer ZIP local header is missing.";
            return result;
        }
        const auto local_flags = read_u16(data, local + 6U);
        const auto local_method = read_u16(data, local + 8U);
        const auto local_crc = read_u32(data, local + 14U);
        const auto local_compressed_size = read_u32(data, local + 18U);
        const auto local_uncompressed_size = read_u32(data, local + 22U);
        const auto local_name_size = read_u16(data, local + 26U);
        const auto local_extra_size = read_u16(data, local + 28U);
        if (!local_flags || !local_method || !local_crc ||
            !local_compressed_size || !local_uncompressed_size ||
            !local_name_size || !local_extra_size) {
            result.error = "A MiSTer ZIP local header is truncated.";
            return result;
        }
        if (*local_method != *method || ((*local_flags ^ *flags) & 0x0001U)) {
            result.error =
                "A MiSTer ZIP local header disagrees with its central entry.";
            return result;
        }
        const std::size_t local_variable_size =
            static_cast<std::size_t>(*local_name_size) +
            static_cast<std::size_t>(*local_extra_size);
        if (local_variable_size > data.size() - (local + 30U)) {
            result.error = "A MiSTer ZIP local name is truncated.";
            return result;
        }
        const std::string local_name(
            data.begin() + static_cast<std::ptrdiff_t>(local + 30U),
            data.begin() + static_cast<std::ptrdiff_t>(
                local + 30U + *local_name_size));
        if (local_name != name) {
            result.error =
                "A MiSTer ZIP local filename disagrees with its central entry.";
            return result;
        }
        const bool uses_descriptor = (*local_flags & 0x0008U) != 0U;
        if (!uses_descriptor &&
            (*local_crc != *expected_crc ||
             *local_compressed_size != *compressed_size ||
             *local_uncompressed_size != *uncompressed_size)) {
            result.error =
                "A MiSTer ZIP local size or CRC disagrees with its central entry.";
            return result;
        }
        const std::size_t payload_offset = local + 30U + local_variable_size;
        if (payload_offset > data.size() ||
            *compressed_size > data.size() - payload_offset) {
            result.error = "A MiSTer ZIP payload is truncated.";
            return result;
        }

        std::vector<std::uint8_t> payload;
        if (*method == 0U) {
            if (*compressed_size != *uncompressed_size) {
                result.error = "A Stored MiSTer ZIP entry has mismatched sizes.";
                return result;
            }
            payload.assign(
                data.begin() + static_cast<std::ptrdiff_t>(payload_offset),
                data.begin() + static_cast<std::ptrdiff_t>(
                    payload_offset + *compressed_size));
        } else {
            const auto inflated = inflate_raw(
                data.data() + payload_offset,
                *compressed_size,
                *uncompressed_size);
            if (!inflated) {
                result.error = "A Deflate-compressed MiSTer entry is invalid.";
                return result;
            }
            payload = *inflated;
        }
        if (crc32(payload) != *expected_crc) {
            result.error = "A MiSTer ZIP entry failed its CRC-32 check.";
            return result;
        }
        result.entries.push_back({name, std::move(payload)});
    }

    if (offset != central_begin + central_bytes) {
        result.error = "The ZIP central-directory size is inconsistent.";
        return result;
    }
    result.success = true;
    return result;
}

Result finish_mister_document(CheatDocument document,
                              std::vector<std::string> warnings,
                              std::string source_name) {
    Result result = render_document(
        SourceFormat::MisterZip, std::move(source_name), document,
        {InputFormat::FcdRaw, InputFormat::ActionReplayMaxRaw,
         InputFormat::EzFlash});
    if (result.success) {
        result.warnings.insert(result.warnings.end(),
                               warnings.begin(), warnings.end());
        result.document.warnings.insert(result.document.warnings.end(),
                                        warnings.begin(), warnings.end());
    }
    return result;
}

} // namespace

Result import_mister(std::string_view filename,
                     const std::vector<std::uint8_t>& data) {
    const std::string extension = filename_extension(filename);
    if (extension == ".gg") {
        const mister_gg::DecodeResult decoded = mister_gg::decode_entry(
            cheat_name(filename, 0U), data);
        if (!decoded.success) {
            return recognized_error(
                SourceFormat::MisterZip, "MiSTer GBA .gg", decoded.error);
        }
        CheatDocument document;
        document.entries.push_back(decoded.entry);
        return finish_mister_document(
            std::move(document), decoded.warnings, "MiSTer GBA .gg");
    }

    const ZipResult zip = extract_gg_entries(data);
    if (!zip.is_zip || !zip.found_gg) return {};
    if (!zip.success) {
        return recognized_error(
            SourceFormat::MisterZip, "MiSTer GBA .zip", zip.error);
    }

    CheatDocument document;
    std::vector<std::string> warnings;
    for (const ExtractedEntry& item : zip.entries) {
        const mister_gg::DecodeResult decoded = mister_gg::decode_entry(
            cheat_name(item.name, document.entries.size()), item.data);
        if (!decoded.success) {
            return recognized_error(
                SourceFormat::MisterZip, "MiSTer GBA .zip",
                "In '" + item.name + "': " + decoded.error);
        }
        document.entries.push_back(decoded.entry);
        warnings.insert(warnings.end(), decoded.warnings.begin(),
                        decoded.warnings.end());
    }
    return finish_mister_document(
        std::move(document), std::move(warnings), "MiSTer GBA .zip");
}

} // namespace gba::native_input::detail
