#include "test_support.hpp"

#include "formats/vba_clt_codec.hpp"

#include <cstring>

namespace gba::tests {
namespace {

void put_u32(std::vector<std::uint8_t>& data,
             std::size_t offset,
             std::uint32_t value) {
    data[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    data[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    data[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    data[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

std::uint32_t get_u32(const std::vector<std::uint8_t>& data,
                      std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

void put_text(std::vector<std::uint8_t>& data,
              std::size_t offset,
              std::size_t capacity,
              std::string_view value) {
    const std::size_t count = std::min(capacity - 1U, value.size());
    std::memcpy(data.data() + offset, value.data(), count);
}

std::vector<std::uint8_t> source_type1_fixture() {
    // This is written directly from VBA-M CheatsData offsets, not through the
    // converter codec: int/int/int/bool+padding/raw/address/value/old/string/desc.
    std::vector<std::uint8_t> data(
        vba_clt::kHeaderSize + 2U * vba_clt::kCurrentRecordSize, 0U);
    put_u32(data, 0U, 1U);
    put_u32(data, 4U, 1U);
    put_u32(data, 8U, 2U);

    std::size_t record = vba_clt::kHeaderSize;
    put_u32(data, record + 0U, 0U);       // generic/internal code family
    put_u32(data, record + 4U, 0U);       // INT_8_BIT_WRITE
    put_u32(data, record + 8U, 0x1234U);  // ignored status
    data[record + 12U] = 1U;
    put_u32(data, record + 16U, 0x02000000U);
    put_u32(data, record + 20U, 0x02000000U);
    put_u32(data, record + 24U, 0x63U);
    put_u32(data, record + 28U, 0x10U);
    put_text(data, record + 32U, 20U, "02000000:63");
    put_text(data, record + 52U, 32U, "Health");

    record += vba_clt::kCurrentRecordSize;
    put_u32(data, record + 0U, 0U);
    put_u32(data, record + 4U, 1U);       // INT_16_BIT_WRITE
    data[record + 12U] = 1U;
    put_u32(data, record + 16U, 0x02000010U);
    put_u32(data, record + 20U, 0x02000010U);
    put_u32(data, record + 24U, 0x03E7U);
    put_text(data, record + 32U, 20U, "02000010:03E7");
    put_text(data, record + 52U, 32U, "Health");
    return data;
}

std::vector<std::uint8_t> source_type0_fixture() {
    // Legacy type 0 stores enabled as a 32-bit int and has no rawaddress.
    std::vector<std::uint8_t> data(
        vba_clt::kHeaderSize + vba_clt::kLegacyRecordSize, 0U);
    put_u32(data, 0U, 1U);
    put_u32(data, 4U, 0U);
    put_u32(data, 8U, 1U);
    const std::size_t record = vba_clt::kHeaderSize;
    put_u32(data, record + 0U, 0U);
    put_u32(data, record + 4U, 2U);       // INT_32_BIT_WRITE
    put_u32(data, record + 8U, 0U);
    put_u32(data, record + 12U, 1U);      // legacy int enabled
    put_u32(data, record + 16U, 0x02000100U);
    put_u32(data, record + 20U, 0x12345678U);
    put_u32(data, record + 24U, 0x87654321U);
    put_text(data, record + 28U, 20U, "02000100:12345678");
    put_text(data, record + 48U, 32U, "Legacy Write");
    return data;
}

CheatDocument parse_imported(const native_input::Result& imported) {
    switch (imported.input_format) {
    case native_input::InputFormat::FcdRaw:
        return codebreaker::parse(imported.text, {false});
    case native_input::InputFormat::ActionReplayMaxRaw:
        return armax::parse(imported.text, {false});
    case native_input::InputFormat::EzFlash:
        return ezflash::parse(imported.text);
    default:
        return {};
    }
}

} // namespace

void test_vba_clt_source_layout_fixtures() {
    {
        const auto data = source_type1_fixture();
        const auto imported = native_input::import_file("source-type1.clt", data);
        require(imported.recognized && imported.success,
                "VBA-M source-layout type-1 fixture was not imported");
        const CheatDocument parsed = parse_imported(imported);
        require(parsed.warnings.empty() && parsed.entries.size() == 1U,
                "VBA-M type-1 fixture did not become one cheat group");
        const ByteMap bytes = direct_write_bytes(parsed);
        require(bytes.at(0x02000000U) == 0x63U &&
                    bytes.at(0x02000010U) == 0xE7U &&
                    bytes.at(0x02000011U) == 0x03U,
                "VBA-M type-1 fixture values changed during import");
    }
    {
        const auto data = source_type0_fixture();
        const auto imported = native_input::import_file("source-type0.clt", data);
        require(imported.recognized && imported.success,
                "VBA-M source-layout type-0 fixture was not imported");
        const CheatDocument parsed = parse_imported(imported);
        const ByteMap bytes = direct_write_bytes(parsed);
        require(bytes.at(0x02000100U) == 0x78U &&
                    bytes.at(0x02000101U) == 0x56U &&
                    bytes.at(0x02000102U) == 0x34U &&
                    bytes.at(0x02000103U) == 0x12U,
                "VBA-M legacy type-0 fixture values changed during import");
    }
}

void test_vba_clt_export_record_mapping() {
    const CheatDocument document = codebreaker::parse(
        "[Writes]\n"
        "32001000 0012\n"
        "82001002 3456\n"
        "[Conditional]\n"
        "72002000 0001\n"
        "32002001 0077\n",
        {false});
    require(document.warnings.empty() && document.entries.size() == 2U,
            "VBA-M mapping fixture did not parse");

    const auto result = output_modes::export_document(
        document, output_modes::Format::VisualBoyAdvanceClt);
    require(result.success && result.warnings.empty() &&
                result.exported_records == 4U,
            "VBA-M .clt mapping export failed");
    require(result.data.size() == vba_clt::kHeaderSize +
                4U * vba_clt::kCurrentRecordSize,
            "VBA-M .clt was padded or used the wrong record size");
    require(get_u32(result.data, 0U) == 1U &&
                get_u32(result.data, 4U) == 1U &&
                get_u32(result.data, 8U) == 4U,
            "VBA-M .clt header does not match cheatsSaveCheatList");

    const std::size_t first = vba_clt::kHeaderSize;
    require(get_u32(result.data, first + 0U) == 512U &&
                get_u32(result.data, first + 4U) == 0U &&
                get_u32(result.data, first + 16U) == 0x32001000U &&
                get_u32(result.data, first + 20U) == 0x02001000U &&
                get_u32(result.data, first + 24U) == 0x12U,
            "VBA-M CodeBreaker byte-write fields are incorrect");

    const std::size_t condition = first +
        2U * vba_clt::kCurrentRecordSize;
    require(get_u32(result.data, condition + 0U) == 512U &&
                get_u32(result.data, condition + 4U) == 8U &&
                get_u32(result.data, condition + 16U) == 0x72002000U &&
                get_u32(result.data, condition + 20U) == 0x02002000U &&
                get_u32(result.data, condition + 24U) == 1U,
            "VBA-M CodeBreaker conditional fields are incorrect");
}

void test_cli_native_cht_to_vba_clt() {
    std::istringstream input(
        "[Infinite Health]\n"
        "ON=W8:0,63;\n");
    std::ostringstream output;
    std::ostringstream error;
    const int result = cli::run(
        {"--from", "auto", "--to", "vba-clt", "-"},
        input, output, error, "GbaCheatConverterCLI");
    const std::string bytes = output.str();
    require(result == 0 && bytes.size() ==
                vba_clt::kHeaderSize + vba_clt::kCurrentRecordSize,
            "CLI did not convert native .cht input to VBA-M .clt");
    const std::vector<std::uint8_t> data(bytes.begin(), bytes.end());
    require(get_u32(data, 0U) == 1U && get_u32(data, 4U) == 1U &&
                get_u32(data, 8U) == 1U,
            "CLI VBA-M .clt output has an invalid header");
    require(error.str().find("detected native: EZ-Flash .cht") !=
                std::string::npos,
            "CLI did not report native .cht auto-detection");
}

} // namespace gba::tests
