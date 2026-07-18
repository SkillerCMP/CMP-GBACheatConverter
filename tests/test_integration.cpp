#include "test_support.hpp"

namespace gba::tests {
namespace {

CheatDocument direct_document() {
    CheatDocument document;
    CheatEntry entry;
    entry.name = "Integration Write";
    entry.enabled = true;
    Operation operation;
    operation.kind = OperationKind::Write;
    operation.address = 0x02001234U;
    operation.value = 0x56U;
    operation.width = 1U;
    entry.operations.push_back(operation);
    document.entries.push_back(entry);
    return document;
}

native_input::Result export_import(
    const CheatDocument& document,
    output_modes::Format format,
    std::string_view filename,
    const output_modes::Options& options = {}) {
    const output_modes::Result exported =
        output_modes::export_document(document, format, options);
    require(exported.success && !exported.data.empty(),
            "integration native export failed");
    const native_input::Result imported =
        native_input::import_file(filename, exported.data);
    require(imported.recognized && imported.success,
            imported.warnings.empty()
                ? "integration native import failed"
                : imported.warnings.front());
    return imported;
}

} // namespace

void test_unified_cht_detection() {
    const CheatDocument document = direct_document();

    {
        const auto imported = export_import(
            document, output_modes::Format::MyBoyCht, "game.cht");
        require(imported.source_format == native_input::SourceFormat::MyBoyCht,
                "unified .cht detector missed My Boy!");
        require(imported.detection_confidence ==
                    native_input::DetectionConfidence::Exact,
                "My Boy! detection confidence was not exact");
    }
    {
        const auto imported = export_import(
            document, output_modes::Format::LibretroCht, "game.cht");
        require(imported.source_format ==
                    native_input::SourceFormat::LibretroCht,
                "unified .cht detector missed RetroArch");
    }
    {
        output_modes::Options options;
        options.rom_md5 = "0123456789abcdef0123456789abcdef";
        options.game_name = "Integration Game";
        const auto imported = export_import(
            document, output_modes::Format::MednafenCht,
            "game.cht", options);
        require(imported.source_format ==
                    native_input::SourceFormat::MednafenCht,
                "unified .cht detector missed Mednafen");
    }
    {
        output_modes::Options options;
        options.ezflash_mode = output_modes::EzFlashMode::Enhanced;
        const auto imported = export_import(
            document, output_modes::Format::EzFlashCht,
            "game.cht", options);
        require(imported.source_format ==
                    native_input::SourceFormat::EzFlashCht,
                "unified .cht detector missed EZ-Flash");
    }
}

void test_cli_format_listing_and_detection() {
    {
        std::istringstream input;
        std::ostringstream output;
        std::ostringstream error;
        const int result = cli::run(
            {"--list-formats"}, input, output, error,
            "GbaCheatConverter.exe");
        require(result == 0 && error.str().empty(),
                "--list-formats failed");
        for (const std::string name : {
                 "vba-clt", "myboy-cht", "retroarch-cht",
                 "mgba-cheats", "mednafen-cht", "mister-gg",
                 "mister-zip", "ezflash-cht"}) {
            require(output.str().find(name) != std::string::npos,
                    "--list-formats omitted " + name);
        }
    }
    {
        const auto exported = output_modes::export_document(
            direct_document(), output_modes::Format::LibretroCht);
        const std::string text(exported.data.begin(), exported.data.end());
        std::istringstream input(text);
        std::ostringstream output;
        std::ostringstream error;
        const int result = cli::run(
            {"--detect-only", "-"}, input, output, error,
            "GbaCheatConverter.exe");
        require(result == 0 &&
                    output.str().find("retroarch-cht") != std::string::npos &&
                    output.str().find("confidence") == std::string::npos,
                "native --detect-only did not report RetroArch");
    }
    {
        std::istringstream input(
            "Integration Write\n82001234 0056\n");
        std::ostringstream output;
        std::ostringstream error;
        const int result = cli::run(
            {"--detect-only", "-"}, input, output, error,
            "GbaCheatConverter.exe");
        require(result == 0 &&
                    output.str().find("cb-raw") != std::string::npos,
                "semantic --detect-only did not report CodeBreaker raw");
    }
}

void test_cli_native_alias_matrix() {
    const std::string input_text =
        "Integration Write\n82001234 0056\n";

    struct Target {
        const char* name;
        const char* filename;
        native_input::SourceFormat expected;
    };
    const Target targets[] = {
        {"myboy-cht", "out.cht", native_input::SourceFormat::MyBoyCht},
        {"retroarch-cht", "out.cht", native_input::SourceFormat::LibretroCht},
        {"mgba-cheats", "out.cheats", native_input::SourceFormat::MgbaCheats},
        {"mister-gg", "out.gg", native_input::SourceFormat::MisterZip},
        {"mister-zip", "out.zip", native_input::SourceFormat::MisterZip},
        {"ezflash-cht", "out.cht", native_input::SourceFormat::EzFlashCht},
        {"vba-clt", "out.clt", native_input::SourceFormat::VisualBoyAdvanceClt},
        {"armax-dsc", "out.dsc", native_input::SourceFormat::ArmaxDsc},
    };

    for (const Target& target : targets) {
        std::istringstream input(input_text);
        std::ostringstream output;
        std::ostringstream error;
        std::vector<std::string> arguments = {
            "--from", "cb-raw", "--to", target.name, "-"};
        if (std::string(target.name) == "armax-dsc") {
            arguments.insert(arguments.end() - 1,
                             {"--game-name", "Integration Game"});
        }
        const int result = cli::run(
            arguments, input, output, error,
            "GbaCheatConverter.exe");
        require(result == 0,
                std::string("CLI native alias failed: ") + target.name +
                " / " + error.str());
        const std::string bytes_text = output.str();
        const std::vector<std::uint8_t> bytes(
            bytes_text.begin(), bytes_text.end());
        const auto imported = native_input::import_file(
            target.filename, bytes);
        require(imported.recognized && imported.success &&
                    imported.source_format == target.expected,
                std::string("CLI alias output was not recognized: ") +
                    target.name);
    }
}

void test_cross_format_direct_write_matrix() {
    const CheatDocument document = direct_document();
    output_modes::Options mednafen_options;
    mednafen_options.rom_md5 = "0123456789abcdef0123456789abcdef";
    mednafen_options.game_name = "Integration Game";

    struct Case {
        output_modes::Format format;
        const char* filename;
        const output_modes::Options* options;
    };
    const output_modes::Options defaults;
    const Case cases[] = {
        {output_modes::Format::ArmaxDsc, "game.dsc", &defaults},
        {output_modes::Format::VisualBoyAdvanceClt, "game.clt", &defaults},
        {output_modes::Format::MyBoyCht, "game.cht", &defaults},
        {output_modes::Format::LibretroCht, "game.cht", &defaults},
        {output_modes::Format::MgbaCheats, "game.cheats", &defaults},
        {output_modes::Format::MednafenCht, "game.cht", &mednafen_options},
        {output_modes::Format::MisterGg, "game.gg", &defaults},
        {output_modes::Format::MisterZip, "game.zip", &defaults},
        {output_modes::Format::EzFlashCht, "game.cht", &defaults},
    };

    for (const Case& item : cases) {
        const auto imported = export_import(
            document, item.format, item.filename, *item.options);
        require(imported.detection_confidence !=
                    native_input::DetectionConfidence::None,
                "cross-format import had no detection confidence");
    }
}

void test_competing_cht_detection_note() {
    const std::string mixed =
        "cheats = 1\n"
        "cheat0_desc = \"Retro\"\n"
        "cheat0_code = \"82001234 0056\"\n"
        "cheat0_enable = false\n"
        "[EZ Option]\n"
        "ON=1234,56;\n";
    const std::vector<std::uint8_t> bytes(mixed.begin(), mixed.end());
    const auto imported = native_input::import_file("mixed.cht", bytes);
    require(imported.recognized && imported.success &&
                imported.source_format ==
                    native_input::SourceFormat::LibretroCht,
            "scored .cht selection did not prefer RetroArch");
    require(!imported.competing_formats.empty(),
            "competing .cht parser was not reported");
}

} // namespace gba::tests
