#include "test_support.hpp"

#include "formats/mgba_cheats.hpp"

namespace gba::tests {
namespace {

const char* source_fixture() {
    return
        "!GSAv1 raw\n"
        "!disabled\n"
        "# Raw GSA\n"
        "02000010 00000012\n"
        "!PARv3 raw\n"
        "# Raw PAR\n"
        "00200020 00000034\n"
        "!reset\n"
        "# Inherited PAR\n"
        "00200021 00000056\n"
        "# CodeBreaker\n"
        "32000030 0077\n"
        "# VBA Write\n"
        "02000040:88\n";
}

std::vector<std::uint8_t> bytes(std::string_view value) {
    return {value.begin(), value.end()};
}

} // namespace

void test_mgba_source_parser_and_state() {
    const auto parsed = mgba_cheats::parse(source_fixture(), true);
    require(parsed.recognized && parsed.success,
            "source-derived mGBA fixture did not parse");
    require(parsed.records.size() == 5U,
            "mGBA parser lost source fixture sets");
    require(!parsed.records[0].enabled &&
            parsed.records[0].family == MgbaCodeFamily::GameSharkV1Raw,
            "mGBA disabled state or GSAv1 raw directive was lost");
    require(parsed.records[1].enabled &&
            parsed.records[1].family ==
                MgbaCodeFamily::ProActionReplayV3Raw,
            "mGBA PARv3 raw directive was not applied");
    require(parsed.records[2].family ==
                MgbaCodeFamily::ProActionReplayV3Raw,
            "mGBA !reset did not retain the previous set properties");
    require(parsed.records[3].code_lines.size() == 1U &&
            parsed.records[3].code_lines[0] == "32000030 0077",
            "mGBA CodeBreaker row was not preserved");
    require(parsed.records[4].code_lines.size() == 1U &&
            parsed.records[4].code_lines[0] == "02000040:88",
            "mGBA VBA ADDRESS:VALUE row was not preserved");

    const std::string saved = mgba_cheats::serialize(parsed.records);
    const auto reparsed = mgba_cheats::parse(saved, true);
    require(reparsed.success && reparsed.records.size() == parsed.records.size(),
            "mGBA canonical save did not reparse");
    for (std::size_t i = 0; i < parsed.records.size(); ++i) {
        require(reparsed.records[i].enabled == parsed.records[i].enabled &&
                reparsed.records[i].family == parsed.records[i].family &&
                reparsed.records[i].code_lines == parsed.records[i].code_lines,
                "mGBA canonical save changed native record behavior");
    }

    const std::string enabled_only =
        "# Enabled CodeBreaker\n"
        "32000030 0077\n";
    require(!mgba_cheats::looks_like(enabled_only),
            "directive-free code text was over-detected as mGBA");
    const auto extension_parse = mgba_cheats::parse(enabled_only, true);
    require(extension_parse.success && extension_parse.records.size() == 1U &&
            extension_parse.records[0].enabled,
            "enabled-only .cheats file was not accepted by extension");
}

void test_mgba_native_import_and_lossless_export() {
    const auto imported = native_input::import_file(
        "fixture.cheats", bytes(source_fixture()));
    require(imported.recognized && imported.success && imported.has_document,
            "mGBA native import failed");
    require(imported.source_format == native_input::SourceFormat::MgbaCheats &&
            imported.document.entries.size() == 5U,
            "mGBA native import returned the wrong source or set count");
    require(imported.document.entries[0].mgba &&
            !imported.document.entries[0].enabled &&
            imported.document.entries[0].mgba->family ==
                MgbaCodeFamily::GameSharkV1Raw,
            "mGBA native metadata was not retained");
    const CheatEntry& vba = imported.document.entries[4];
    require(vba.mgba && vba.operations.size() == 1U &&
            vba.operations[0].kind == OperationKind::Write &&
            vba.operations[0].address == 0x02000040U &&
            vba.operations[0].value == 0x88U &&
            vba.operations[0].width == 1U,
            "mGBA VBA row did not normalize to a semantic byte write");

    const auto exported = output_modes::export_document(
        imported.document, output_modes::Format::MgbaCheats);
    require(exported.success && exported.exported_entries == 5U,
            "mGBA lossless native export failed");
    const std::string saved(exported.data.begin(), exported.data.end());
    const auto reparsed = mgba_cheats::parse(saved, true);
    require(reparsed.success && reparsed.records.size() == 5U,
            "mGBA lossless output did not parse");
    require(!reparsed.records[0].enabled &&
            reparsed.records[0].family == MgbaCodeFamily::GameSharkV1Raw &&
            reparsed.records[4].code_lines[0] == "02000040:88",
            "mGBA lossless output changed state, family, or VBA text");
}

void test_mgba_cli_and_semantic_export() {
    {
        const std::string input =
            "# Enabled CodeBreaker\n"
            "32000030 0077\n";
        std::istringstream in(input);
        std::ostringstream out;
        std::ostringstream err;
        const int code = cli::run(
            {"--from", "mgba-cheats", "--to", "mgba-cheats", "-"},
            in, out, err, "GbaCheatConverterCLI");
        require(code == 0 &&
                out.str().find("!disabled") == std::string::npos &&
                out.str().find("# Enabled CodeBreaker") != std::string::npos &&
                out.str().find("32000030 0077") != std::string::npos,
                "mGBA explicit CLI input did not support enabled-only stdin");
    }

    CheatDocument document;
    CheatEntry enabled;
    enabled.name = "Enabled Write";
    enabled.enabled = true;
    Operation write;
    write.kind = OperationKind::Write;
    write.address = 0x02000010U;
    write.value = 0x12U;
    write.width = 1U;
    enabled.operations.push_back(write);
    document.entries.push_back(enabled);

    CheatEntry disabled = enabled;
    disabled.name = "Disabled Write";
    disabled.enabled = false;
    disabled.operations[0].address = 0x02000011U;
    document.entries.push_back(disabled);

    const auto exported = output_modes::export_document(
        document, output_modes::Format::MgbaCheats);
    const std::string text(exported.data.begin(), exported.data.end());
    require(exported.success && exported.exported_entries == 2U,
            "semantic mGBA export failed");
    require(text.rfind("# Enabled Write\n", 0U) == 0U,
            "enabled mGBA entries were incorrectly prefixed with !disabled");
    require(text.find("!disabled\n# Disabled Write\n") != std::string::npos,
            "disabled mGBA entry state was not exported");
}

} // namespace gba::tests
