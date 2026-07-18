#include "test_support.hpp"

namespace gba::tests {

void test_ezflash_enhanced_v4_gameshark_romif_roundtrip() {
    const std::string input =
        "[Stealing Trainer's Pokemon]\n"
        "E00200FD 09014CD3\n"
        "020239F8 00000004\n"
        "60075060 00002000\n";
    const auto source = gba::gameshark::parse(input, {false});
    const auto enhanced = gba::ezflash::export_document(source);
    require(enhanced.success && enhanced.text.find(
                "=ROMIF:09014CD3,FD,00;W8:239F8,04;"
                "ROM:080EA0C0,00,20;") != std::string::npos,
            "GameShark guarded ROM patch did not become Enhanced v4 ROMIF");
    const auto raw = gba::gameshark::export_document(
        gba::ezflash::parse(enhanced.text), {false});
    require(raw.success && raw.text.find("E00200FD 09014CD3") !=
                std::string::npos,
            "Enhanced v4 ROMIF did not round-trip to GameShark");
}

void test_ezflash_enhanced_v4_armax_if_rom_tail() {
    const auto source = gba::armax::parse(
        "[AR MAX Mixed]\n"
        "0A400130 000000FF\n"
        "002239F8 00000004\n"
        "00000000 1801D418\n"
        "00002000 00000000\n", {false});
    const auto enhanced = gba::ezflash::export_document(source);
    require(enhanced.success && enhanced.text.find(
                "=IF:W16,80130,00FF;W8:239F8,04;ENDIF;"
                "ROM:0803A830,00,20;") != std::string::npos,
            "AR MAX condition and independent ROM tail were not emitted");
}

void test_ezflash_enhanced_v4_long_rom_byte_lists() {
    const std::string input =
        "[Long ROM Patch]\n"
        "ON=ROM:08010000,01,02,03,04;ROM:08010004,05,06,07,08;\n";
    const auto parsed = gba::ezflash::parse(input);
    require(parsed.warnings.empty() && parsed.entries.size() == 1U,
            "Long ROM byte list did not parse");
    const auto output = gba::ezflash::export_document(parsed);
    require(output.success && output.text.find(
                "ON=ROM:08010000,01,02,03,04;ROM:08010004,05,06,07,08;") !=
                std::string::npos,
            "Long ROM byte list did not round-trip");
    require(gba::detect::format(input).format == gba::detect::Format::EzFlash,
            "Auto Detect did not recognize grouped ROM syntax");
}

void test_ezflash_enhanced_v4_condition_families_and_else() {
    const std::string input =
        "[Conditions]\n"
        "Choice=IFNE:W16,202,0001;W8:10A78,CC;ELSE;"
        "W8:10A78,64;ENDIF;\n"
        "[Ordered]\n"
        "All=IFLT:W16,204,0001;W8:10A80,01;ENDIF;"
        "IFGT:W16,206,0002;W8:10A81,02;ENDIF;"
        "IFLE:W16,208,0003;W8:10A82,03;ENDIF;"
        "IFGE:W16,20A,0004;W8:10A83,04;ENDIF;\n";
    const auto document = gba::ezflash::parse(input);
    require(document.warnings.empty() && document.entries.size() == 2U,
            "Enhanced v4 condition text did not parse");
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find(
                "Choice=IFNE:W16,202,0001;W8:10A78,CC;ELSE;"
                "W8:10A78,64;ENDIF;") != std::string::npos &&
            output.text.find("IFGE:W16,20A,0004;") != std::string::npos,
            "Enhanced v4 condition families did not round-trip");
}

void test_ezflash_enhanced_v4_arithmetic_pointer_fill_slide() {
    const std::string input =
        "[Operations]\n"
        "All=ADD:W32,10A78,04030201;SUB:W16,10A80,1234;"
        "PTR:W8,10,00000123,7F;"
        "FILL:W16,10000,00000100,1234;"
        "SLIDE:W32,20000,00000100,00000004,00000001,11223344;\n";
    const auto document = gba::ezflash::parse(input);
    require(document.warnings.empty() && document.entries.size() == 1U &&
            document.entries[0].operations.size() == 5U,
            "Compact Enhanced v4 operations did not parse");
    gba::ezflash::Options options;
    options.maximum_runtime_records = 10U;
    const auto output = gba::ezflash::export_document(document, options);
    require(output.success && output.text.find(input.substr(0U, input.size() - 1U)) !=
                std::string::npos,
            "ADD/SUB/PTR/FILL/SLIDE did not stay compact");
}

