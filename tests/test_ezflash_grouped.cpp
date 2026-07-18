#include "test_support.hpp"

namespace gba::tests {

void test_direct_write_merge() {
    const std::string input =
        "Infinite Money:\n"
        "82025BC4 423F\n"
        "82025BC6 000F\n";
    const auto document = gba::codebreaker::parse(input, {false});
    const auto output = gba::ezflash::export_document(document);
    require(output.success &&
            output.text.find("Infinite Money=W32:25BC4,000F423F;") !=
                std::string::npos,
            "Adjacent aligned writes were not widened into one W32 operation");
}

void test_condition() {
    const auto document = gba::codebreaker::parse(
        "Conditional:\n72024C10 9EB7\n32024C10 00C2\n", {false});
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find(
                "Conditional=IF:W16,24C10,9EB7;W8:24C10,C2;ENDIF;") !=
                std::string::npos,
            "Width-aware condition was not emitted");
}

void test_ezflash_v4_multiple_conditions() {
    const std::string input =
        "[Modes]\n"
        "First=IF:W16,202,0001;W16:300,1234;ENDIF;"
        "IFNE:W8,204,02;W8:302,44;ENDIF;\n";
    const auto parsed = gba::ezflash::parse(input);
    require(parsed.warnings.empty() && parsed.entries.size() == 1U,
            "Multiple Enhanced v4 conditions did not parse");
    const auto output = gba::ezflash::export_document(parsed);
    require(output.success && output.text.find(input.substr(0U, input.size() - 1U)) !=
                std::string::npos,
            "Multiple Enhanced v4 conditions did not round-trip");
}

void test_optional_split_mode() {
    gba::CheatEntry first{"One", {gba::Operation{}}};
    first.operations[0].kind = gba::OperationKind::Write;
    first.operations[0].address = 0x02000100U;
    first.operations[0].value = 1U;
    first.operations[0].width = 1U;
    first.ezflash_group_name = "Starting Lives";
    first.ezflash_option_name = "One";
    gba::CheatEntry three = first;
    three.name = "Three";
    three.operations[0].value = 3U;
    three.ezflash_option_name = "Three";
    const auto output = gba::ezflash::export_document({{first, three}, {}});
    require(output.success && output.text.find(
                "[Starting Lives]\nOne=W8:100,01;\nThree=W8:100,03;") !=
                std::string::npos,
            "EZ-Flash multi-select group was not preserved");
}

void test_ezflash_v4_compound_condition_roundtrip() {
    const std::string input =
        "[Compound]\n"
        "ON=IF:W16,200,1234;IF:W8,204,56;W32:300,89ABCDEF;"
        "ENDIF;ENDIF;\n";
    const auto parsed = gba::ezflash::parse(input);
    require(parsed.warnings.empty() && parsed.entries.size() == 1U,
            "Nested equality conditions did not parse");
    const auto output = gba::ezflash::export_document(parsed);
    require(output.success && output.text.find(
                "ON=IF:W16,200,1234;IF:W8,204,56;W32:300,89ABCDEF;"
                "ENDIF;ENDIF;") != std::string::npos,
            "Nested equality conditions did not round-trip");
}

void test_ezflash_v4_transactional_parse() {
    const auto parsed = gba::ezflash::parse(
        "[Bad]\nON=W32:101,12345678;W8:200,01;\n");
    require(parsed.entries.empty() && !parsed.warnings.empty(),
            "Malformed aligned option leaked a partial entry");
}

void test_ezflash_v4_multiline_continuation() {
    const auto parsed = gba::ezflash::parse(
        "[Long]\nON=W8:100,01;\nW16:102,1234;\nW32:104,89ABCDEF;\n");
    require(parsed.warnings.empty() && parsed.entries.size() == 1U &&
            parsed.entries[0].operations.size() == 3U,
            "Enhanced continuation rows were not joined");
}

void test_ezflash_v4_comment_and_terminator_rules() {
    const auto parsed = gba::ezflash::parse(
        "# heading\n[Good]\nON=W8:100,01; # note\n-- end\n"
        "[Ignored]\nON=W8:101,02;\n");
    require(parsed.entries.size() == 1U &&
            parsed.entries[0].ezflash_group_name == "Good",
            "Comments or database terminator were not handled");
}

void test_ezflash_v4_compact_range_boundaries() {
    const auto valid = gba::ezflash::parse(
        "[Bounds]\nA=W8:3FFFF,AA;\nB=W8:40000,BB;\n"
        "C=W8:47FFF,CC;\nD=IF:W8,803FF,01;W8:100,02;ENDIF;\n");
    require(valid.warnings.empty() && valid.entries.size() == 4U,
            "Valid compact range boundaries were rejected");
    const auto invalid = gba::ezflash::parse(
        "[Bad]\nA=W16:3FFFF,1234;\nB=W32:47FFC,12345678;\n");
    require(invalid.entries.size() == 1U && !invalid.warnings.empty(),
            "Invalid cross-boundary width was not rejected");
}

