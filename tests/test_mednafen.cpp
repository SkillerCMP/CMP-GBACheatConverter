#include "test_cases.hpp"
#include "test_support.hpp"

#include "formats/mednafen_cht.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace gba::tests {
namespace {

const char* complete_fixture =
    "[0123456789abcdef0123456789abcdef] First Game\n"
    "R A 1 L 0 02000000 63 Basic Write\n"
    "\n"
    "!A I 8 B 7 02000010 1122334455667788 00000003 00000008 "
    "0000000000000001 00000000 00000000 Wide Add\n"
    "1 L 0x02001000 == 0x01, 2 B 0x02001002 != 0x0203, "
    "4 L 0x02001004 >= 0x10, 1 L 0x02001008 <= 9, "
    "1 L 0x02001009 > 0, 1 L 0x0200100a < 8, "
    "1 L 0x0200100b & 1, 1 L 0x0200100c !& 2, "
    "1 L 0x0200100d ^ 3, 1 L 0x0200100e !^ 4, "
    "1 L 0x0200100f | 5, 1 L 0x02001010 !| 0\n"
    "!T A 4 L 0 02000100 0000000000000000 00000004 00000004 "
    "0000000000000000 02010000 00000004 Transfer Block\n"
    "\n"
    "S A 2 B 0 02000200 beef Read Substitute\n"
    "\n"
    "C A 4 L 0 02000300 deadbeef 12345678 Compare Substitute\n"
    "\n"
    "[fedcba9876543210fedcba9876543210] Second Game\n"
    "R I 2 L 3 02000400 03e7 Other Section\n"
    "\n";

void require_same_record(const mednafen_cht::Record& left,
                         const mednafen_cht::Record& right) {
    require(left.rom_md5 == right.rom_md5 &&
            left.game_name == right.game_name &&
            left.name == right.name && left.type == right.type &&
            left.enabled == right.enabled && left.length == right.length &&
            left.big_endian == right.big_endian &&
            left.instance_count == right.instance_count &&
            left.address == right.address && left.value == right.value &&
            left.compare == right.compare &&
            left.repeat_count == right.repeat_count &&
            left.repeat_address_increment ==
                right.repeat_address_increment &&
            left.repeat_value_increment == right.repeat_value_increment &&
            left.copy_source_address == right.copy_source_address &&
            left.copy_source_increment == right.copy_source_increment &&
            left.conditions.size() == right.conditions.size(),
            "Mednafen record changed during codec round trip");
}

} // namespace

void test_mednafen_complete_codec() {
    const auto parsed = mednafen_cht::parse(complete_fixture);
    require(parsed.recognized && parsed.success,
            parsed.warnings.empty()
                ? "complete Mednafen fixture was not parsed"
                : parsed.warnings.front());
    require(parsed.records.size() == 6U,
            "complete Mednafen fixture record count is wrong");
    require(parsed.records[1].type == 'A' &&
            parsed.records[1].length == 8U &&
            parsed.records[1].big_endian &&
            parsed.records[1].instance_count == 7U &&
            parsed.records[1].value == 0x1122334455667788ULL &&
            parsed.records[1].repeat_count == 3U &&
            parsed.records[1].repeat_address_increment == 8U &&
            parsed.records[1].repeat_value_increment == 1U &&
            parsed.records[1].conditions.size() == 12U,
            "extended Mednafen fields were not parsed completely");
    require(parsed.records[2].type == 'T' &&
            parsed.records[2].copy_source_address == 0x02010000U &&
            parsed.records[2].copy_source_increment == 4U,
            "Mednafen transfer fields were not parsed");
    require(parsed.records[4].type == 'C' &&
            parsed.records[4].compare == 0x12345678U,
            "Mednafen compare-substitute fields were not parsed");
    require(parsed.records[5].rom_md5 ==
                "fedcba9876543210fedcba9876543210" &&
            parsed.records[5].game_name == "Second Game",
            "Mednafen multiple-section metadata was lost");

    const std::string serialized = mednafen_cht::serialize(parsed.records);
    const auto reparsed = mednafen_cht::parse(serialized);
    require(reparsed.success && reparsed.records.size() == parsed.records.size(),
            "serialized Mednafen fixture did not parse");
    for (std::size_t index = 0U; index < parsed.records.size(); ++index) {
        require_same_record(parsed.records[index], reparsed.records[index]);
    }
}

