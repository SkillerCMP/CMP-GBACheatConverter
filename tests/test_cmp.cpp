#include "test_support.hpp"

namespace gba::tests {

void test_cmp_input_code_type_awareness() {
    const std::string input =
        "!Access:\n"
        "$82001000 1234\n"
        "+Slide Code\n"
        "%Credits: Skiller\n"
        "$42002000 1000\n"
        "$00010004 0002\n"
        "!!\n";

    const gba::cmp::NormalizedInput normalized =
        gba::cmp::normalize_input(input);
    require(normalized.recognized, "CMP input was not recognized");

    gba::CheatDocument parsed =
        gba::codebreaker::parse(normalized.text, {false});
    const gba::CheatDocument document =
        gba::cmp::attach_layout(normalized, std::move(parsed));

    require(document.cmp_groups.size() == 1U,
            "CMP group header was not retained");
    require(document.cmp_groups[0].path ==
                std::vector<std::string>{"Access"},
            "CMP group path was not retained");
    require(document.cmp_groups[0].header_operations.size() == 1U &&
                document.cmp_groups[0].header_operations[0].kind ==
                    gba::OperationKind::Write,
            "CMP group header code was not decoded semantically");
    require(document.entries.size() == 1U,
            "CMP code entry was not retained");
    require(document.entries[0].name == "Slide Code" &&
                document.entries[0].credits == "Skiller" &&
                document.entries[0].cmp_group_path ==
                    std::vector<std::string>{"Access"},
            "CMP entry metadata was not retained");
    require(document.entries[0].operations.size() == 1U &&
                document.entries[0].operations[0].repeat == 4U &&
                document.entries[0].operations[0].address_step == 2 &&
                document.entries[0].operations[0].value_step == 1,
            "CMP continuation rows were not decoded as one FCD slide");

    const gba::CheatDocument flattened =
        gba::cmp::flatten_for_device_output(document);
    require(flattened.entries[0].operations.size() == 2U &&
                flattened.entries[0].operations[0].address == 0x02001000U,
            "CMP inherited group code was not prepended to the entry");
}

void test_cmp_output_wrapper_and_credits() {
    gba::CheatDocument document;

    gba::CmpGroup group;
    group.path = {"Access"};
    group.order = 1U;
    gba::Operation header;
    header.kind = gba::OperationKind::Write;
    header.address = 0x02001000U;
    header.value = 0x1234U;
    header.width = 2U;
    group.header_operations.push_back(header);
    document.cmp_groups.push_back(group);

    gba::CheatEntry entry;
    entry.name = "Infinite Health";
    entry.credits = "Skiller";
    entry.cmp_group_path = {"Access"};
    entry.cmp_order = 2U;
    gba::Operation write;
    write.kind = gba::OperationKind::Write;
    write.address = 0x02002000U;
    write.value = 0x63U;
    write.width = 1U;
    entry.operations.push_back(write);
    document.entries.push_back(entry);

    const gba::cmp::PreparedOutput prepared =
        gba::cmp::prepare_output(document);
    gba::codebreaker::ExportOptions export_options;
    export_options.encrypted = false;
    const auto exported = gba::codebreaker::export_document(
        prepared.document, export_options);
    const std::string output =
        gba::cmp::render_output(exported.text, prepared);

    require(output.find("!Access:\n") != std::string::npos &&
                output.find("$82001000 1234\n") != std::string::npos &&
                output.find("+Infinite Health\n") != std::string::npos &&
                output.find("%Credits: Skiller\n") != std::string::npos &&
                output.find("$32002000 0063\n") != std::string::npos &&
                output.find("!!\n") != std::string::npos,
            "CMP output wrapper did not emit group, credit, and $ rows");
}

void test_cmp_to_ezflash_group_mapping() {
    const std::string input =
        "!Starting Lives:\n"
        "+One\n"
        "$32025BC4 0001\n"
        "+Three\n"
        "$32025BC4 0003\n"
        "+Nine\n"
        "$32025BC4 0009\n"
        "!!\n";

    const gba::cmp::NormalizedInput normalized =
        gba::cmp::normalize_input(input);
    gba::CheatDocument document = gba::cmp::attach_layout(
        normalized, gba::codebreaker::parse(normalized.text, {false}));
    document = gba::cmp::prepare_for_ezflash(document);

    gba::ezflash::Options options;
    options.mode = gba::ezflash::Mode::Enhanced;
    const auto output = gba::ezflash::export_document(document, options);

    require(output.success &&
                output.text.find("[Starting Lives]\n") !=
                    std::string::npos &&
                output.text.find("One=W8:25BC4,01;") !=
                    std::string::npos &&
                output.text.find("Three=W8:25BC4,03;") !=
                    std::string::npos &&
                output.text.find("Nine=W8:25BC4,09;") !=
                    std::string::npos,
            "CMP group did not map to EZ-Flash grouped options");
}

void test_ezflash_group_to_cmp_output() {
    const gba::CheatDocument document = gba::ezflash::parse(
        "[Starting Lives]\n"
        "One=W8:25BC4,01;\n"
        "Three=W8:25BC4,03;\n");
    const gba::cmp::PreparedOutput prepared =
        gba::cmp::prepare_output(document);
    gba::codebreaker::ExportOptions export_options;
    export_options.encrypted = false;
    const auto exported = gba::codebreaker::export_document(
        prepared.document, export_options);
    const std::string output =
        gba::cmp::render_output(exported.text, prepared);

    require(output.find("!Starting Lives:\n") != std::string::npos &&
                output.find("+One\n$32025BC4 0001\n") !=
                    std::string::npos &&
                output.find("+Three\n$32025BC4 0003\n") !=
                    std::string::npos,
            "EZ-Flash group did not map to CMP group output");
}


void test_cmp_ezflash_e7_one_roundtrip() {
    const std::string cmp_input =
        "!Starting Lives|ONE:\n"
        "+One\n"
        "$32025BC4 0001\n"
        "+Three\n"
        "$32025BC4 0003\n"
        "!!\n";
    const auto normalized = gba::cmp::normalize_input(cmp_input);
    const gba::CheatDocument cmp_document = gba::cmp::attach_layout(
        normalized, gba::codebreaker::parse(normalized.text, {false}));
    const auto prepared_for_ez = gba::cmp::prepare_for_ezflash(cmp_document);
    require(prepared_for_ez.entries.size() == 2U &&
                prepared_for_ez.entries[0].ezflash_group_name ==
                    "Starting Lives" &&
                prepared_for_ez.entries[0].ezflash_group_mode ==
                    gba::EzFlashGroupMode::ZeroOrOne,
            "CMP |ONE group did not map to the E7 zero-or-one mode");
    const auto ez_output = gba::ezflash::export_document(prepared_for_ez);
    require(ez_output.success &&
                ez_output.text.find("[Starting Lives|ONE]\n") !=
                    std::string::npos,
            "CMP |ONE group did not export as an E7 |ONE heading");

    const auto reparsed = gba::ezflash::parse(ez_output.text);
    const auto cmp_prepared = gba::cmp::prepare_output(reparsed);
    gba::codebreaker::ExportOptions options;
    const auto encoded = gba::codebreaker::export_document(
        cmp_prepared.document, options);
    const std::string cmp_output = gba::cmp::render_output(
        encoded.text, cmp_prepared);
    require(cmp_output.find("!Starting Lives|ONE:\n") !=
                std::string::npos,
            "E7 |ONE mode was not preserved when exporting back to CMP");
}

void test_cmp_nested_groups_and_inheritance() {
    const std::string input =
        "!Parent:\n"
        "$82001000 1111\n"
        "!Child:\n"
        "$82001002 2222\n"
        "+Nested Code\n"
        "$82001004 3333\n"
        "!!\n"
        "!!\n";
    const auto normalized = gba::cmp::normalize_input(input);
    const gba::CheatDocument document = gba::cmp::attach_layout(
        normalized, gba::codebreaker::parse(normalized.text, {false}));
    require(document.entries.size() == 1U &&
                document.entries[0].cmp_group_path ==
                    std::vector<std::string>({"Parent", "Child"}),
            "Nested CMP group path was not retained");
    const gba::CheatDocument flattened =
        gba::cmp::flatten_for_device_output(document);
    require(flattened.entries[0].operations.size() == 3U &&
                flattened.entries[0].operations[0].value == 0x1111U &&
                flattened.entries[0].operations[1].value == 0x2222U &&
                flattened.entries[0].operations[2].value == 0x3333U,
            "Nested CMP header codes were not inherited outer-to-inner");

    const auto prepared = gba::cmp::prepare_output(document);
    gba::codebreaker::ExportOptions options;
    const auto exported =
        gba::codebreaker::export_document(prepared.document, options);
    const std::string output =
        gba::cmp::render_output(exported.text, prepared);
    require(output.find("!Parent:\n$82001000 1111\n!Child:\n") !=
                std::string::npos &&
                output.find("$82001002 2222\n+Nested Code\n") !=
                    std::string::npos,
            "Nested CMP groups were not rendered in hierarchy order");
}

void test_cmp_encrypted_fcd_seed_is_group_header() {
    gba::CheatDocument document;
    gba::CheatEntry entry;
    entry.name = "Encrypted Code";
    gba::Operation write;
    write.kind = gba::OperationKind::Write;
    write.address = 0x02001000U;
    write.value = 0x1234U;
    write.width = 2U;
    entry.operations.push_back(write);
    document.entries.push_back(entry);

    const auto prepared = gba::cmp::prepare_output(document);
    gba::codebreaker::ExportOptions options;
    options.encrypted = true;
    options.seed = gba::codebreaker::Seed{0x9ABCDEF0U, 0x1234U};
    const auto exported =
        gba::codebreaker::export_document(prepared.document, options);
    const std::string output =
        gba::cmp::render_output(exported.text, prepared);
    require(output.rfind("!Codes:\n$9ABCDEF0 1234\n", 0U) == 0U &&
                output.find("+Encrypted Code\n$") != std::string::npos,
            "Encrypted FCD seed was not retained as CMP group header code");
}

void test_cmp_original_ez_single_on_is_normal_code() {
    const gba::CheatDocument document = gba::ezflash::parse(
        "[Infinite Health]\n"
        "ON=25BC4,63;\n");
    const auto prepared = gba::cmp::prepare_output(document);
    gba::codebreaker::ExportOptions options;
    const auto exported =
        gba::codebreaker::export_document(prepared.document, options);
    const std::string output =
        gba::cmp::render_output(exported.text, prepared);
    require(output.find("!Codes:\n+Infinite Health\n") !=
                std::string::npos &&
                output.find("!Infinite Health:\n+ON\n") ==
                    std::string::npos,
            "Single ON= EZ-Flash section was incorrectly treated as a choice group");
}

void test_cmp_gameshark_and_armax_multiline_awareness() {
    const std::string gameshark_cmp =
        "!Lists:\n"
        "+Assignment\n"
        "$30000004 02004000\n"
        "$02001000 02002000\n"
        "$02003000 00000000\n"
        "!!\n";
    const auto gs_normalized = gba::cmp::normalize_input(gameshark_cmp);
    const gba::CheatDocument gs_document = gba::cmp::attach_layout(
        gs_normalized,
        gba::gameshark::parse(gs_normalized.text, {false}));
    require(gs_document.entries.size() == 1U &&
                gs_document.entries[0].operations.size() == 4U &&
                std::all_of(
                    gs_document.entries[0].operations.begin(),
                    gs_document.entries[0].operations.end(),
                    [](const gba::Operation& operation) {
                        return operation.encoding_hint ==
                            gba::EncodingHint::GameSharkAssignmentList;
                    }),
            "CMP split a GameShark assignment-list continuation");

    const std::string armax_cmp =
        "!Blocks:\n"
        "+IF ELSE\n"
        "$8A200010 00001234\n"
        "$04200100 11111111\n"
        "$00000000 60000000\n"
        "$04200104 22222222\n"
        "$00000000 40000000\n"
        "!!\n";
    const auto ar_normalized = gba::cmp::normalize_input(armax_cmp);
    const gba::CheatDocument ar_document = gba::cmp::attach_layout(
        ar_normalized,
        gba::armax::parse(ar_normalized.text, {false}));
    require(ar_document.entries.size() == 1U &&
                ar_document.entries[0].operations.size() == 3U &&
                ar_document.entries[0].operations[0].condition_has_else &&
                ar_document.entries[0].operations[0].condition_span == 1U &&
                ar_document.entries[0].operations[0].condition_else_span == 1U,
            "CMP split an Action Replay MAX IF/ELSE block");
}

void test_cmp_output_wrapper_all_device_families() {
    gba::CheatDocument document;
    gba::CheatEntry entry;
    entry.name = "Universal";
    entry.credits = "Skiller";
    gba::Operation write;
    write.kind = gba::OperationKind::Write;
    write.address = 0x02001000U;
    write.value = 0x12U;
    write.width = 1U;
    entry.operations.push_back(write);
    document.entries.push_back(entry);

    const auto check_wrapped = [](const std::string& exported,
                                  const gba::cmp::PreparedOutput& prepared,
                                  const char* family) {
        const std::string wrapped =
            gba::cmp::render_output(exported, prepared);
        require(wrapped.rfind("!Codes:\n+Universal\n%Credits: Skiller\n$", 0U) == 0U &&
                    wrapped.find("\n!!\n") != std::string::npos,
                std::string("CMP wrapper failed for ") + family);
    };

    {
        const auto prepared = gba::cmp::prepare_output(document);
        const auto exported =
            gba::gameshark::export_document(prepared.document, {false});
        require(exported.success, "GameShark CMP source export failed");
        check_wrapped(exported.text, prepared, "GameShark/AR GBX");
    }
    {
        const auto prepared = gba::cmp::prepare_output(document);
        const auto exported =
            gba::armax::export_document(prepared.document, {false});
        require(exported.success, "AR MAX CMP source export failed");
        check_wrapped(exported.text, prepared, "Action Replay MAX");
    }
    {
        const auto prepared = gba::cmp::prepare_output(document);
        gba::xploder::ExportOptions options;
        options.encrypted = false;
        const auto exported =
            gba::xploder::export_document(prepared.document, options);
        require(exported.success, "Xploder CMP source export failed");
        check_wrapped(exported.text, prepared, "Xploder Advance");
    }
}


void test_cmp_relaxed_groups_and_ezflash_credits() {
    const std::string input =
        "100% Health , by Shimer , Crypt_Codebreaker/GameShark SP/Xploder\n"
        "32005924 0064\n"
        "!Have:\n"
        "Golden Aku Aku Mask , by MadCatz , Crypt_Codebreaker/GameShark SP/Xploder\n"
        "320026F0 0002\n"
        "+All Powers\n"
        "%Credits: Skiller\n"
        "$320027CB 00FF\n"
        "!!\n";

    const gba::cmp::NormalizedInput normalized =
        gba::cmp::normalize_input(input);
    require(normalized.recognized,
            "Relaxed !Group:/!! input without +/$ prefixes was not recognized");

    gba::CheatDocument document = gba::cmp::attach_layout(
        normalized, gba::codebreaker::parse(normalized.text, {false}));
    require(document.entries.size() == 3U,
            "Relaxed grouped database entries were not retained");
    require(document.entries[0].cmp_group_path.empty() &&
                document.entries[1].cmp_group_path ==
                    std::vector<std::string>{"Have"} &&
                document.entries[2].cmp_group_path ==
                    std::vector<std::string>{"Have"},
            "Relaxed !Have: group membership was not retained");

    document = gba::cmp::prepare_for_ezflash(document);
    const auto output = gba::ezflash::export_document(document);
    require(output.success,
            "Relaxed grouped database did not export to EZ-Flash E7");
    require(output.text.find(
                "// Credits: Shimer\n"
                "100% Health=W8:5924,64;\n") != std::string::npos,
            "Inline author/crypt metadata was not cleaned for standalone E7 output");
    require(output.text.find(
                "[Have]\n"
                "// Credits: MadCatz\n"
                "Golden Aku Aku Mask=W8:26F0,02;\n"
                "// Credits: Skiller\n"
                "All Powers=W8:27CB,FF;\n") != std::string::npos,
            "!Have: entries or E7 credit comments were not emitted correctly");
    require(output.text.find("Crypt_") == std::string::npos &&
                output.text.find(" , by ") == std::string::npos,
            "Inline metadata leaked into the E7 code name");
}

} // namespace gba::tests
