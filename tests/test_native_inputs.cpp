#include "test_support.hpp"

#include "import/native_input.hpp"

namespace gba::tests {
namespace {

CheatDocument native_import_document() {
    CheatDocument document;

    CheatEntry direct;
    direct.name = "Direct Writes";
    for (const auto& values : std::vector<std::tuple<std::uint32_t,
                                                     std::uint32_t,
                                                     std::uint8_t>>{
             {0x02001000U, 0x12U, std::uint8_t{1}},
             {0x02001002U, 0x3456U, std::uint8_t{2}},
             {0x02001004U, 0x89ABCDEFU, std::uint8_t{4}}}) {
        Operation operation;
        operation.kind = OperationKind::Write;
        operation.address = std::get<0>(values);
        operation.value = std::get<1>(values);
        operation.width = std::get<2>(values);
        direct.operations.push_back(operation);
    }
    document.entries.push_back(direct);

    CheatEntry conditional;
    conditional.name = "Conditional Write";
    Operation condition;
    condition.kind = OperationKind::IfEqual;
    condition.address = 0x02002000U;
    condition.value = 1U;
    condition.width = 1U;
    condition.condition_span = 1U;
    conditional.operations.push_back(condition);
    Operation controlled;
    controlled.kind = OperationKind::Write;
    controlled.address = 0x02002001U;
    controlled.value = 0x77U;
    controlled.width = 1U;
    conditional.operations.push_back(controlled);
    document.entries.push_back(conditional);

    const auto patch = armax::parse(
        "[AR Patch]\n"
        "00000000 18000010\n"
        "00001234 DEADBEEF\n", {false});
    require(patch.entries.size() == 1U && patch.warnings.empty(),
            "native-input patch fixture did not parse");
    document.entries.push_back(patch.entries.front());
    return document;
}

CheatDocument parse_native_import(const native_input::Result& imported) {
    switch (imported.input_format) {
    case native_input::InputFormat::FcdRaw:
        return codebreaker::parse(imported.text, {false});
    case native_input::InputFormat::FcdEncrypted:
        return codebreaker::parse(imported.text, {true});
    case native_input::InputFormat::GameSharkRaw:
        return gameshark::parse(imported.text, {false});
    case native_input::InputFormat::GameSharkEncrypted:
        return gameshark::parse(imported.text, {true});
    case native_input::InputFormat::ActionReplayMaxRaw:
        return armax::parse(imported.text, {false});
    case native_input::InputFormat::ActionReplayMaxEncrypted:
        return armax::parse(imported.text, {true});
    case native_input::InputFormat::EzFlash:
        return ezflash::parse(imported.text);
    }
    return {};
}

native_input::Result export_and_import(
    const CheatDocument& document,
    output_modes::Format format,
    std::string_view filename,
    const output_modes::Options& options = {}) {
    const auto exported = output_modes::export_document(document, format, options);
    require(exported.success && !exported.data.empty(),
            "native output fixture export failed");
    const auto imported = native_input::import_file(filename, exported.data);
    require(imported.recognized, "native input was not recognized");
    require(imported.success,
            imported.warnings.empty()
                ? "native input failed without a warning"
                : imported.warnings.front());
    require(!imported.text.empty(), "native input produced empty text");
    return imported;
}

} // namespace

void test_native_input_imports() {
    const CheatDocument document = native_import_document();

    {
        output_modes::Options options;
        options.game_name = "Test Game";
        const auto imported = export_and_import(
            document, output_modes::Format::ArmaxDsc,
            "Test Game.dsc", options);
        require(imported.source_format == native_input::SourceFormat::ArmaxDsc,
                "AR MAX .dsc source type was not preserved");
        require(imported.input_format ==
                    native_input::InputFormat::ActionReplayMaxEncrypted,
                "AR MAX .dsc did not select encrypted AR MAX input");
        require(imported.text.find("[Direct Writes]") != std::string::npos,
                "AR MAX .dsc import lost cheat names");
        const auto parsed = parse_native_import(imported);
        require(parsed.warnings.empty() && parsed.entries.size() == 3U,
                "AR MAX .dsc imported text did not parse cleanly");
    }
    {
        const auto imported = export_and_import(
            document, output_modes::Format::VisualBoyAdvanceClt,
            "Test Game.clt");
        require(imported.input_format ==
                    native_input::InputFormat::ActionReplayMaxRaw,
                "VBA-M .clt did not select a lossless mixed semantic output");
        const auto parsed = parse_native_import(imported);
        require(parsed.warnings.empty() && parsed.entries.size() == 3U,
                "VBA-M .clt imported text did not parse cleanly");
        require(direct_write_bytes(parsed) == direct_write_bytes(document),
                "VBA-M .clt import changed direct-write bytes");
    }
    {
        output_modes::Options options;
        options.ezflash_mode = output_modes::EzFlashMode::Enhanced;
        const auto imported = export_and_import(
            document, output_modes::Format::EzFlashCht,
            "Test Game.cht", options);
        require(imported.input_format == native_input::InputFormat::EzFlash,
                "EZ-Flash .cht did not select EZ-Flash input");
        const auto parsed = parse_native_import(imported);
        require(parsed.entries.size() == 3U,
                "EZ-Flash .cht import lost entries");
    }
    {
        const auto imported = export_and_import(
            document, output_modes::Format::MyBoyCht,
            "Test Game.cht");
        require(imported.source_format == native_input::SourceFormat::MyBoyCht,
                "My Boy! .cht source was misidentified");
        const auto parsed = parse_native_import(imported);
        require(parsed.warnings.empty() && parsed.entries.size() == 3U,
                "mixed My Boy! import was not normalized losslessly");
    }
    {
        const auto imported = export_and_import(
            document, output_modes::Format::MgbaCheats,
            "Test Game.cheats");
        require(imported.source_format == native_input::SourceFormat::MgbaCheats,
                "mGBA source was misidentified");
        const auto parsed = parse_native_import(imported);
        require(parsed.warnings.empty() && parsed.entries.size() == 3U,
                "mixed mGBA import was not normalized losslessly");
    }
    {
        const auto imported = export_and_import(
            document, output_modes::Format::LibretroCht,
            "Test Game.cht");
        require(imported.source_format == native_input::SourceFormat::LibretroCht,
                "Libretro source was misidentified");
        const auto parsed = parse_native_import(imported);
        require(parsed.warnings.empty() && parsed.entries.size() == 3U,
                "mixed Libretro import was not normalized losslessly");
    }
    {
        output_modes::Options options;
        options.game_name = "Test Game";
        options.rom_md5 = "0123456789abcdef0123456789abcdef";
        const auto imported = export_and_import(
            document, output_modes::Format::MednafenCht,
            "Test Game.cht", options);
        require(imported.input_format ==
                    native_input::InputFormat::ActionReplayMaxRaw,
                "Mednafen .cht did not select an exact raw semantic input");
        const auto parsed = parse_native_import(imported);
        require(parsed.warnings.empty() && parsed.entries.size() == 1U,
                "Mednafen import did not preserve its direct-write entry");
    }
    {
        const auto imported = export_and_import(
            document, output_modes::Format::MisterZip,
            "Test Game.zip");
        require(imported.source_format == native_input::SourceFormat::MisterZip,
                "MiSTer ZIP source was misidentified");
        const auto parsed = parse_native_import(imported);
        require(parsed.warnings.empty() && parsed.entries.size() == 2U,
                "MiSTer ZIP import did not preserve supported entries");
    }

    {
        CheatDocument escaped;
        CheatEntry entry;
        entry.name = "A&B \\\"Quoted\\\" \\\\ Name";
        Operation operation;
        operation.kind = OperationKind::Write;
        operation.address = 0x02003000U;
        operation.value = 0x5AU;
        operation.width = 1U;
        entry.operations.push_back(operation);
        escaped.entries.push_back(entry);

        const auto myboy = export_and_import(
            escaped, output_modes::Format::MyBoyCht, "escaped.cht");
        const auto myboy_parsed = parse_native_import(myboy);
        require(myboy_parsed.entries.size() == 1U &&
                myboy_parsed.entries.front().name == entry.name,
                "My Boy! XML name escaping did not round-trip");

        const auto libretro = export_and_import(
            escaped, output_modes::Format::LibretroCht, "escaped.cht");
        const auto libretro_parsed = parse_native_import(libretro);
        require(libretro_parsed.entries.size() == 1U &&
                libretro_parsed.entries.front().name == entry.name,
                "Libretro quoted name escaping did not round-trip");
    }

    {
        output_modes::Options options;
        options.game_name = "Test Game";
        auto exported = output_modes::export_document(
            document, output_modes::Format::ArmaxDsc, options);
        exported.data.resize(0x176U);
        const auto imported = native_input::import_file(
            "broken.dsc", exported.data);
        require(imported.recognized && !imported.success &&
                !imported.warnings.empty(),
                "truncated AR MAX .dsc was not rejected safely");
    }
    {
        auto exported = output_modes::export_document(
            document, output_modes::Format::MisterZip);
        require(exported.data.size() > 32U,
                "MiSTer corruption fixture is too small");
        const std::size_t name_size =
            static_cast<std::size_t>(exported.data[26U]) |
            (static_cast<std::size_t>(exported.data[27U]) << 8U);
        const std::size_t payload_offset = 30U + name_size;
        require(payload_offset < exported.data.size(),
                "MiSTer corruption fixture has no payload");
        exported.data[payload_offset] ^= 0x01U;
        const auto imported = native_input::import_file(
            "broken.zip", exported.data);
        require(imported.recognized && !imported.success &&
                !imported.warnings.empty(),
                "MiSTer ZIP CRC corruption was not rejected");
    }
    {
        const std::string ordinary = "[Code]\n02000000 1234\n";
        const std::vector<std::uint8_t> bytes(
            ordinary.begin(), ordinary.end());
        const auto imported = native_input::import_file("codes.txt", bytes);
        require(!imported.recognized,
                "ordinary text was incorrectly treated as a native file");
    }
}

} // namespace gba::tests
