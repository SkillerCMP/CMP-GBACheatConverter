#include "export/output_modes_internal.hpp"

#include "core/text.hpp"
#include "formats/vba_clt_codec.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {
namespace {

constexpr std::int32_t kUnknownCode = -1;
constexpr std::int32_t kInt8BitWrite = 0;
constexpr std::int32_t kInt16BitWrite = 1;
constexpr std::int32_t kInt32BitWrite = 2;
constexpr std::int32_t kCbaIfKeysPressed = 7;
constexpr std::int32_t kCbaIfTrue = 8;
constexpr std::int32_t kCbaSlideCode = 9;
constexpr std::int32_t kCbaIfFalse = 10;
constexpr std::int32_t kCbaAnd = 11;
constexpr std::int32_t kGsa8BitIfTrue = 19;
constexpr std::int32_t kGsa32BitIfTrue = 20;
constexpr std::int32_t kGsa8BitIfFalse = 21;
constexpr std::int32_t kGsa32BitIfFalse = 22;
constexpr std::int32_t kGsa8BitFill = 23;
constexpr std::int32_t kGsa16BitFill = 24;
constexpr std::int32_t kGsa8BitIfTrue2 = 25;
constexpr std::int32_t kGsa16BitIfTrue2 = 26;
constexpr std::int32_t kGsa32BitIfTrue2 = 27;
constexpr std::int32_t kGsa8BitIfFalse2 = 28;
constexpr std::int32_t kGsa16BitIfFalse2 = 29;
constexpr std::int32_t kGsa32BitIfFalse2 = 30;
constexpr std::int32_t kGsaSlowdown = 31;
constexpr std::int32_t kCbaAdd = 32;
constexpr std::int32_t kCbaOr = 33;
constexpr std::int32_t kCbaLt = 34;
constexpr std::int32_t kCbaGt = 35;
constexpr std::int32_t kCbaSuper = 36;
constexpr std::int32_t kGsa8BitPointer = 37;
constexpr std::int32_t kGsa16BitPointer = 38;
constexpr std::int32_t kGsa32BitPointer = 39;
constexpr std::int32_t kGsa8BitAdd = 40;
constexpr std::int32_t kGsa16BitAdd = 41;
constexpr std::int32_t kGsa32BitAdd = 42;
constexpr std::int32_t kGsa8BitIfLowerU = 43;
constexpr std::int32_t kGsa16BitIfLowerU = 44;
constexpr std::int32_t kGsa32BitIfLowerU = 45;
constexpr std::int32_t kGsa8BitIfHigherU = 46;
constexpr std::int32_t kGsa16BitIfHigherU = 47;
constexpr std::int32_t kGsa32BitIfHigherU = 48;
constexpr std::int32_t kGsa8BitIfAnd = 49;
constexpr std::int32_t kGsa16BitIfAnd = 50;
constexpr std::int32_t kGsa32BitIfAnd = 51;
constexpr std::int32_t kGsa8BitIfLowerU2 = 52;
constexpr std::int32_t kGsa16BitIfLowerU2 = 53;
constexpr std::int32_t kGsa32BitIfLowerU2 = 54;
constexpr std::int32_t kGsa8BitIfHigherU2 = 55;
constexpr std::int32_t kGsa16BitIfHigherU2 = 56;
constexpr std::int32_t kGsa32BitIfHigherU2 = 57;
constexpr std::int32_t kGsa8BitIfAnd2 = 58;
constexpr std::int32_t kGsa16BitIfAnd2 = 59;
constexpr std::int32_t kGsa32BitIfAnd2 = 60;
constexpr std::int32_t kGsaAlways = 61;
constexpr std::int32_t kGsaAlways2 = 62;
constexpr std::int32_t kGsa8BitIfLowerS = 63;
constexpr std::int32_t kGsa16BitIfLowerS = 64;
constexpr std::int32_t kGsa32BitIfLowerS = 65;
constexpr std::int32_t kGsa8BitIfHigherS = 66;
constexpr std::int32_t kGsa16BitIfHigherS = 67;
constexpr std::int32_t kGsa32BitIfHigherS = 68;
constexpr std::int32_t kGsa8BitIfLowerS2 = 69;
constexpr std::int32_t kGsa16BitIfLowerS2 = 70;
constexpr std::int32_t kGsa32BitIfLowerS2 = 71;
constexpr std::int32_t kGsa8BitIfHigherS2 = 72;
constexpr std::int32_t kGsa16BitIfHigherS2 = 73;
constexpr std::int32_t kGsa32BitIfHigherS2 = 74;
constexpr std::int32_t kGsa16BitWriteIo = 75;
constexpr std::int32_t kGsa32BitWriteIo = 76;
constexpr std::int32_t kGsaCodesOn = 77;
constexpr std::int32_t kGsa8BitIfTrue3 = 78;
constexpr std::int32_t kGsa16BitIfTrue3 = 79;
constexpr std::int32_t kGsa32BitIfTrue3 = 80;
constexpr std::int32_t kGsa8BitIfFalse3 = 81;
constexpr std::int32_t kGsa16BitIfFalse3 = 82;
constexpr std::int32_t kGsa32BitIfFalse3 = 83;
constexpr std::int32_t kGsa8BitIfLowerS3 = 84;
constexpr std::int32_t kGsa16BitIfLowerS3 = 85;
constexpr std::int32_t kGsa32BitIfLowerS3 = 86;
constexpr std::int32_t kGsa8BitIfHigherS3 = 87;
constexpr std::int32_t kGsa16BitIfHigherS3 = 88;
constexpr std::int32_t kGsa32BitIfHigherS3 = 89;
constexpr std::int32_t kGsa8BitIfLowerU3 = 90;
constexpr std::int32_t kGsa16BitIfLowerU3 = 91;
constexpr std::int32_t kGsa32BitIfLowerU3 = 92;
constexpr std::int32_t kGsa8BitIfHigherU3 = 93;
constexpr std::int32_t kGsa16BitIfHigherU3 = 94;
constexpr std::int32_t kGsa32BitIfHigherU3 = 95;
constexpr std::int32_t kGsa8BitIfAnd3 = 96;
constexpr std::int32_t kGsa16BitIfAnd3 = 97;
constexpr std::int32_t kGsa32BitIfAnd3 = 98;
constexpr std::int32_t kGsaAlways3 = 99;
constexpr std::int32_t kGsa16BitRomPatch2C = 15;
constexpr std::int32_t kGsa16BitRomPatch2D = 100;
constexpr std::int32_t kGsa16BitRomPatch2E = 101;
constexpr std::int32_t kGsa16BitRomPatch2F = 102;
constexpr std::int32_t kMasterCode = 112;

using VbaRecord = vba_clt::Record;

VbaRecord make_record(std::int32_t family,
                      std::int32_t size,
                      std::uint32_t raw_address,
                      std::uint32_t address,
                      std::uint32_t value,
                      std::string code_string,
                      std::string description) {
    VbaRecord record;
    record.code = family;
    record.size = size;
    record.raw_address = raw_address;
    record.address = address;
    record.value = value;
    record.code_string = std::move(code_string);
    record.description = std::move(description);
    return record;
}

bool previous_has_cba_data(const std::vector<VbaRecord>& records) {
    if (records.empty()) return false;
    return records.back().size == kCbaSlideCode ||
           records.back().size == kCbaSuper;
}

std::uint32_t cba_super_data_lines(std::uint32_t value) {
    return (((value - 1U) & 0xFFFFU) / 3U) + 1U;
}

bool append_cba_records(const Lines8x4& lines,
                        std::string_view name,
                        std::vector<VbaRecord>& records) {
    std::uint32_t super_remaining = 0U;
    for (const auto& line : lines) {
        const std::uint32_t raw_address = line.first;
        const std::uint32_t raw_value = line.second;
        const std::string code_string =
            text::hex(raw_address, 8U) + " " + text::hex(raw_value, 4U);
        const std::string description = single_line_name(name);

        if (previous_has_cba_data(records) || super_remaining > 0U) {
            records.push_back(make_record(
                512, kUnknownCode, raw_address, raw_address, raw_value,
                code_string, description));
            if (super_remaining > 0U) --super_remaining;
            continue;
        }

        const std::uint32_t type = raw_address >> 28U;
        std::int32_t size = kUnknownCode;
        std::uint32_t address = raw_address;
        std::uint32_t value = raw_value;
        switch (type) {
        case 0x0U:
            address = raw_address & 0x0FFFFFFFU;
            break;
        case 0x1U:
            size = kMasterCode;
            address = (raw_address & 0x01FFFFFFU) | 0x08000000U;
            break;
        case 0x2U:
            size = kCbaOr;
            address = raw_address & 0x0FFFFFFEU;
            break;
        case 0x3U:
            size = kInt8BitWrite;
            address = raw_address & 0x0FFFFFFFU;
            break;
        case 0x4U:
            size = kCbaSlideCode;
            address = raw_address & 0x0FFFFFFEU;
            break;
        case 0x5U:
            size = kCbaSuper;
            address = raw_address & 0x0FFFFFFEU;
            super_remaining = cba_super_data_lines(raw_value);
            break;
        case 0x6U:
            size = kCbaAnd;
            address = raw_address & 0x0FFFFFFEU;
            break;
        case 0x7U:
            size = kCbaIfTrue;
            address = raw_address & 0x0FFFFFFEU;
            break;
        case 0x8U:
            size = kInt16BitWrite;
            address = raw_address & 0x0FFFFFFEU;
            break;
        case 0xAU:
            size = kCbaIfFalse;
            address = raw_address & 0x0FFFFFFEU;
            break;
        case 0xBU:
            size = kCbaGt;
            address = raw_address & 0x0FFFFFFEU;
            break;
        case 0xCU:
            size = kCbaLt;
            address = raw_address & 0x0FFFFFFEU;
            break;
        case 0xDU:
            if ((raw_address & 0xF0U) >= 0x30U) return false;
            size = kCbaIfKeysPressed;
            address = raw_address & 0xF0U;
            break;
        case 0xEU:
            size = kCbaAdd;
            address = raw_address & 0x0FFFFFFFU;
            if ((value & 0x8000U) != 0U) value |= 0xFFFF0000U;
            break;
        case 0xFU:
            size = kGsa16BitIfAnd;
            address = raw_address & 0x0FFFFFFEU;
            break;
        default:
            break;
        }
        records.push_back(make_record(
            512, size, raw_address, address, value,
            code_string, description));
    }
    return super_remaining == 0U;
}

std::int32_t par_v3_size(std::uint32_t type) {
    switch (type) {
    case 0x04U: return kGsa8BitIfTrue;
    case 0x05U: return kCbaIfTrue;
    case 0x06U: return kGsa32BitIfTrue;
    case 0x07U: return kGsaAlways;
    case 0x08U: return kGsa8BitIfFalse;
    case 0x09U: return kCbaIfFalse;
    case 0x0AU: return kGsa32BitIfFalse;
    case 0x0CU: return kGsa8BitIfLowerS;
    case 0x0DU: return kGsa16BitIfLowerS;
    case 0x0EU: return kGsa32BitIfLowerS;
    case 0x10U: return kGsa8BitIfHigherS;
    case 0x11U: return kGsa16BitIfHigherS;
    case 0x12U: return kGsa32BitIfHigherS;
    case 0x14U: return kGsa8BitIfLowerU;
    case 0x15U: return kGsa16BitIfLowerU;
    case 0x16U: return kGsa32BitIfLowerU;
    case 0x18U: return kGsa8BitIfHigherU;
    case 0x19U: return kGsa16BitIfHigherU;
    case 0x1AU: return kGsa32BitIfHigherU;
    case 0x1CU: return kGsa8BitIfAnd;
    case 0x1DU: return kGsa16BitIfAnd;
    case 0x1EU: return kGsa32BitIfAnd;
    case 0x20U: return kGsa8BitPointer;
    case 0x21U: return kGsa16BitPointer;
    case 0x22U: return kGsa32BitPointer;
    case 0x24U: return kGsa8BitIfTrue2;
    case 0x25U: return kGsa16BitIfTrue2;
    case 0x26U: return kGsa32BitIfTrue2;
    case 0x27U: return kGsaAlways2;
    case 0x28U: return kGsa8BitIfFalse2;
    case 0x29U: return kGsa16BitIfFalse2;
    case 0x2AU: return kGsa32BitIfFalse2;
    case 0x2CU: return kGsa8BitIfLowerS2;
    case 0x2DU: return kGsa16BitIfLowerS2;
    case 0x2EU: return kGsa32BitIfLowerS2;
    case 0x30U: return kGsa8BitIfHigherS2;
    case 0x31U: return kGsa16BitIfHigherS2;
    case 0x32U: return kGsa32BitIfHigherS2;
    case 0x34U: return kGsa8BitIfLowerU2;
    case 0x35U: return kGsa16BitIfLowerU2;
    case 0x36U: return kGsa32BitIfLowerU2;
    case 0x38U: return kGsa8BitIfHigherU2;
    case 0x39U: return kGsa16BitIfHigherU2;
    case 0x3AU: return kGsa32BitIfHigherU2;
    case 0x3CU: return kGsa8BitIfAnd2;
    case 0x3DU: return kGsa16BitIfAnd2;
    case 0x3EU: return kGsa32BitIfAnd2;
    case 0x40U: return kGsa8BitAdd;
    case 0x41U: return kGsa16BitAdd;
    case 0x42U: return kGsa32BitAdd;
    case 0x44U: return kGsa8BitIfTrue3;
    case 0x45U: return kGsa16BitIfTrue3;
    case 0x46U: return kGsa32BitIfTrue3;
    case 0x47U: return kGsaAlways3;
    case 0x48U: return kGsa8BitIfFalse3;
    case 0x49U: return kGsa16BitIfFalse3;
    case 0x4AU: return kGsa32BitIfFalse3;
    case 0x4CU: return kGsa8BitIfLowerS3;
    case 0x4DU: return kGsa16BitIfLowerS3;
    case 0x4EU: return kGsa32BitIfLowerS3;
    case 0x50U: return kGsa8BitIfHigherS3;
    case 0x51U: return kGsa16BitIfHigherS3;
    case 0x52U: return kGsa32BitIfHigherS3;
    case 0x54U: return kGsa8BitIfLowerU3;
    case 0x55U: return kGsa16BitIfLowerU3;
    case 0x56U: return kGsa32BitIfLowerU3;
    case 0x58U: return kGsa8BitIfHigherU3;
    case 0x59U: return kGsa16BitIfHigherU3;
    case 0x5AU: return kGsa32BitIfHigherU3;
    case 0x5CU: return kGsa8BitIfAnd3;
    case 0x5DU: return kGsa16BitIfAnd3;
    case 0x5EU: return kGsa32BitIfAnd3;
    case 0x63U: return kGsa16BitWriteIo;
    case 0xE3U: return kGsa32BitWriteIo;
    default: return kUnknownCode;
    }
}

bool append_par_v3_records(const Lines8x8& raw_lines,
                           const Lines8x8& encrypted_lines,
                           std::string_view name,
                           std::vector<VbaRecord>& records) {
    if (raw_lines.size() != encrypted_lines.size()) return false;
    for (std::size_t index = 0U; index < raw_lines.size(); ++index) {
        const std::uint32_t raw_address = raw_lines[index].first;
        const std::uint32_t raw_value = raw_lines[index].second;
        const std::string code_string =
            text::hex(encrypted_lines[index].first, 8U) +
            text::hex(encrypted_lines[index].second, 8U);
        const std::string description = single_line_name(name);
        const std::uint32_t mcode = (raw_address >> 24U) & 0xFFU;
        if ((mcode & 0xFEU) == 0xC4U) {
            records.push_back(make_record(
                257, kMasterCode, raw_address,
                (raw_address & 0x01FFFFFFU) | 0x08000000U,
                raw_value, code_string, description));
            continue;
        }

        const std::uint32_t type =
            ((raw_address >> 25U) & 0x7FU) |
            ((raw_address >> 17U) & 0x80U);
        const std::uint32_t decoded_address =
            ((raw_address & 0x00F00000U) << 4U) |
            (raw_address & 0x0003FFFFU);
        std::int32_t size = kUnknownCode;
        std::uint32_t address = raw_address;
        std::uint32_t value = raw_value;

        if (type == 0U) {
            if (raw_address == 0U) {
                const std::uint32_t embedded_type = (raw_value >> 25U) & 0x7FU;
                const std::uint32_t embedded_address =
                    ((raw_value & 0x00F00000U) << 4U) |
                    (raw_value & 0x0003FFFFU);
                address = 0U;
                switch (embedded_type) {
                case 0x04U: size = kGsaSlowdown; value = raw_value & 0x00FFFFFFU; break;
                case 0x08U: size = 12; value = embedded_address; break;
                case 0x09U: size = 13; value = embedded_address; break;
                case 0x0AU: size = 14; value = embedded_address; break;
                case 0x0CU: size = kGsa16BitRomPatch2C; value = raw_value & 0x00FFFFFFU; break;
                case 0x0DU: size = kGsa16BitRomPatch2D; value = raw_value & 0x00FFFFFFU; break;
                case 0x0EU: size = kGsa16BitRomPatch2E; value = raw_value & 0x00FFFFFFU; break;
                case 0x0FU: size = kGsa16BitRomPatch2F; value = raw_value & 0x00FFFFFFU; break;
                case 0x20U: size = kGsaCodesOn; value = embedded_address; break;
                case 0x40U: size = 16; value = embedded_address; break;
                case 0x41U: size = 17; value = embedded_address; break;
                case 0x42U: size = 18; value = embedded_address; break;
                default: address = raw_address; value = raw_value; break;
                }
            } else {
                size = kGsa8BitFill;
                address = decoded_address;
            }
        } else if (type == 0x01U) {
            size = kGsa16BitFill;
            address = decoded_address;
        } else if (type == 0x02U) {
            size = kInt32BitWrite;
            address = decoded_address;
        } else {
            size = par_v3_size(type);
            address = decoded_address;
        }

        records.push_back(make_record(
            257, size, raw_address, address, value,
            code_string, description));
    }
    return true;
}


} // namespace

