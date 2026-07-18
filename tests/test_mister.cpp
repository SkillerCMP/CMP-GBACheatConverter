#include "test_support.hpp"

#include "formats/mister_gg.hpp"

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace gba::tests {
namespace {

void append_u16(std::vector<std::uint8_t>& data, std::uint16_t value) {
    data.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    data.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& data, std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        data.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void append_record(std::vector<std::uint8_t>& data,
                   std::uint32_t flags,
                   std::uint32_t address,
                   std::uint32_t compare,
                   std::uint32_t value) {
    append_u32(data, flags);
    append_u32(data, address);
    append_u32(data, compare);
    append_u32(data, value);
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& data,
                       std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
        (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
        (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

Operation write(std::uint32_t address,
                std::uint32_t value,
                std::uint8_t width) {
    Operation operation;
    operation.kind = OperationKind::Write;
    operation.address = address;
    operation.value = value;
    operation.width = width;
    return operation;
}

const std::vector<std::uint8_t>& deflated_descriptor_fixture() {
    static const std::vector<std::uint8_t> fixture{
        0x50U, 0x4BU, 0x03U, 0x04U, 0x14U, 0x00U, 0x08U, 0x00U,
        0x08U, 0x00U, 0x84U, 0x1AU, 0xEFU, 0x5CU, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x12U, 0x00U, 0x00U, 0x00U, 0x46U, 0x6FU,
        0x6CU, 0x64U, 0x65U, 0x72U, 0x2FU, 0x44U, 0x65U, 0x66U,
        0x6CU, 0x61U, 0x74U, 0x65U, 0x64U, 0x2EU, 0x67U, 0x67U,
        0x13U, 0x60U, 0x00U, 0x03U, 0x26U, 0x10U, 0x11U, 0x05U,
        0xC4U, 0x00U, 0x50U, 0x4BU, 0x07U, 0x08U, 0x8BU, 0xB2U,
        0xDAU, 0x80U, 0x0AU, 0x00U, 0x00U, 0x00U, 0x10U, 0x00U,
        0x00U, 0x00U, 0x50U, 0x4BU, 0x01U, 0x02U, 0x14U, 0x03U,
        0x14U, 0x00U, 0x08U, 0x00U, 0x08U, 0x00U, 0x84U, 0x1AU,
        0xEFU, 0x5CU, 0x8BU, 0xB2U, 0xDAU, 0x80U, 0x0AU, 0x00U,
        0x00U, 0x00U, 0x10U, 0x00U, 0x00U, 0x00U, 0x12U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x80U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x46U, 0x6FU, 0x6CU, 0x64U, 0x65U, 0x72U, 0x2FU, 0x44U,
        0x65U, 0x66U, 0x6CU, 0x61U, 0x74U, 0x65U, 0x64U, 0x2EU,
        0x67U, 0x67U, 0x50U, 0x4BU, 0x05U, 0x06U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x01U, 0x00U, 0x01U, 0x00U, 0x40U, 0x00U,
        0x00U, 0x00U, 0x4AU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
    };
    return fixture;
}


std::vector<std::uint8_t> dynamic_deflated_fixture() {
    static const std::vector<std::uint8_t> compressed{
        0x2DU, 0xCDU, 0x31U, 0x2CU, 0x03U, 0x61U, 0x18U, 0xC6U,
        0xF1U, 0x46U, 0x0CU, 0x06U, 0x43U, 0x07U, 0x83U, 0xC1U,
        0xD0U, 0xC1U, 0x60U, 0x30U, 0x74U, 0x30U, 0x18U, 0x0CU,
        0x1DU, 0x0CU, 0x1DU, 0x0CU, 0x06U, 0x83U, 0xC1U, 0x60U,
        0x30U, 0x18U, 0x0CU, 0x06U, 0x83U, 0xC1U, 0xE0U, 0xD0U,
        0xD0U, 0x48U, 0xD3U, 0x34U, 0xCDU, 0x91U, 0x43U, 0xC3U,
        0x39U, 0xDAU, 0x5EU, 0xDBU, 0xD3U, 0x9EU, 0x3AU, 0xE7U,
        0x5CU, 0xAFU, 0x65U, 0x30U, 0x18U, 0x0CU, 0x06U, 0x83U,
        0x41U, 0xC2U, 0xD0U, 0x34U, 0x22U, 0x15U, 0x15U, 0x12U,
        0x06U, 0xE1U, 0x7BU, 0xE4U, 0x79U, 0x87U, 0xEFU, 0xC9U,
        0x2FU, 0xF9U, 0x92U, 0xBFU, 0xDFU, 0xF7U, 0x7FU, 0x6DU,
        0x78U, 0x0EU, 0xC5U, 0x05U, 0xC4U, 0xB6U, 0xD3U, 0x6AU,
        0x3AU, 0x73U, 0x10U, 0x12U, 0xDBU, 0x41U, 0x1BU, 0x9AU,
        0x9AU, 0x59U, 0x14U, 0xDBU, 0x49U, 0xEBU, 0x05U, 0x2DU,
        0x1DU, 0x14U, 0xEBU, 0xA7U, 0x2DU, 0xDDU, 0x50U, 0x2FU,
        0xC4U, 0x76U, 0xD1U, 0x66U, 0xAEU, 0xB8U, 0xF7U, 0x26U,
        0xB6U, 0x9BU, 0xF6U, 0x6CU, 0x5DU, 0x13U, 0x7FU, 0x7DU,
        0x3DU, 0xB4U, 0x53U, 0xCAU, 0xEFU, 0xA3U, 0x17U, 0xA0U,
        0x23U, 0x9EU, 0x65U, 0xA0U, 0xD7U, 0x4BU, 0x4BU, 0x55U,
        0xBBU, 0x80U, 0x5EU, 0x1FU, 0x1DU, 0x73U, 0xCDU, 0x22U,
        0x7AU, 0xFDU, 0x74U, 0x74U, 0xA5U, 0x7CU, 0x84U, 0x5EU,
        0x90U, 0x96U, 0x25U, 0x4FU, 0x47U, 0x6FU, 0x80U, 0x4EU,
        0x2CU, 0xD5U, 0xB2U, 0xE8U, 0x0DU, 0xD2U, 0xA9U, 0xB8U,
        0x93U, 0x47U, 0x6FU, 0x88U, 0x56U, 0xD6U, 0xDCU, 0x1CU,
        0x7AU, 0x21U, 0xBAU, 0x2EU, 0x47U, 0x2CU, 0xF4U, 0x86U,
        0xE9U, 0xA7U, 0x8DU, 0xD5U, 0x13U, 0xF4U, 0xC2U, 0x74U,
        0x33U, 0x29U, 0xD9U, 0xE8U, 0x8DU, 0xD0U, 0xCFU, 0x3BU,
        0xCBU, 0xA7U, 0xE8U, 0x8DU, 0xD2U, 0x9FU, 0x4AU, 0xCCU,
        0x44U, 0x6FU, 0x8CU, 0x6EU, 0x6DU, 0xC5U, 0x4BU, 0xE8U,
        0x8DU, 0xD3U, 0x3FU, 0x8DU, 0x68U, 0x19U, 0xBDU, 0x09U,
        0xFAU, 0xFBU, 0x71U, 0xFDU, 0x18U, 0xBDU, 0x49U, 0xFAU,
        0xAAU, 0x29U, 0x7BU, 0xE8U, 0x4DU, 0xD1U, 0x97U, 0xAFU,
        0x9BU, 0x15U, 0xF4U, 0xA6U, 0xE9U, 0x9BU, 0x97U, 0x44U,
        0x0DU, 0xBDU, 0x19U, 0xFAU, 0xFAU, 0x23U, 0x59U, 0x45U,
        0x6FU, 0x96U, 0xBEU, 0x6BU, 0xA5U, 0x1CU, 0xF4U, 0xE6U,
        0xE8U, 0xDBU, 0xF7U, 0xDDU, 0x33U, 0xF4U, 0xE6U, 0xE9U,
        0x87U, 0x5FU, 0xC5U, 0x45U, 0x6FU, 0x81U, 0xBEU, 0xFFU,
        0xDAU, 0x3EU, 0xFFU, 0x03U
    };
    const std::string name = "Dynamic.gg";
    constexpr std::uint32_t crc = 0xC3986DEEU;
    constexpr std::uint32_t uncompressed_size = 512U;
    std::vector<std::uint8_t> zip;
    append_u32(zip, 0x04034B50U);
    append_u16(zip, 20U);
    append_u16(zip, 0U);
    append_u16(zip, 8U);
    append_u16(zip, 0U);
    append_u16(zip, 0U);
    append_u32(zip, crc);
    append_u32(zip, static_cast<std::uint32_t>(compressed.size()));
    append_u32(zip, uncompressed_size);
    append_u16(zip, static_cast<std::uint16_t>(name.size()));
    append_u16(zip, 0U);
    zip.insert(zip.end(), name.begin(), name.end());
    zip.insert(zip.end(), compressed.begin(), compressed.end());
    const std::uint32_t central_offset = static_cast<std::uint32_t>(zip.size());
    append_u32(zip, 0x02014B50U);
    append_u16(zip, 20U);
    append_u16(zip, 20U);
    append_u16(zip, 0U);
    append_u16(zip, 8U);
    append_u16(zip, 0U);
    append_u16(zip, 0U);
    append_u32(zip, crc);
    append_u32(zip, static_cast<std::uint32_t>(compressed.size()));
    append_u32(zip, uncompressed_size);
    append_u16(zip, static_cast<std::uint16_t>(name.size()));
    append_u16(zip, 0U);
    append_u16(zip, 0U);
    append_u16(zip, 0U);
    append_u16(zip, 0U);
    append_u32(zip, 0U);
    append_u32(zip, 0U);
    zip.insert(zip.end(), name.begin(), name.end());
    const std::uint32_t central_size =
        static_cast<std::uint32_t>(zip.size()) - central_offset;
    append_u32(zip, 0x06054B50U);
    append_u16(zip, 0U);
    append_u16(zip, 0U);
    append_u16(zip, 1U);
    append_u16(zip, 1U);
    append_u32(zip, central_size);
    append_u32(zip, central_offset);
    append_u16(zip, 0U);
    return zip;
}

} // namespace

void test_mister_source_record_semantics() {
    std::vector<std::uint8_t> data;
    constexpr std::array<OperationKind, 6U> expected{
        OperationKind::IfEqual,
        OperationKind::IfGreater,
        OperationKind::IfGreaterOrEqual,
        OperationKind::IfLess,
        OperationKind::IfLessOrEqual,
        OperationKind::IfNotEqual
    };
    for (std::uint32_t type = 1U; type <= 6U; ++type) {
        append_record(data, 0x30U | type, 0x02000000U, type == 1U ? 7U : 0U,
                      type);
        append_record(data, 0x10U, 0x02000100U + (type - 1U) * 4U,
                      0U, 0x40U + type);
    }
    // Arbitrary byte masks are legal in the RTL and become exact byte writes.
    append_record(data, 0x50U, 0x02000200U, 0U, 0x00AA00BBU);

    const auto imported = native_input::import_file("Source.gg", data);
    require(imported.recognized && imported.success && imported.has_document,
            "source-derived MiSTer .gg fixture did not import");
    require(imported.source_format == native_input::SourceFormat::MisterZip,
            "raw MiSTer .gg source type was not retained");
    require(imported.document.entries.size() == 1U,
            "raw MiSTer .gg did not become one cheat entry");

    const CheatEntry& entry = imported.document.entries.front();
    require(entry.operations.size() == 14U,
            "MiSTer operation fixture decoded the wrong operation count");
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        const Operation& condition = entry.operations[index * 2U];
        require(condition.kind == expected[index] &&
                condition.condition_span == 1U,
                "MiSTer condition opcode truth table was decoded incorrectly");
        require(entry.operations[index * 2U + 1U].kind == OperationKind::Write,
                "MiSTer condition did not retain its following write");
    }
    require(entry.operations[12U].address == 0x02000200U &&
            entry.operations[12U].value == 0xBBU &&
            entry.operations[13U].address == 0x02000202U &&
            entry.operations[13U].value == 0xAAU,
            "MiSTer arbitrary byte mask did not split exactly");
    require(!imported.warnings.empty(),
            "nonzero ignored MiSTer compare data did not produce a warning");

    // Verify the exporter uses the actual GBA-core operation numbering for
    // every supported comparison, including the counterintuitive 3/4 pair.
    constexpr std::array<OperationKind, 6U> export_kinds{
        OperationKind::IfEqual,
        OperationKind::IfGreater,
        OperationKind::IfGreaterOrEqual,
        OperationKind::IfLess,
        OperationKind::IfLessOrEqual,
        OperationKind::IfNotEqual
    };
    for (std::size_t index = 0U; index < export_kinds.size(); ++index) {
        CheatEntry export_entry;
        export_entry.name = "Opcode";
        Operation export_condition;
        export_condition.kind = export_kinds[index];
        export_condition.address = 0x02001000U;
        export_condition.value = 0x12U;
        export_condition.width = 1U;
        export_condition.condition_span = 1U;
        export_entry.operations.push_back(export_condition);
        export_entry.operations.push_back(write(0x02001004U, 0x34U, 1U));
        const auto export_result = mister_gg::encode_entry(export_entry);
        require(export_result.success && export_result.record_count == 2U &&
                (read_u32(export_result.data, 0U) & 0x0FU) == index + 1U,
                "MiSTer exporter emitted an incorrect condition opcode");
    }
}

void test_mister_condition_expansion_and_limit() {
    CheatEntry entry;
    entry.name = "Three Controlled Writes";
    Operation condition;
    condition.kind = OperationKind::IfEqual;
    condition.address = 0x02000000U;
    condition.value = 1U;
    condition.width = 2U;
    condition.condition_span = 3U;
    entry.operations.push_back(condition);
    entry.operations.push_back(write(0x02000004U, 0x11U, 1U));
    entry.operations.push_back(write(0x02000006U, 0x2233U, 2U));
    entry.operations.push_back(write(0x02000008U, 0x44556677U, 4U));

    const auto encoded = mister_gg::encode_entry(entry);
    require(encoded.success && encoded.record_count == 6U &&
            encoded.data.size() == 96U,
            "MiSTer multi-write condition was not expanded per record");
    for (std::size_t pair = 0U; pair < 3U; ++pair) {
        const std::size_t condition_offset = pair * 32U;
        require((read_u32(encoded.data, condition_offset) & 0x0FU) == 1U &&
                read_u32(encoded.data, condition_offset + 8U) == 0U,
                "MiSTer repeated condition record is malformed");
        require((read_u32(encoded.data, condition_offset + 16U) & 0x0FU) == 0U,
                "MiSTer controlled write record is malformed");
    }

    const auto decoded = mister_gg::decode_entry(entry.name, encoded.data);
    require(decoded.success && decoded.entry.operations.size() == 6U,
            "MiSTer expanded condition did not decode");
    for (std::size_t pair = 0U; pair < 3U; ++pair) {
        require(decoded.entry.operations[pair * 2U].kind ==
                    OperationKind::IfEqual &&
                decoded.entry.operations[pair * 2U].condition_span == 1U,
                "MiSTer one-record skip behavior was not retained");
    }

    CheatEntry boundary;
    boundary.name = "Boundary";
    Operation boundary_condition = condition;
    boundary_condition.condition_span = 16U;
    boundary.operations.push_back(boundary_condition);
    for (std::uint32_t index = 0U; index < 16U; ++index) {
        boundary.operations.push_back(write(
            0x02000100U + index * 4U, index, 4U));
    }
    require(mister_gg::encode_entry(boundary).success,
            "MiSTer rejected the exact 32-record boundary");

    boundary.operations.push_back(write(0x02000200U, 0x55U, 1U));
    boundary.operations.front().condition_span = 17U;
    const auto too_large = mister_gg::encode_entry(boundary);
    require(!too_large.success &&
            too_large.error.find("32") != std::string::npos,
            "MiSTer did not reject expansion beyond 32 active records");
}

void test_mister_deflate_zip_detection_and_cli() {
    const auto imported = native_input::import_file(
        "Deflated.zip", deflated_descriptor_fixture());
    require(imported.recognized && imported.success && imported.has_document,
            "Deflate/data-descriptor MiSTer ZIP did not import");
    require(imported.document.entries.size() == 1U &&
            imported.document.entries.front().name == "Deflated" &&
            direct_write_bytes(imported.document).at(0x02000000U) == 0x5AU,
            "Deflated MiSTer ZIP payload changed during import");

    const auto dynamic_import = native_input::import_file(
        "Dynamic.zip", dynamic_deflated_fixture());
    require(dynamic_import.recognized && dynamic_import.success &&
            dynamic_import.has_document &&
            dynamic_import.document.entries.size() == 1U,
            "dynamic-Huffman MiSTer ZIP did not import");

    std::vector<std::uint8_t> ordinary{
        0x50U, 0x4BU, 0x05U, 0x06U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
    };
    require(!native_input::import_file("ordinary.zip", ordinary).recognized,
            "an ordinary ZIP without .gg entries was misdetected as MiSTer");

    std::vector<std::uint8_t> corrupted = deflated_descriptor_fixture();
    corrupted[52U] ^= 0x01U;
    const auto damaged = native_input::import_file("Damaged.zip", corrupted);
    require(damaged.recognized && !damaged.success,
            "corrupted Deflate MiSTer payload was not rejected");

    std::vector<std::uint8_t> mismatched_header =
        deflated_descriptor_fixture();
    mismatched_header[8U] = 0U;
    mismatched_header[9U] = 0U;
    const auto mismatched = native_input::import_file(
        "Mismatched.zip", mismatched_header);
    require(mismatched.recognized && !mismatched.success,
            "inconsistent local/central MiSTer ZIP methods were accepted");

    const std::string source =
        "[CLI MiSTer]\n"
        "32000004 0011\n"
        "82000006 2233\n";
    std::istringstream source_stream(source);
    std::ostringstream zip_stream;
    std::ostringstream export_errors;
    const int export_status = cli::run(
        {"--from", "cb-raw", "--to", "mister-zip", "-"},
        source_stream, zip_stream, export_errors, "converter");
    require(export_status == 0 && zip_stream.str().size() > 22U,
            "CLI MiSTer ZIP export alias failed");

    std::istringstream zip_input(zip_stream.str());
    std::ostringstream text_output;
    std::ostringstream import_errors;
    const int import_status = cli::run(
        {"--from", "mister-zip", "--to", "cb-raw", "-"},
        zip_input, text_output, import_errors, "converter");
    require(import_status == 0 &&
            text_output.str().find("32000004 0011") != std::string::npos &&
            text_output.str().find("82000006 2233") != std::string::npos,
            "CLI MiSTer native import alias failed");
}

} // namespace gba::tests
