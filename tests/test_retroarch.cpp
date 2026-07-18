#include "test_support.hpp"

#include "formats/retroarch_cht.hpp"

namespace gba::tests {
namespace {

std::vector<std::uint8_t> bytes(std::string_view value) {
    return std::vector<std::uint8_t>(value.begin(), value.end());
}

retroarch_cht::Record only_record(
    const retroarch_cht::ParseResult& parsed) {
    require(parsed.recognized && parsed.success && parsed.records.size() == 1U,
            "RetroArch fixture did not parse as one record");
    return parsed.records.front();
}

} // namespace

void test_retroarch_complete_codec() {
    retroarch_cht::Record source;
    source.desc = "Quoted \\\"Name\\\"";
    source.code = "82001000 1234+32002000 00FF";
    source.enabled = true;
    source.big_endian = true;
    source.handler = 1U;
    source.memory_search_size = 4U;
    source.cheat_type = 3U;
    source.value = 0x1234U;
    source.address = 0x5678U;
    source.address_mask = 0xF0U;
    source.rumble_type = 6U;
    source.rumble_value = 7U;
    source.rumble_port = 8U;
    source.rumble_primary_strength = 9U;
    source.rumble_primary_duration = 10U;
    source.rumble_secondary_strength = 11U;
    source.rumble_secondary_duration = 12U;
    source.repeat_count = 13U;
    source.repeat_add_to_value = 14U;
    source.repeat_add_to_address = 15U;

    const std::string serialized = retroarch_cht::serialize({source});
    const retroarch_cht::Record parsed =
        only_record(retroarch_cht::parse(serialized));
    require(parsed.desc == source.desc && parsed.code == source.code &&
            parsed.enabled == source.enabled &&
            parsed.big_endian == source.big_endian &&
            parsed.handler == source.handler &&
            parsed.memory_search_size == source.memory_search_size &&
            parsed.cheat_type == source.cheat_type &&
            parsed.value == source.value && parsed.address == source.address &&
            parsed.address_mask == source.address_mask &&
            parsed.rumble_type == source.rumble_type &&
            parsed.rumble_value == source.rumble_value &&
            parsed.rumble_port == source.rumble_port &&
            parsed.rumble_primary_strength ==
                source.rumble_primary_strength &&
            parsed.rumble_primary_duration ==
                source.rumble_primary_duration &&
            parsed.rumble_secondary_strength ==
                source.rumble_secondary_strength &&
            parsed.rumble_secondary_duration ==
                source.rumble_secondary_duration &&
            parsed.repeat_count == source.repeat_count &&
            parsed.repeat_add_to_value == source.repeat_add_to_value &&
            parsed.repeat_add_to_address == source.repeat_add_to_address,
            "RetroArch complete record fields did not round-trip");
}

void test_retroarch_native_import_and_detection() {
    const std::string input =
        "cheats = 3\n"
        "cheat0_handler = 1\n"
        "cheat0_desc = \"Native Fill\"\n"
        "cheat0_enable = true\n"
        "cheat0_memory_search_size = 4\n"
        "cheat0_cheat_type = 1\n"
        "cheat0_value = 4660\n"
        "cheat0_address = 256\n"
        "cheat0_repeat_count = 3\n"
        "cheat0_repeat_add_to_value = 1\n"
        "cheat0_repeat_add_to_address = 2\n"
        "cheat1_handler = 1\n"
        "cheat1_desc = \"Native Condition\"\n"
        "cheat1_memory_search_size = 3\n"
        "cheat1_cheat_type = 6\n"
        "cheat1_value = 5\n"
        "cheat1_address = 32\n"
        "cheat2_handler = 0\n"
        "cheat2_desc = \"Core Code\"\n"
        "cheat2_code = \"82001000 1234\"\n";

    const auto imported = native_input::import_file(
        "native-only.cht", bytes(input));
    require(imported.recognized && imported.success && imported.has_document,
            "complete RetroArch file was not recognized/imported");
    require(imported.source_format == native_input::SourceFormat::LibretroCht,
            "RetroArch file was misidentified");
    require(imported.document.entries.size() == 3U,
            "RetroArch declared record count was not preserved");

    const CheatEntry& fill = imported.document.entries[0];
    require(fill.enabled && fill.retroarch && fill.operations.size() == 1U,
            "RetroArch native write state/metadata was not imported");
    require(fill.operations[0].kind == OperationKind::Write &&
            fill.operations[0].width == 2U &&
            fill.operations[0].address == 256U &&
            fill.operations[0].value == 4660U &&
            fill.operations[0].repeat == 3U &&
            fill.operations[0].address_step == 4 &&
            fill.operations[0].value_step == 1,
            "RetroArch native repeat mapping is incorrect");

    const CheatEntry& condition = imported.document.entries[1];
    require(condition.retroarch && condition.operations.empty() &&
            condition.retroarch->cheat_type == 6U,
            "RetroArch cross-record condition was not preserved safely");

    const CheatEntry& core = imported.document.entries[2];
    require(core.retroarch && !core.operations.empty() &&
            core.retroarch->code == "82001000 1234",
            "RetroArch handler-0 core code was not preserved and decoded");

    const std::string minimal =
        "cheats = 1\n"
        "cheat0_handler = 1\n"
        "cheat0_address = 0\n"
        "cheat0_value = 1\n";
    const auto minimal_import = native_input::import_file(
        "minimal.cht", bytes(minimal));
    require(minimal_import.recognized && minimal_import.success &&
            minimal_import.document.entries.size() == 1U,
            "RetroArch detection still requires desc/code fields");
}

void test_retroarch_native_roundtrip_and_cli() {
    const std::string input =
        "cheats = 1\n\n"
        "cheat0_desc = \"Native 32\"\n"
        "cheat0_code = \"ignored but preserved\"\n"
        "cheat0_enable = true\n"
        "cheat0_big_endian = false\n"
        "cheat0_handler = 1\n"
        "cheat0_memory_search_size = 5\n"
        "cheat0_cheat_type = 2\n"
        "cheat0_value = 9\n"
        "cheat0_address = 4096\n"
        "cheat0_address_bit_position = 255\n"
        "cheat0_rumble_type = 5\n"
        "cheat0_rumble_value = 6\n"
        "cheat0_rumble_port = 7\n"
        "cheat0_rumble_primary_strength = 8\n"
        "cheat0_rumble_primary_duration = 9\n"
        "cheat0_rumble_secondary_strength = 10\n"
        "cheat0_rumble_secondary_duration = 11\n"
        "cheat0_repeat_count = 12\n"
        "cheat0_repeat_add_to_value = 13\n"
        "cheat0_repeat_add_to_address = 14\n";

    const auto imported = native_input::import_file(
        "source.cht", bytes(input));
    require(imported.success && imported.has_document,
            "RetroArch native round-trip fixture did not import");
    const auto exported = output_modes::export_document(
        imported.document, output_modes::Format::LibretroCht);
    require(exported.success && exported.exported_entries == 1U,
            "RetroArch exact native re-export failed");
    const retroarch_cht::Record roundtrip = only_record(
        retroarch_cht::parse(std::string(exported.data.begin(),
                                         exported.data.end())));
    require(roundtrip.desc == "Native 32" && roundtrip.enabled &&
            roundtrip.handler == 1U && roundtrip.memory_search_size == 5U &&
            roundtrip.cheat_type == 2U && roundtrip.value == 9U &&
            roundtrip.address == 4096U && roundtrip.address_mask == 255U &&
            roundtrip.rumble_type == 5U && roundtrip.rumble_value == 6U &&
            roundtrip.rumble_port == 7U &&
            roundtrip.rumble_primary_strength == 8U &&
            roundtrip.rumble_primary_duration == 9U &&
            roundtrip.rumble_secondary_strength == 10U &&
            roundtrip.rumble_secondary_duration == 11U &&
            roundtrip.repeat_count == 12U &&
            roundtrip.repeat_add_to_value == 13U &&
            roundtrip.repeat_add_to_address == 14U &&
            roundtrip.code == "ignored but preserved",
            "RetroArch exact native fields changed during re-export");

    std::istringstream cli_input(input);
    std::ostringstream cli_output;
    std::ostringstream cli_error;
    const int cli_result = cli::run(
        {"--from", "retroarch-cht", "--to", "retroarch-cht", "-"},
        cli_input, cli_output, cli_error, "GbaCheatConverter.exe");
    require(cli_result == 0,
            "RetroArch native-to-native CLI conversion failed: " +
            cli_error.str());
    const retroarch_cht::Record cli_record = only_record(
        retroarch_cht::parse(cli_output.str()));
    require(cli_record.handler == 1U && cli_record.cheat_type == 2U &&
            cli_record.repeat_count == 12U && cli_record.enabled,
            "RetroArch CLI did not preserve native record fields");
}

void test_retroarch_semantic_export_defaults() {
    CheatDocument document;
    CheatEntry entry;
    entry.name = "Enabled Core Code";
    entry.enabled = true;
    Operation write;
    write.kind = OperationKind::Write;
    write.address = 0x02001000U;
    write.value = 0x1234U;
    write.width = 2U;
    entry.operations.push_back(write);
    document.entries.push_back(entry);

    const auto exported = output_modes::export_document(
        document, output_modes::Format::LibretroCht);
    const retroarch_cht::Record record = only_record(
        retroarch_cht::parse(std::string(exported.data.begin(),
                                         exported.data.end())));
    require(record.enabled && record.handler == 0U &&
            record.memory_search_size == 3U && record.cheat_type == 1U &&
            record.repeat_count == 1U &&
            record.repeat_add_to_address == 1U &&
            record.code.find("82001000 1234") != std::string::npos,
            "semantic RetroArch export does not match official defaults");
}

} // namespace gba::tests
