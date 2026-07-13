#include "test_support.hpp"

namespace gba::tests {

void test_direct_write_merge() {
    const std::string input =
        "Infinite Money:\n"
        "82025BC4 423F\n"
        "82025BC6 000F\n";

    const auto doc = gba::codebreaker::parse(input, {false});
    const auto ez = gba::ezflash::export_document(doc);

    require(ez.text.find("ON=25BC4,3F,42,0F,00;") != std::string::npos,
            "Adjacent 16-bit writes were not merged");
}

void test_condition() {
    const std::string input =
        "Conditional:\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n";

    const auto doc = gba::codebreaker::parse(input, {false});
    const auto ez = gba::ezflash::export_document(doc);

    require(ez.text.find(
        "IF=24C10,B7,9E;ON=24C10,C2;") != std::string::npos,
        "CodeBreaker equal condition was not converted");
}

void test_fix8_multi_if() {
    const std::string input =
        "Sapphire:\n"
        "74000130 00BF\n"
        "830005B0 0000\n"
        "74000130 00BF\n"
        "830005B8 0200\n"
        "74000130 00BF\n"
        "830005C0 EA14\n"
        "74000130 00BF\n"
        "830005C2 0814\n"
        "730005C0 EA3F\n"
        "830005C0 EA4A\n";

    const auto doc = gba::codebreaker::parse(input, {false});
    const auto ez = gba::ezflash::export_document(doc);

    const std::string expected =
        "[Sapphire]\n"
        "IF=80130,BF,00;ON=405B0,00,00;405B8,00,02;"
        "405C0,14,EA,14,08;IF=405C0,3F,EA;ON=405C0,4A,EA;";

    require(ez.text.find(expected) != std::string::npos,
            "Fix 8 multi-IF groups were not combined into one entry");
    require(ez.text.find("Part 1") == std::string::npos,
            "Fix 8 output unexpectedly split a valid multi-IF entry");
}

void test_optional_split_mode() {
    const std::string input =
        "Split Test:\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n"
        "72024C12 1122\n"
        "32024C12 0033\n";

    const auto doc = gba::codebreaker::parse(input, {false});
    gba::ezflash::Options options;
    options.combine_multiple_if_groups = false;
    const auto ez = gba::ezflash::export_document(doc, options);

    require(ez.text.find("[Split Test - Part 1]") != std::string::npos &&
            ez.text.find("[Split Test - Part 2]") != std::string::npos,
            "Optional legacy split mode did not split condition groups");
}

void test_ezflash_fix8_compound_condition_roundtrip() {
    const std::string input =
        "[Compound IF]\n"
        "IF=24C10,B7,9E;405C0,3F,EA;ON=25BC4,3F,42;\n";

    const auto document = gba::ezflash::parse(input);
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 2U,
            "Fix 8 compound IF did not parse atomically");
    require(document.entries[0].operations[0].condition_terms.size() == 2U,
            "Fix 8 discontiguous condition runs were not preserved");

    const auto output = gba::ezflash::export_document(document);
    require(output.text.find(
        "IF=24C10,B7,9E;405C0,3F,EA;ON=25BC4,3F,42;") !=
            std::string::npos,
            "Fix 8 compound IF did not roundtrip");
}

void test_ezflash_fix8_transactional_parse() {
    const auto direct = gba::ezflash::parse(
        "[Broken ON]\n"
        "ON=25BC4,3F,42;NOTHEX,00;\n");
    require(direct.entries.size() == 1U &&
            direct.entries[0].operations.empty(),
            "Malformed Fix 8 ON= line leaked valid records");

    const auto conditional = gba::ezflash::parse(
        "[Broken IF]\n"
        "IF=24C10,B7,9E;ON=25BC4,3F,42;"
        "IF=405C0,3F,EA;ON=NOTHEX,00;\n");
    require(conditional.entries.size() == 1U &&
            conditional.entries[0].operations.empty(),
            "Malformed later Fix 8 IF group leaked earlier groups");
}

void test_ezflash_fix8_multiline_continuation() {
    const std::string input =
        "[Multi-line]\n"
        "IF=24C10,B7,9E;ON=25BC4,3F,42;\n"
        "25BC6,0F,00;\n";

    const auto document = gba::ezflash::parse(input);
    const auto output = gba::ezflash::export_document(document);
    require(output.text.find(
        "IF=24C10,B7,9E;ON=25BC4,3F,42,0F,00;") !=
            std::string::npos,
            "Fix 8 continuation rows were not joined");
}

void test_ezflash_fix8_comment_and_terminator_rules() {
    const auto inline_comment = gba::ezflash::parse(
        "[Inline Comment]\n"
        "ON=0,11;# accepted on the first key row\n");
    require(inline_comment.entries.size() == 1U &&
            inline_comment.entries[0].operations.size() == 1U,
            "Fix 8 first-line inline # comment was not accepted");

    const auto continuation_comment = gba::ezflash::parse(
        "[Continuation Comment]\n"
        "ON=0,11;\n"
        "1,22;# invalid in a continuation row\n");
    require(continuation_comment.entries.size() == 1U &&
            continuation_comment.entries[0].operations.empty(),
            "Fix 8 continuation inline # did not invalidate the whole key");

    const auto terminated = gba::ezflash::parse(
        "[Before]\n"
        "ON=0,11;\n"
        "--\n"
        "[After]\n"
        "ON=1,22;\n");
    require(terminated.entries.size() == 1U &&
            terminated.entries[0].operations.size() == 1U,
            "Fix 8 -- terminator did not stop parsing");
}