Result export_vba_clt(const CheatDocument& document) {
    Result result;
    std::vector<VbaRecord> records;
    for (const CheatEntry& entry : document.entries) {
        std::vector<VbaRecord> converted;
        bool represented = false;
        if (const auto fcd = exact_fcd(entry)) {
            represented = append_cba_records(*fcd, entry.name, converted);
        }
        if (!represented) {
            converted.clear();
            const auto raw = exact_armax(entry, false);
            const auto encrypted = exact_armax(entry, true);
            if (raw && encrypted) {
                represented = append_par_v3_records(
                    *raw, *encrypted, entry.name, converted);
            }
        }
        if (!represented || converted.empty()) {
            warn_omitted(result, entry, "VisualBoy Advance-M .clt");
            continue;
        }
        if (converted.size() > vba_clt::kMaximumRecords - records.size()) {
            result.warnings.push_back(
                "VisualBoy Advance-M .clt supports at most 16,384 records; "
                "remaining cheats were omitted.");
            break;
        }
        result.exported_records += converted.size();
        ++result.exported_entries;
        records.insert(records.end(), converted.begin(), converted.end());
    }

    result.data = vba_clt::encode_current(records);
    result.success = !records.empty() || document.entries.empty();
    return result;
}

} // namespace gba::output_modes::detail