void test_ezflash_v4_runtime_limit() {
    gba::CheatEntry entry;
    entry.name = "Many 32-bit Writes";
    for (std::uint32_t index = 0U; index < 128U; ++index) {
        gba::Operation write;
        write.kind = gba::OperationKind::Write;
        write.address = 0x02010000U + index * 4U;
        write.value = index;
        write.width = 4U;
        entry.operations.push_back(write);
    }
    const auto accepted = gba::ezflash::export_document({{entry}, {}});
    require(accepted.success,
            "128 W32 operations should fit the compact 128-record table");
    entry.operations.push_back(entry.operations.back());
    entry.operations.back().address += 4U;
    const auto rejected = gba::ezflash::export_document({{entry}, {}});
    require(!rejected.success && rejected.text.empty(),
            "129 compact runtime records were not rejected transactionally");
}

void test_ezflash_v4_physical_line_wrapping() {
    gba::CheatEntry entry;
    entry.name = "Wrapped";
    for (std::uint32_t index = 0U; index < 40U; ++index) {
        gba::Operation write;
        write.kind = gba::OperationKind::Write;
        write.address = 0x02001000U + index;
        write.value = index;
        write.width = 1U;
        entry.operations.push_back(write);
    }
    gba::ezflash::Options options;
    options.maximum_physical_line_length = 64U;
    const auto output = gba::ezflash::export_document({{entry}, {}}, options);
    require(output.success && output.text.find("\nW32:") != std::string::npos,
            "Enhanced option was not wrapped at command boundaries");
}

void test_ezflash_v4_section_names() {
    const auto parsed = gba::ezflash::parse(
        "[Starting Lives]\nOne Life=W8:100,01;\nThree Lives=W8:100,03;\n");
    require(parsed.entries.size() == 2U &&
            parsed.entries[0].ezflash_group_name == "Starting Lives" &&
            parsed.entries[0].ezflash_option_name == "One Life",
            "Group and option names with spaces were not retained");
    const auto output = gba::ezflash::export_document(parsed);
    require(output.text.find("[Starting Lives]\nOne Life=") !=
                std::string::npos,
            "Group and option names with spaces were not exported");
}

void test_ezflash_v4_shared_table_warning() {
    gba::CheatDocument document;
    for (std::uint32_t group = 0U; group < 2U; ++group) {
        gba::CheatEntry entry;
        entry.name = "Group " + std::to_string(group);
        for (std::uint32_t index = 0U; index < 80U; ++index) {
            gba::Operation write;
            write.kind = gba::OperationKind::Write;
            write.address = 0x02000000U + group * 0x200U + index * 4U;
            write.value = index;
            write.width = 1U;
            entry.operations.push_back(write);
        }
        document.entries.push_back(entry);
    }
    const auto output = gba::ezflash::export_document(document);
    require(output.success && std::any_of(
                output.warnings.begin(), output.warnings.end(),
                [](const std::string& warning) {
                    return warning.find("shares one 128-record") !=
                           std::string::npos;
                }),
            "Shared runtime-table warning was not reported");
}

void test_ezflash_v4_compact_runtime_work_budget() {
    const auto accepted = gba::ezflash::parse(
        "[Fill]\nSafe=FILL:W32,10000,00001000,11223344;\n");
    require(accepted.warnings.empty() && accepted.entries.size() == 1U,
            "Maximum compact runtime-work option did not parse");
    const auto accepted_output = gba::ezflash::export_document(accepted);
    require(accepted_output.success,
            "Maximum compact runtime-work option was not exported");

    const auto rejected = gba::ezflash::parse(
        "[Fill]\nToo Much=FILL:W32,10000,00001001,11223344;\n");
    require(rejected.warnings.empty() && rejected.entries.size() == 1U,
            "Over-budget compact option should remain syntactically valid");
    const auto rejected_output = gba::ezflash::export_document(rejected);
    require(!rejected_output.success && rejected_output.text.empty(),
            "Over-budget compact runtime work was not rejected");
}

void test_ezflash_v4_group_capacity_uses_largest_option() {
    gba::CheatDocument document;
    for (std::uint32_t option = 0U; option < 3U; ++option) {
        gba::CheatEntry entry;
        entry.name = "Choice " + std::to_string(option);
        entry.ezflash_group_name = "Exclusive Choices";
        entry.ezflash_option_name = entry.name;
        entry.ezflash_group_mode = gba::EzFlashGroupMode::ZeroOrOne;
        for (std::uint32_t index = 0U; index < 80U; ++index) {
            gba::Operation write;
            write.kind = gba::OperationKind::Write;
            write.address = 0x02000000U + option * 0x100U + index;
            write.value = index;
            write.width = 1U;
            entry.operations.push_back(write);
        }
        document.entries.push_back(entry);
    }
    const auto output = gba::ezflash::export_document(document);
    require(output.success &&
                output.text.find("[Exclusive Choices|ONE]\n") !=
                    std::string::npos &&
                std::none_of(
                output.warnings.begin(), output.warnings.end(),
                [](const std::string& warning) {
                    return warning.find("largest selectable combination") !=
                           std::string::npos;
                }),
            "E7 |ONE sibling options were incorrectly summed");
}