void test_ezflash_fix8_compact_range_boundaries() {
    const auto crossing = gba::ezflash::parse(
        "[Boundary]\n"
        "ON=3FFFF,AA,BB;\n");
    require(crossing.entries.size() == 1U &&
            crossing.entries[0].operations.size() == 2U &&
            crossing.entries[0].operations[0].address == 0x0203FFFFU &&
            crossing.entries[0].operations[1].address == 0x03000000U,
            "Fix 8 compact EWRAM-to-IWRAM boundary was not expanded per byte");

    const auto crossing_output =
        gba::ezflash::export_document(crossing);
    require(crossing_output.text.find(
                "ON=3FFFF,AA;40000,BB;") != std::string::npos,
            "Fix 8 compact boundary did not re-export safely");

    const auto invalid = gba::ezflash::parse(
        "[Invalid Boundary]\n"
        "ON=47FFF,AA,BB;\n");
    require(invalid.entries.size() == 1U &&
            invalid.entries[0].operations.empty(),
            "Out-of-range trailing compact byte leaked from an ON= line");
}

void test_ezflash_fix8_runtime_limit() {
    std::string input = "[Too Large]\nON=0";
    for (std::size_t index = 0U; index < 129U; ++index) {
        input += "," + gba::text::hex(
            static_cast<std::uint32_t>(index), 2);
    }
    input += ";\n";

    const auto document = gba::ezflash::parse(input);
    const auto output = gba::ezflash::export_document(document);
    require(output.text.empty(),
            "A 129-record Fix 8 entry was incorrectly exported");
    require(std::any_of(
                output.warnings.begin(), output.warnings.end(),
                [](const std::string& warning) {
                    return warning.find("requires 129") !=
                        std::string::npos;
                }),
            "The Fix 8 128-record rejection was not reported");
}

void test_ezflash_fix8_physical_line_wrapping() {
    std::string input = "[Wrap]\nON=0";
    for (std::size_t index = 0U; index < 128U; ++index) {
        input += "," + gba::text::hex(
            static_cast<std::uint32_t>(index), 2);
    }
    input += ";\n";

    const auto document = gba::ezflash::parse(input);
    const auto output = gba::ezflash::export_document(document);
    const auto lines = gba::text::split_lines(output.text);

    bool saw_continuation = false;
    for (const std::string& line : lines) {
        if (line.empty() || line.front() == '[') {
            continue;
        }
        require(line.size() <= 298U,
                "EZ output exceeded the Fix 8 physical-line limit");
        if (line.rfind("ON=", 0U) != 0U &&
            line.rfind("IF=", 0U) != 0U) {
            saw_continuation = true;
            require(line.find('=') == std::string::npos,
                    "Fix 8 continuation row contained a new key");
        }
    }
    require(saw_continuation,
            "Large Fix 8 ON= output was not wrapped");

    const auto reparsed = gba::ezflash::parse(output.text);
    require(reparsed.entries.size() == 1U &&
            !reparsed.entries[0].operations.empty(),
            "Wrapped Fix 8 output did not parse again");
}

void test_ezflash_fix8_section_names() {
    gba::CheatDocument document;
    gba::CheatEntry first;
    first.name =
        "This Is An Extremely Long EZ Flash Section Name That Must Be Trimmed";
    gba::Operation section_write;
    section_write.kind = gba::OperationKind::Write;
    section_write.address = 0x02000000U;
    section_write.value = 0x11U;
    section_write.width = 1U;
    first.operations.push_back(section_write);

    gba::CheatEntry second = first;
    second.name =
        "ThisIsAnExtremelyLongEZFlashSectionNameThatMustBeTrimmed";
    document.entries.push_back(first);
    document.entries.push_back(second);

    const auto output = gba::ezflash::export_document(document);
    std::vector<std::string> names;
    for (const std::string& line : gba::text::split_lines(output.text)) {
        if (line.size() >= 2U &&
            line.front() == '[' && line.back() == ']') {
            const std::string name =
                line.substr(1U, line.size() - 2U);
            require(name.size() <= 49U,
                    "EZ section name exceeded the Fix 8 limit");
            names.push_back(name);
        }
    }
    require(names.size() == 2U && names[0] != names[1],
            "Kernel-colliding EZ section names were not made unique");
}

void test_ezflash_fix8_shared_table_warning() {
    gba::CheatDocument document;
    for (int entry_index = 0; entry_index < 2; ++entry_index) {
        gba::CheatEntry entry;
        entry.name = "Shared " + std::to_string(entry_index + 1);
        for (std::uint32_t index = 0U; index < 70U; ++index) {
            gba::Operation operation;
            operation.kind = gba::OperationKind::Write;
            operation.address =
                0x02000000U +
                static_cast<std::uint32_t>(entry_index) * 0x100U +
                index;
            operation.value = index;
            operation.width = 1U;
            entry.operations.push_back(operation);
        }
        document.entries.push_back(entry);
    }

    const auto output = gba::ezflash::export_document(document);
    require(std::any_of(
                output.warnings.begin(), output.warnings.end(),
                [](const std::string& warning) {
                    return warning.find("one shared 128-record table") !=
                        std::string::npos;
                }),
            "Fix 8 shared-table activation warning was not emitted");
}

} // namespace gba::tests
