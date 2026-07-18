#include "test_support.hpp"

namespace gba::tests {

void test_encrypted_stream_to_ez() {
    const std::string raw =
        "Encrypted EZ:\n72024C10 9EB7\n32024C10 00C2\n";
    const gba::codebreaker::Seed seed{0x9ABCDEF0U, 0x1234U};
    const auto document = gba::codebreaker::parse(
        gba::codebreaker::format_raw(raw, false, seed), {true});
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find(
                "=IF:W16,24C10,9EB7;W8:24C10,C2;ENDIF;") !=
                std::string::npos,
            "Encrypted CodeBreaker input did not convert to Enhanced v4");
}

void test_gameshark_condition_to_ez() {
    const auto document = gba::gameshark::parse(
        "GSA Conditional:\nD2024C10 00009EB7\n02024C10 000000C2\n",
        {false});
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find(
                "=IF:W16,24C10,9EB7;W8:24C10,C2;ENDIF;") !=
                std::string::npos,
            "GameShark condition did not convert to width-aware EZ syntax");
}

void test_armax_write_to_ez() {
    const auto document = gba::armax::parse(
        "AR MAX Money:\n04225BC4 000F423F\n", {false});
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find(
                "=W32:25BC4,000F423F;") != std::string::npos,
            "AR MAX 32-bit write did not become one W32 record");
}

void test_armax_next_two_condition_to_ez() {
    const auto document = gba::armax::parse(
        "AR MAX Conditional:\n"
        "4A224C10 00009EB7\n"
        "00224C10 000000C2\n"
        "00224C11 000000D3\n", {false});
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find(
                "=IF:W16,24C10,9EB7;W8:24C10,C2;W8:24C11,D3;ENDIF;") !=
                std::string::npos,
            "AR MAX next-two condition did not convert to one condition block");
}

void test_xploder_raw_to_ez() {
    const auto document = gba::xploder::parse(
        "Xploder Conditional:\n72024C10 9EB7\n32024C10 00C2\n",
        {false});
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find(
                "=IF:W16,24C10,9EB7;W8:24C10,C2;ENDIF;") !=
                std::string::npos,
            "Xploder condition did not convert to Enhanced v4");
}

void test_ezflash_original_on_only() {
    const auto document = gba::codebreaker::parse(
        "Original Mode:\n82025BC4 423F\n72024C10 9EB7\n32024C10 00C2\n",
        {false});
    gba::ezflash::Options options;
    options.mode = gba::ezflash::Mode::Original;
    const auto output = gba::ezflash::export_document(document, options);
    require(output.text.find("ON=25BC4,3F,42;") != std::string::npos &&
            output.text.find("24C10,C2") == std::string::npos &&
            !output.success,
            "Stock Original mode did not remain byte-list only");
}

void test_ezflash_parse_original_to_codebreaker() {
    const auto document = gba::ezflash::parse(
        "[Infinite Money]\nON=25BC4,3F,42,0F,00;\n");
    const auto output = gba::codebreaker::export_document(document, {});
    require(output.success && output.text.find("82025BC4 423F") !=
                std::string::npos &&
            output.text.find("82025BC6 000F") != std::string::npos,
            "Stock Original byte-list input did not convert to CodeBreaker");
}

void test_ezflash_parse_cheat_mod_roundtrip() {
    const std::string input =
        "[Sapphire]\n"
        "Balloon=IF:W16,80130,00BF;W16:405B0,0000;W16:405B8,0200;"
        "ENDIF;IF:W16,405C0,EA3F;W16:405C0,EA4A;ENDIF;\n";
    const auto document = gba::ezflash::parse(input);
    const auto output = gba::ezflash::export_document(document);
    require(document.warnings.empty() && output.success &&
            output.text.find("Balloon=IF:W16,80130,00BF;") !=
                std::string::npos,
            "Enhanced grouped input did not round-trip");
}

void test_ezflash_grouped_condition_to_fcd_repeats_condition() {
    const auto document = gba::ezflash::parse(
        "[Wide Conditional]\n"
        "ON=IF:W16,202,0001;W16:43AFA,007D;W16:43AFC,000B;ENDIF;\n");
    const auto output = gba::codebreaker::export_document(document, {});
    require(output.success && output.text.find(
                "72000202 0001\n53003AFA 0002\n7D000B00 0000") != std::string::npos,
            "Grouped condition did not preserve both FCD writes");
}

void test_ezflash_compound_condition_does_not_leak() {
    const auto document = gba::ezflash::parse(
        "[Compound]\n"
        "ON=IF:W16,202,0001;IF:W16,204,0002;W16:300,1234;"
        "ENDIF;ENDIF;\n");
    const auto armax = gba::armax::export_document(document, {false});
    require(armax.success &&
            armax.text.find("4A200202 00000001") != std::string::npos &&
            armax.text.find("0A200204 00000002") != std::string::npos &&
            armax.text.find("02200300 00001234") != std::string::npos,
            "Nested conditions did not remain dependent in AR MAX output");
}

} // namespace gba::tests
