#include "test_support.hpp"

namespace gba::tests {

void test_utf8_bom_cleanup() {
    const std::string bom("\xEF\xBB\xBF", 3U);
    const std::string input =
        bom +
        "[BOM Test]\r\n"
        "82001000 1234\r\n";

    const std::string cleaned =
        gba::text::cleanup_gamehacking_org_blocks(input);
    require(cleaned.rfind("[BOM Test]", 0U) == 0U,
            "UTF-8 BOM was not removed before parsing/cleanup");

    const auto document = gba::codebreaker::parse(cleaned, {false});
    require(document.entries.size() == 1U &&
            !document.entries.front().operations.empty(),
            "UTF-8 BOM input did not parse after cleanup");
}

void test_plain_cheat_name_preservation() {
    const std::string raw =
        "Infinite Health\n"
        "82001000 1234\n";

    const std::string normalized =
        gba::text::normalize_plain_cheat_headers(raw);
    require(normalized.find("[Infinite Health]") != std::string::npos,
            "Plain cheat name was not promoted to a parser header");

    const auto parsed = gba::codebreaker::parse(raw, {false});
    require(parsed.entries.size() == 1U &&
            parsed.entries.front().name == "Infinite Health" &&
            !parsed.entries.front().operations.empty(),
            "Plain CodeBreaker cheat name was not kept with its code");

    gba::output_modes::Options options;
    options.ezflash_mode = gba::output_modes::EzFlashMode::Original;
    const auto saved = gba::output_modes::export_document(
        parsed, gba::output_modes::Format::EzFlashCht, options);
    const std::string saved_text(saved.data.begin(), saved.data.end());
    require(saved.success &&
            saved_text.find("[Infinite Health]") != std::string::npos,
            "EZ-Flash Save Output As lost the plain cheat name");

    const auto ez = gba::ezflash::parse(
        "[Infinite Health]\n"
        "ON=1000,34,12;\n");
    require(ez.entries.size() == 1U &&
            ez.entries.front().name == "Infinite Health",
            "EZ-Flash parser removed spaces from the displayed cheat name");
}

void test_inline_notes_inside_entry() {
    const std::string input =
        "Mixed Code:\n"
        "02025BC4 000000C2\n"
        "62000010 00001234\n";

    const auto document = gba::gameshark::parse(input, {false});
    const auto converted =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    const std::string annotated = gba::inline_notes::apply(
        converted.text,
        document,
        converted.warnings,
        {gba::inline_notes::Style::Slash, true});

    const std::size_t header = annotated.find("[Mixed Code]");
    const std::size_t note = annotated.find("// Conversion Note:");
    const std::size_t code = annotated.find("32025BC4 00C2");

    require(header != std::string::npos &&
            note != std::string::npos &&
            code != std::string::npos &&
            header < note && note < code,
            "Inline note was not placed directly inside the affected entry");
    require(annotated.find("// Source: 62000010 00001234") !=
                std::string::npos,
            "Inline note did not preserve the skipped source row");
    require(annotated.find(
                "// Conversion Summary: 1 inline note(s)") !=
                std::string::npos,
            "Duplicate decoder/exporter notes were not consolidated");
}

void test_inline_notes_for_empty_entry() {
    const std::string input =
        "Only Patch:\n"
        "62000010 00001234\n";

    const auto document = gba::gameshark::parse(input, {false});
    const auto converted =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    const std::string annotated = gba::inline_notes::apply(
        converted.text,
        document,
        converted.warnings,
        {gba::inline_notes::Style::Slash, true});

    require(annotated.find("[Only Patch]") != std::string::npos &&
            annotated.find("// Conversion Note:") != std::string::npos,
            "Skipped entry disappeared instead of receiving an inline note");
}

void test_inline_notes_ez_comment_style() {
    const std::string input =
        "Original Conditional:\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n";

    const auto document = gba::codebreaker::parse(input, {false});
    gba::ezflash::Options options;
    options.mode = gba::ezflash::Mode::Original;
    const auto converted =
        gba::ezflash::export_document(document, options);
    const std::string annotated = gba::inline_notes::apply(
        converted.text,
        document,
        converted.warnings,
        {gba::inline_notes::Style::Hash, true});

    require(annotated.find("# Conversion Note:") != std::string::npos,
            "EZ-Flash inline notes did not use hash comments");
    const auto parsed_again = gba::ezflash::parse(annotated);
    require(parsed_again.warnings.empty(),
            "EZ-Flash parser did not ignore its inline conversion comments");
}

void test_newline_cleanup() {
    const std::string lf =
        "Code A\n"
        "12345678 1234\n"
        "Code B\n"
        "87654321 4321";

    const std::string cr =
        "Code A\r"
        "12345678 1234\r"
        "Code B\r"
        "87654321 4321";

    const std::string crlf =
        "Code A\r\n"
        "12345678 1234\r\n"
        "Code B\r\n"
        "87654321 4321";

    const std::string mixed =
        "Code A\r\n"
        "12345678 1234\n"
        "Code B\r"
        "87654321 4321";

    require(gba::text::normalize_newlines_lf(lf) == lf,
            "LF normalization changed valid Unix newlines");
    require(gba::text::normalize_newlines_lf(cr) == lf,
            "Classic Mac CR was not normalized to LF");
    require(gba::text::normalize_newlines_lf(crlf) == lf,
            "Windows CRLF was not normalized to LF");
    require(gba::text::normalize_newlines_lf(mixed) == lf,
            "Mixed newlines were not normalized to LF");

    require(gba::text::normalize_newlines_crlf(lf) == crlf,
            "Unix LF was not normalized to Windows CRLF");
    require(gba::text::normalize_newlines_crlf(cr) == crlf,
            "Classic Mac CR was not normalized to Windows CRLF");
    require(gba::text::normalize_newlines_crlf(crlf) == crlf,
            "Windows CRLF normalization was not idempotent");
    require(gba::text::normalize_newlines_crlf(mixed) == crlf,
            "Mixed newlines were not normalized to Windows CRLF");

    const auto lf_lines = gba::text::split_lines(lf);
    require(gba::text::split_lines(cr) == lf_lines,
            "CR input did not split into the same lines as LF");
    require(gba::text::split_lines(crlf) == lf_lines,
            "CRLF input did not split into the same lines as LF");
    require(gba::text::split_lines(mixed) == lf_lines,
            "Mixed-newline input did not split consistently");
}

void test_compact_code_line_auto_format() {
    const std::string compact =
        "98816CA98172\n"
        "0515877376E0\r"
        "2C234EDC92B1\r\n"
        "\r\n"
        "156739CB40DA6751\n"
        "65DA389A09D8882C\n"
        "[Code Name]\n"
        "# Comment 123456789ABC\n"
        "ON=25BC4,3F,42,0F,00;";

    const std::string expected =
        "98816CA9 8172\n"
        "05158773 76E0\n"
        "2C234EDC 92B1\n"
        "\n"
        "156739CB 40DA6751\n"
        "65DA389A 09D8882C\n"
        "[Code Name]\n"
        "# Comment 123456789ABC\n"
        "ON=25BC4,3F,42,0F,00;";

    require(gba::text::format_compact_code_lines(compact) == expected,
            "Compact 8+4/8+8 code rows were not auto-formatted");

    const std::string spaced =
        "05158773    76E0\n"
        "156739CB\t40DA6751\n"
        "00000000:0000\n"
        "12345678 : 9ABCDEF0\n";
    const std::string canonical =
        "05158773 76E0\n"
        "156739CB 40DA6751\n"
        "00000000 0000\n"
        "12345678 9ABCDEF0\n";

    require(gba::text::format_compact_code_lines(spaced) == canonical,
            "Whitespace/colon-separated code rows were not canonicalized");

    require(gba::text::format_compact_code_lines(expected) == expected,
            "Compact code formatting was not idempotent");
}

void test_flattened_code_stream_cleanup() {
    const std::string armax_flat =
        "0A400130 000000FF 002239F8 00000004 "
        "00000000 1801D418 00002000 00000000";
    const std::string armax_expected =
        "0A400130 000000FF\n"
        "002239F8 00000004\n"
        "00000000 1801D418\n"
        "00002000 00000000";
    require(gba::text::format_compact_code_lines(armax_flat) ==
                armax_expected,
            "Flattened 8+8 clipboard stream was not restored to rows");

    const std::string fcd_flat =
        "74000130 01BF 82000202 0001 72000202 0001";
    const std::string fcd_expected =
        "74000130 01BF\n"
        "82000202 0001\n"
        "72000202 0001";
    require(gba::text::format_compact_code_lines(fcd_flat) ==
                fcd_expected,
            "Flattened 8+4 clipboard stream was not restored to rows");

    const std::string compact_flat =
        "0A400130000000FF 002239F800000004 "
        "000000001801D418 0000200000000000";
    require(gba::text::format_compact_code_lines(compact_flat) ==
                armax_expected,
            "Flattened compact 8+8 stream was not restored to rows");

    const std::string ordinary_text =
        "Revision 0A400130 000000FF notes";
    require(gba::text::format_compact_code_lines(ordinary_text) ==
                ordinary_text,
            "Ordinary text containing code-like values was split");

    require(gba::text::format_compact_code_lines(armax_expected) ==
                armax_expected,
            "Flattened stream cleanup was not idempotent");
}

void test_codetwink_attached_code_cleanup() {
    const std::string input =
        "Super Mario World Codes\n"
        "1Press Up+L For Balloon Mode On74000130 01BF\n"
        "82000202 0001\n"
        "72000202 0001\n"
        "8300369E 00C0\n"
        "72000202 0001\n"
        "83003AFA 007D\n"
        "72000202 0001\n"
        "83003AFC 000B";

    const std::string expected =
        "Super Mario World Codes\n"
        "1Press Up+L For Balloon Mode On\n"
        "74000130 01BF\n"
        "82000202 0001\n"
        "72000202 0001\n"
        "8300369E 00C0\n"
        "72000202 0001\n"
        "83003AFA 007D\n"
        "72000202 0001\n"
        "83003AFC 000B";

    const std::string cleaned =
        gba::text::cleanup_gamehacking_org_blocks(input);
    require(cleaned == expected,
            "CodeTwink attached first code row was not split from the name");
    require(gba::text::cleanup_gamehacking_org_blocks(cleaned) == expected,
            "CodeTwink attached-code cleanup was not idempotent");

    const std::string compact_attached =
        "Infinite Health820010001234\n"
        "Wide Write123456789ABCDEF0";
    const std::string compact_expected =
        "Infinite Health\n"
        "82001000 1234\n"
        "Wide Write\n"
        "12345678 9ABCDEF0";
    require(gba::text::format_compact_code_lines(compact_attached) ==
                compact_expected,
            "Attached compact 8+4/8+8 code rows were not split");

    const std::string spaced_title =
        "Version 12345678 9ABC\n"
        "Game 1234 Codes\n"
        "# Comment123456789ABC";
    require(gba::text::format_compact_code_lines(spaced_title) ==
                spaced_title,
            "Normally spaced code-looking title text was split incorrectly");
}

void test_gamehacking_org_cleanup() {
    const std::string input =
        "[M] Must Be On by MadCatz\r\n"
        "Codebreaker/GameShark SP/Xploder\r\n"
        "98816CA98172\r\n"
        "0515877376E0\r\n"
        "2C234EDC92B1";

    const std::string expected =
        "[M] Must Be On , by MadCatz , "
        "Crypt_Codebreaker/GameShark SP/Xploder\n"
        "98816CA9 8172\n"
        "05158773 76E0\n"
        "2C234EDC 92B1";

    const std::string cleaned =
        gba::text::cleanup_gamehacking_org_blocks(input);
    require(cleaned == expected,
            "GameHacking.org name/credit/crypt block cleanup failed");
    require(gba::text::cleanup_gamehacking_org_blocks(cleaned) == expected,
            "GameHacking.org cleanup was not idempotent");
}

void test_gamehacking_org_multiple_blocks() {
    const std::string input =
        "Infinite Health by Alice\n"
        "GameShark Advance / Action Replay GBX\n"
        "156739CB40DA6751\n"
        "\n"
        "Max Money\n"
        "Action Replay MAX\n"
        "65DA389A09D8882C\n";

    const std::string expected =
        "Infinite Health , by Alice , "
        "Crypt_GameShark Advance/Action Replay GBX\n"
        "156739CB 40DA6751\n"
        "\n"
        "Max Money , Crypt_Action Replay MAX\n"
        "65DA389A 09D8882C\n";

    require(gba::text::cleanup_gamehacking_org_blocks(input) == expected,
            "Multiple GameHacking.org blocks were not cleaned correctly");
}

void test_gamehacking_org_gameshark_advance_alias() {
    const std::string input =
        "(M) by Codejunkies\n"
        "GameShark Advance/Action Replay\n"
        "2286524F 8E51FD40\n"
        "3112D648 B66140BA";

    const std::string expected =
        "(M) , by Codejunkies , "
        "Crypt_GameShark Advance/Action Replay\n"
        "2286524F 8E51FD40\n"
        "3112D648 B66140BA";

    const std::string cleaned =
        gba::text::cleanup_gamehacking_org_blocks(input);
    require(cleaned == expected,
            "GameHacking.org GameShark Advance/Action Replay alias was not cleaned");
    require(gba::text::cleanup_gamehacking_org_blocks(cleaned) == expected,
            "GameShark Advance/Action Replay alias cleanup was not idempotent");
}

void test_inline_metadata_heading_roundtrip() {
    const std::string cleaned =
        "[M] Must Be On , by MadCatz , "
        "Crypt_Codebreaker/GameShark SP/Xploder\n"
        "98816CA9 8172\n"
        "05158773 76E0\n"
        "2C234EDC 92B1";

    const auto document = gba::codebreaker::parse(cleaned, {false});
    require(document.entries.size() == 1U,
            "Inline metadata heading was not parsed as a cheat entry");
    require(document.entries[0].name ==
                "[M] Must Be On , by MadCatz , "
                "Crypt_Codebreaker/GameShark SP/Xploder",
            "Inline metadata heading text was not preserved");

    const auto exported =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    require(exported.text.find(
                "[M] Must Be On , by MadCatz , "
                "Crypt_Codebreaker/GameShark SP/Xploder\n") == 0U,
            "Raw output added extra brackets around inline metadata");
    require(exported.text.find(
                "[[M] Must Be On") == std::string::npos,
            "Raw output produced a double-bracket metadata heading");
}

void test_gamehacking_org_legacy_separator_removal() {
    const std::string old_cleaned =
        "[M] Must Be On , by MadCatz , "
        "Crypt_Codebreaker/GameShark SP/Xploder\n"
        "98816CA9 8172\n"
        "05158773 76E0\n"
        "2C234EDC 92B1\n"
        "______________________________\n"
        "\n"
        "Max Money , Crypt_Action Replay MAX\n"
        "156739CB 40DA6751\n"
        "__________";

    const std::string expected =
        "[M] Must Be On , by MadCatz , "
        "Crypt_Codebreaker/GameShark SP/Xploder\n"
        "98816CA9 8172\n"
        "05158773 76E0\n"
        "2C234EDC 92B1\n"
        "\n"
        "Max Money , Crypt_Action Replay MAX\n"
        "156739CB 40DA6751";

    require(
        gba::text::cleanup_gamehacking_org_blocks(old_cleaned) == expected,
        "Legacy GameHacking.org underscore separators were not removed");
}

} // namespace gba::tests