void test_ezflash_e7_revision6_structure() {
    const std::string input =
        "moonjump=IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;\n"
        "infinitehealth=W16:10A78,0064;\n"
        "\n"
        "[Independent Movement Codes]\n"
        "Walk Through Walls=W8:15060,01;\n"
        "Fast Movement=ADD:W16,15062,0001;\n"
        "\n"
        "[Starting Lives|ONE]\n"
        "1 Life=W8:15070,01;\n"
        "3 Lives=W8:15070,03;\n"
        "9 Lives=W8:15070,09;\n";

    const auto parsed = gba::ezflash::parse(input);
    require(parsed.warnings.empty() && parsed.entries.size() == 7U,
            "E7 standalone/grouped revision-6 database did not parse");
    require(gba::detect::format(input).format ==
                gba::detect::Format::EzFlash,
            "Auto Detect did not recognize E7 standalone CodeName rows");
    const std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const auto imported = gba::native_input::import_file("revision6.cht", bytes);
    require(imported.recognized && imported.success &&
                imported.source_format ==
                    gba::native_input::SourceFormat::EzFlashCht &&
                imported.document.entries.size() == 7U,
            "Native .cht auto-detection did not import E7 standalone/grouped syntax");
    require(parsed.entries[0].name == "moonjump" &&
                parsed.entries[0].ezflash_group_name.empty() &&
                parsed.entries[0].ezflash_group_mode ==
                    gba::EzFlashGroupMode::None,
            "E7 standalone CodeName=commands row was not retained");
    require(parsed.entries[2].ezflash_group_name ==
                "Independent Movement Codes" &&
                parsed.entries[2].ezflash_group_mode ==
                    gba::EzFlashGroupMode::MultiSelect,
            "Plain E7 group was not marked multi-select");
    require(parsed.entries[4].ezflash_group_name == "Starting Lives" &&
                parsed.entries[4].ezflash_group_mode ==
                    gba::EzFlashGroupMode::ZeroOrOne,
            "E7 |ONE group was not normalized and retained");

    const auto output = gba::ezflash::export_document(parsed);
    require(output.success &&
                output.text.rfind(
                    "moonjump=IFM:W16,80130,0001,0000;", 0U) == 0U &&
                output.text.find("[Independent Movement Codes]\n") !=
                    std::string::npos &&
                output.text.find("[Starting Lives|ONE]\n") !=
                    std::string::npos,
            "E7 standalone, multi-select, or |ONE syntax did not round-trip");
}

void test_ezflash_e7_one_suffix_is_case_sensitive() {
    const auto parsed = gba::ezflash::parse(
        "[Modes|one]\nA=W8:100,01;\n");
    require(parsed.warnings.empty() && parsed.entries.size() == 1U &&
                parsed.entries[0].ezflash_group_name == "Modes|one" &&
                parsed.entries[0].ezflash_group_mode ==
                    gba::EzFlashGroupMode::MultiSelect,
            "Lowercase |one was incorrectly treated as E7 |ONE");
}

void test_ezflash_e7_multiselect_capacity_sums_siblings() {
    gba::CheatDocument document;
    for (std::uint32_t option = 0U; option < 2U; ++option) {
        gba::CheatEntry entry;
        entry.name = "Multi " + std::to_string(option);
        entry.ezflash_group_name = "Multi Select";
        entry.ezflash_option_name = entry.name;
        entry.ezflash_group_mode = gba::EzFlashGroupMode::MultiSelect;
        for (std::uint32_t index = 0U; index < 80U; ++index) {
            gba::Operation write;
            write.kind = gba::OperationKind::Write;
            write.address = 0x02000000U + option * 0x10000U +
                index * index * 4U + index;
            write.value = (index * 37U) ^ (option * 0x55U);
            write.width = 1U;
            entry.operations.push_back(write);
        }
        document.entries.push_back(std::move(entry));
    }
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find("[Multi Select]\n") !=
                std::string::npos &&
                std::any_of(
                    output.warnings.begin(), output.warnings.end(),
                    [](const std::string& warning) {
                        return warning.find("largest selectable combination") !=
                               std::string::npos;
                    }),
            "Plain E7 group siblings were not summed as simultaneous selections");
}

void test_ezflash_e7_one_header_limit_includes_suffix() {
    gba::CheatEntry entry;
    entry.name = "Choice";
    entry.ezflash_group_name = std::string(49U, 'A');
    entry.ezflash_option_name = "Choice";
    entry.ezflash_group_mode = gba::EzFlashGroupMode::ZeroOrOne;
    gba::Operation write;
    write.kind = gba::OperationKind::Write;
    write.address = 0x02000100U;
    write.value = 1U;
    write.width = 1U;
    entry.operations.push_back(write);

    const auto output = gba::ezflash::export_document({{entry}, {}});
    const std::size_t open = output.text.find('[');
    const std::size_t close = output.text.find(']', open);
    require(output.success && open != std::string::npos &&
                close != std::string::npos && close - open - 1U == 49U &&
                output.text.substr(close - 4U, 4U) == "|ONE",
            "E7 |ONE suffix was not counted in the 49-byte physical header limit");
}

} // namespace gba::tests