void test_ezflash_enhanced_v4_romif_with_named_runtime_action() {
    const std::string input =
        "[Guarded Arithmetic]\n"
        "Safe=ROMIF:09014CD3,FD,00;ADD:W8,239F8,01;"
        "ROM:080EA0C0,00,20;\n";
    const auto document = gba::ezflash::parse(input);
    require(document.warnings.empty() && document.entries.size() == 1U &&
            document.entries[0].ezflash_option_name == "Safe",
            "Named ROMIF option did not parse");
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find(
                "Safe=ROMIF:09014CD3,FD,00;ADD:W8,239F8,01;"
                "ROM:080EA0C0,00,20;") != std::string::npos,
            "Named ROMIF option did not round-trip");
}

void test_ezflash_enhanced_v4_rejects_rom_inside_runtime_if() {
    const std::string input =
        "[Unsafe ROM Control]\n"
        "Bad=IF:W8,100,01;ROM:08000200,55;ENDIF;\n";
    const auto parsed = gba::ezflash::parse(input);
    require(parsed.entries.empty() && !parsed.warnings.empty(),
            "ROM inside a runtime IF branch was not rejected");

    gba::CheatDocument document;
    gba::CheatEntry entry;
    entry.name = "Unsafe Semantic ROM";
    gba::Operation condition;
    condition.kind = gba::OperationKind::IfEqual;
    condition.address = 0x02000100U;
    condition.value = 1U;
    condition.width = 1U;
    condition.condition_span = 1U;
    gba::Operation patch;
    patch.kind = gba::OperationKind::RomPatch;
    patch.address = 0x08000200U;
    patch.value = 0x55U;
    patch.width = 1U;
    entry.operations.push_back(condition);
    entry.operations.push_back(patch);
    document.entries.push_back(entry);
    const auto output = gba::ezflash::export_document(document);
    require(!output.success && output.text.empty(),
            "Exporter emitted a ROM patch inside a runtime IF branch");
}


void test_ezflash_v4_masked_condition_families() {
    const std::string input =
        "[Masked Conditions]\n"
        "All=IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;"
        "IFNEM:W8,100,0F,01;W8:101,02;ENDIF;"
        "IFLTM:W16,102,00FF,0010;W8:104,03;ENDIF;"
        "IFGTM:W16,106,00FF,0020;W8:108,04;ENDIF;"
        "IFLEM:W32,10C,0000FFFF,00000100;W8:110,05;ENDIF;"
        "IFGEM:W32,114,FFFF0000,12340000;W8:118,06;ENDIF;\n";

    const auto document = gba::ezflash::parse(input);
    require(document.warnings.empty() && document.entries.size() == 1U,
            "Masked Enhanced condition families did not parse");
    require(document.entries[0].operations.size() == 12U,
            "Masked Enhanced conditions decoded to the wrong operation count");
    for (std::size_t index = 0U; index < 12U; index += 2U) {
        const gba::Operation& condition =
            document.entries[0].operations[index];
        require(condition.condition_has_mask,
                "Masked Enhanced condition lost its mask flag");
    }

    const auto output = gba::ezflash::export_document(document);
    require(output.success &&
            output.text.find(
                "IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;") !=
                std::string::npos &&
            output.text.find("IFNEM:W8,100,0F,01;") != std::string::npos &&
            output.text.find("IFLTM:W16,102,00FF,0010;") !=
                std::string::npos &&
            output.text.find("IFGTM:W16,106,00FF,0020;") !=
                std::string::npos &&
            output.text.find("IFLEM:W32,10C,0000FFFF,00000100;") !=
                std::string::npos &&
            output.text.find("IFGEM:W32,114,FFFF0000,12340000;") !=
                std::string::npos,
            "Masked Enhanced condition families did not round-trip");
}