void test_mednafen_native_import_and_lossless_export() {
    const std::vector<std::uint8_t> bytes(
        complete_fixture, complete_fixture + std::char_traits<char>::length(
            complete_fixture));
    const auto imported = native_input::import_file("gba.cht", bytes);
    require(imported.recognized && imported.success && imported.has_document,
            imported.warnings.empty()
                ? "Mednafen native import failed"
                : imported.warnings.front());
    require(imported.source_format == native_input::SourceFormat::MednafenCht,
            "Mednafen native input was misdetected");
    require(imported.document.entries.size() == 6U,
            "Mednafen native import lost records");
    require(imported.document.entries[1].mednafen.has_value() &&
            imported.document.entries[1].enabled == false &&
            imported.document.entries[1].operations.back().has_wide_value,
            "Mednafen 64-bit metadata was not retained");
    require(imported.document.entries[2].operations.back().kind ==
                OperationKind::Transfer,
            "Mednafen transfer semantic kind was not retained");
    require(imported.document.entries[3].operations.back().kind ==
                OperationKind::ReadSubstitute &&
            imported.document.entries[4].operations.back().kind ==
                OperationKind::CompareReadSubstitute,
            "Mednafen read-substitution types were not retained");

    const auto exported = output_modes::export_document(
        imported.document, output_modes::Format::MednafenCht);
    require(exported.success && exported.exported_entries == 6U,
            "native Mednafen re-export failed without metadata options");
    const std::string output(exported.data.begin(), exported.data.end());
    const auto reparsed = mednafen_cht::parse(output);
    require(reparsed.success && reparsed.records.size() == 6U,
            "native Mednafen re-export is malformed");
    require(reparsed.records[1].value == 0x1122334455667788ULL &&
            reparsed.records[2].copy_source_address == 0x02010000U &&
            reparsed.records[4].compare == 0x12345678U &&
            reparsed.records[5].rom_md5 ==
                "fedcba9876543210fedcba9876543210",
            "native Mednafen re-export lost exact fields");
}

void test_mednafen_semantic_export_and_cli() {
    CheatDocument document;
    CheatEntry entry;
    entry.name = "Conditioned Slide";
    entry.enabled = true;

    Operation condition;
    condition.kind = OperationKind::IfEqual;
    condition.address = 0x02002000U;
    condition.value = 1U;
    condition.width = 1U;
    condition.condition_span = 3U;
    entry.operations.push_back(condition);
    for (std::uint32_t index = 0U; index < 3U; ++index) {
        Operation write;
        write.kind = OperationKind::Write;
        write.address = 0x02003000U + index * 2U;
        write.value = 0x10U + index;
        write.width = 2U;
        entry.operations.push_back(write);
    }
    document.entries.push_back(entry);

    output_modes::Options options;
    options.rom_md5 = "0123456789abcdef0123456789abcdef";
    options.game_name = "Semantic Game";
    const auto exported = output_modes::export_document(
        document, output_modes::Format::MednafenCht, options);
    require(exported.success && exported.exported_entries == 1U,
            "semantic Mednafen extended export failed");
    const std::string output(exported.data.begin(), exported.data.end());
    const auto parsed = mednafen_cht::parse(output);
    require(parsed.success && parsed.records.size() == 1U &&
            parsed.records[0].repeat_count == 3U &&
            parsed.records[0].repeat_address_increment == 2U &&
            parsed.records[0].repeat_value_increment == 1U &&
            parsed.records[0].conditions.size() == 1U,
            "semantic Mednafen slide/condition was not collapsed exactly");

    std::istringstream input(output);
    std::ostringstream cli_output;
    std::ostringstream cli_error;
    const int status = cli::run(
        {"--from", "mednafen-cht", "--to", "mednafen-cht", "-"},
        input, cli_output, cli_error, "GbaCheatConverterCLI");
    require(status == 0,
            "Mednafen native CLI round trip failed: " + cli_error.str());
    const auto cli_parsed = mednafen_cht::parse(cli_output.str());
    require(cli_parsed.success && cli_parsed.records.size() == 1U &&
            cli_parsed.records[0].repeat_count == 3U,
            "Mednafen CLI round trip changed the record");

    std::istringstream raw_input("[One]\n32000000 0063\n");
    std::ostringstream raw_output;
    std::ostringstream raw_error;
    const int raw_status = cli::run(
        {"--from", "cb-raw", "--to", "mednafen-cht",
         "--rom-md5", "0123456789abcdef0123456789abcdef",
         "--game-name", "CLI Game", "-"},
        raw_input, raw_output, raw_error, "GbaCheatConverterCLI");
    require(raw_status == 0,
            "semantic-to-Mednafen CLI failed: " + raw_error.str());
    require(mednafen_cht::parse(raw_output.str()).success,
            "semantic-to-Mednafen CLI output is malformed");
}

} // namespace gba::tests