void test_ezflash_v4_masked_condition_runtime_cost() {
    const auto document = gba::ezflash::parse(
        "[Masked Cost]\n"
        "ON=IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;\n");
    require(document.warnings.empty() && document.entries.size() == 1U,
            "Masked runtime-cost sample did not parse");

    gba::ezflash::Options too_small;
    too_small.maximum_runtime_records = 3U;
    const auto rejected = gba::ezflash::export_document(document, too_small);
    require(!rejected.success && rejected.text.empty(),
            "Masked condition incorrectly fit in fewer than four records");

    gba::ezflash::Options exact;
    exact.maximum_runtime_records = 4U;
    const auto accepted = gba::ezflash::export_document(document, exact);
    require(accepted.success && accepted.text.find("IFM:W16") !=
                std::string::npos,
            "Masked condition did not fit in its exact four-record budget");
}

void test_ezflash_v4_merges_repeated_identical_conditions() {
    const std::string input =
        "[Access Your PC _Press A+R+Up_]\n"
        "ON=IF:W16,80130,02BE;W16:405B8,0201;ENDIF;"
        "IF:W16,80130,02BE;W16:405B0,0000;ENDIF;"
        "IF:W16,80130,02BE;W16:405C0,F7FA;ENDIF;"
        "IF:W16,80130,02BE;W16:405C2,0819;ENDIF;"
        "IF:W16,80130,02BE;W16:405C4,001D;ENDIF;"
        "IF:W16,80130,02BE;W16:405C6,081A;ENDIF;\n";
    const auto parsed = gba::ezflash::parse(input);
    require(parsed.warnings.empty() && parsed.entries.size() == 1U,
            "Repeated identical condition sample did not parse");

    gba::ezflash::Options options;
    options.maximum_runtime_records = 6U;
    const auto output = gba::ezflash::export_document(parsed, options);
    require(output.success && output.text.find(
                "ON=IF:W16,80130,02BE;W16:405B8,0201;"
                "W16:405B0,0000;W32:405C0,0819F7FA;"
                "W32:405C4,081A001D;ENDIF;") != std::string::npos,
            "Repeated identical conditions were not merged and widened");
}

void test_ezflash_v4_condition_merge_preserves_recheck_semantics() {
    const std::string input =
        "[Unsafe Merge]\n"
        "ON=IF:W8,100,01;W8:100,02;ENDIF;"
        "IF:W8,100,01;W8:101,03;ENDIF;\n";
    const auto parsed = gba::ezflash::parse(input);
    require(parsed.warnings.empty() && parsed.entries.size() == 1U,
            "Condition self-write safety sample did not parse");
    const auto output = gba::ezflash::export_document(parsed);
    const std::string condition = "IF:W8,100,01;";
    const std::size_t first = output.text.find(condition);
    const std::size_t second = first == std::string::npos
        ? std::string::npos : output.text.find(condition, first + 1U);
    require(output.success && first != std::string::npos &&
            second != std::string::npos,
            "Conditions were merged even though the first branch changes "
            "the compared address");
}

void test_armax_encrypted_mirrored_ewram_to_ez() {
    const std::string input =
        "[Mirrored EWRAM]\n"
        "3C8BBA54 A8648690\n";
    const auto document = gba::armax::parse(input, {true});
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 1U &&
            document.entries[0].operations[0].kind ==
                gba::OperationKind::Write &&
            document.entries[0].operations[0].address == 0x020426ECU &&
            document.entries[0].operations[0].value == 0x63U,
            "Encrypted AR MAX mirror sample did not decrypt as expected");

    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.warnings.empty() &&
            output.text.find("=W8:26EC,63;") != std::string::npos,
            "Mirrored EWRAM write did not canonicalize for EZ-Flash");
}

void test_ezflash_v4_ram_mirror_canonicalization() {
    gba::CheatDocument document;
    gba::CheatEntry entry;
    entry.name = "RAM Mirrors";

    gba::Operation ewram;
    ewram.kind = gba::OperationKind::Write;
    ewram.address = 0x020826ECU;
    ewram.value = 0x63U;
    ewram.width = 1U;
    entry.operations.push_back(ewram);

    gba::Operation iwram;
    iwram.kind = gba::OperationKind::Write;
    iwram.address = 0x03008010U;
    iwram.value = 0x1234U;
    iwram.width = 2U;
    entry.operations.push_back(iwram);

    gba::Operation condition;
    condition.kind = gba::OperationKind::IfEqual;
    condition.address = 0x02040020U;
    condition.value = 1U;
    condition.width = 1U;
    condition.condition_span = 1U;
    entry.operations.push_back(condition);

    gba::Operation controlled;
    controlled.kind = gba::OperationKind::Write;
    controlled.address = 0x03010022U;
    controlled.value = 2U;
    controlled.width = 1U;
    entry.operations.push_back(controlled);

    document.entries.push_back(entry);
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.warnings.empty() &&
            output.text.find("W8:26EC,63;") != std::string::npos &&
            output.text.find("W16:40010,1234;") != std::string::npos &&
            output.text.find(
                "IF:W8,20,01;W8:40022,02;ENDIF;") !=
                std::string::npos,
            "EWRAM/IWRAM mirrors were not canonicalized consistently");
}

void test_ezflash_v4_mirror_alias_preserves_recheck_semantics() {
    gba::CheatDocument document;
    gba::CheatEntry entry;
    entry.name = "Mirror Alias Safety";

    gba::Operation first_condition;
    first_condition.kind = gba::OperationKind::IfEqual;
    first_condition.address = 0x02000020U;
    first_condition.value = 1U;
    first_condition.width = 1U;
    first_condition.condition_span = 1U;
    entry.operations.push_back(first_condition);

    gba::Operation alias_write;
    alias_write.kind = gba::OperationKind::Write;
    alias_write.address = 0x02040020U;
    alias_write.value = 2U;
    alias_write.width = 1U;
    entry.operations.push_back(alias_write);

    gba::Operation second_condition = first_condition;
    entry.operations.push_back(second_condition);

    gba::Operation second_write;
    second_write.kind = gba::OperationKind::Write;
    second_write.address = 0x02000021U;
    second_write.value = 3U;
    second_write.width = 1U;
    entry.operations.push_back(second_write);

    document.entries.push_back(entry);
    const auto output = gba::ezflash::export_document(document);
    const std::string marker = "IF:W8,20,01;";
    const std::size_t first = output.text.find(marker);
    const std::size_t second = first == std::string::npos
        ? std::string::npos : output.text.find(marker, first + 1U);
    require(output.success && first != std::string::npos &&
            second != std::string::npos,
            "Optimizer merged conditions across a mirrored alias write");
}


void test_ezflash_v4_mirror_boundary_splits_byte_run() {
    gba::CheatDocument document;
    gba::CheatEntry entry;
    entry.name = "Mirror Boundary";

    gba::Operation first_write;
    first_write.kind = gba::OperationKind::Write;
    first_write.address = 0x0207FFFFU;
    first_write.value = 0xAAU;
    first_write.width = 1U;
    entry.operations.push_back(first_write);

    gba::Operation second_write;
    second_write.kind = gba::OperationKind::Write;
    second_write.address = 0x02080000U;
    second_write.value = 0xBBU;
    second_write.width = 1U;
    entry.operations.push_back(second_write);
    document.entries.push_back(entry);

    const auto enhanced = gba::ezflash::export_document(document);
    require(enhanced.success &&
            enhanced.text.find("=W8:3FFFF,AA;W8:0,BB;") !=
                std::string::npos,
            "Enhanced output did not split a write at an EWRAM mirror boundary");

    gba::ezflash::Options original_options;
    original_options.mode = gba::ezflash::Mode::Original;
    const auto original = gba::ezflash::export_document(
        document, original_options);
    require(original.success &&
            original.text.find("ON=3FFFF,AA;0,BB;") != std::string::npos,
            "Original output did not split a byte run at an EWRAM mirror boundary");
}

} // namespace gba::tests
