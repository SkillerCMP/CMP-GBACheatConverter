#include "test_support.hpp"

namespace gba::tests {

void test_encrypted_stream_to_ez() {
    const std::string raw =
        "Encrypted EZ:\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n";
    const gba::codebreaker::Seed seed{0x9ABCDEF0U, 0x1234U};
    const std::string encrypted =
        gba::codebreaker::format_raw(raw, false, seed);

    const auto document = gba::codebreaker::parse(encrypted, {true});
    const auto ez = gba::ezflash::export_document(document);

    require(ez.text.find("[Encrypted EZ]") != std::string::npos &&
            ez.text.find("IF=24C10,B7,9E;ON=24C10,C2;") != std::string::npos,
            "Encrypted CodeBreaker input did not convert to EZ");
    require(ez.warnings.empty(),
            "Encrypted stream seed created a false empty EZ entry warning");
}

void test_gameshark_condition_to_ez() {
    const std::string input =
        "GSA Conditional:\n"
        "D2024C10 00009EB7\n"
        "02024C10 000000C2\n";

    const auto document = gba::gameshark::parse(input, {false});
    const auto output = gba::ezflash::export_document(document);

    require(output.text.find(
        "IF=24C10,B7,9E;ON=24C10,C2;") != std::string::npos,
        "GameShark equal condition did not convert to EZ");
}

void test_armax_write_to_ez() {
    const std::string raw =
        "AR MAX Money:\n"
        "04225BC4 000F423F\n";

    const auto document = gba::armax::parse(raw, {false});
    const auto output = gba::ezflash::export_document(document);

    require(output.text.find(
        "ON=25BC4,3F,42,0F,00;") != std::string::npos,
        "Action Replay MAX 32-bit write did not convert to EZ");
}

void test_armax_next_two_condition_to_ez() {
    const std::string raw =
        "AR MAX Conditional:\n"
        "4A224C10 00009EB7\n"
        "00224C10 000000C2\n"
        "00224C11 000000D3\n";

    const auto document = gba::armax::parse(raw, {false});
    const auto output = gba::ezflash::export_document(document);

    require(output.text.find(
        "IF=24C10,B7,9E;ON=24C10,C2,D3;") != std::string::npos,
        "AR MAX next-two condition did not become one EZ IF group");
}

void test_xploder_raw_to_ez() {
    const std::string input =
        "Xploder Conditional:\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n";

    const auto document = gba::xploder::parse(input, {false});
    const auto output = gba::ezflash::export_document(document);

    require(output.text.find(
        "IF=24C10,B7,9E;ON=24C10,C2;") != std::string::npos,
        "Xploder raw condition did not convert to EZ Fix 8");
}

void test_ezflash_original_on_only() {
    const std::string input =
        "Original Mode:\n"
        "82025BC4 423F\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n";

    const auto document = gba::codebreaker::parse(input, {false});

    gba::ezflash::Options options;
    options.mode = gba::ezflash::Mode::Original;
    const auto output = gba::ezflash::export_document(document, options);

    require(output.text.find("ON=25BC4,3F,42;") != std::string::npos,
            "EZ-Flash Original did not preserve a direct ON= write");
    require(output.text.find("IF=") == std::string::npos,
            "EZ-Flash Original emitted an IF= condition");
    require(output.text.find("24C10,C2") == std::string::npos,
            "EZ-Flash Original leaked a conditioned write as unconditional");
    require(!output.success,
            "EZ-Flash Original did not report the rejected condition");
    require(!output.warnings.empty(),
            "EZ-Flash Original did not explain its ON=-only limitation");
}

void test_ezflash_parse_original_to_codebreaker() {
    const std::string input =
        "[Infinite Money]\n"
        "ON=25BC4,3F,42,0F,00;\n";

    const auto document = gba::ezflash::parse(input);
    const auto output = gba::codebreaker::export_document(document, {});

    require(output.text.find("82025BC4 423F") != std::string::npos &&
            output.text.find("82025BC6 000F") != std::string::npos,
            "EZ-Flash Original input did not convert to CodeBreaker writes");
}

void test_ezflash_parse_cheat_mod_roundtrip() {
    const std::string input =
        "[Sapphire]\n"
        "IF=80130,BF,00;ON=405B0,00,00;405B8,00,02;"
        "IF=405C0,3F,EA;ON=405C0,4A,EA;\n";

    const auto document = gba::ezflash::parse(input);
    gba::ezflash::Options options;
    options.mode = gba::ezflash::Mode::Enhanced;
    const auto output = gba::ezflash::export_document(document, options);

    require(output.text.find(
        "IF=80130,BF,00;ON=405B0,00,00;405B8,00,02;"
        "IF=405C0,3F,EA;ON=405C0,4A,EA;") != std::string::npos,
        "EZ-Flash Cheat MOD multi-IF input did not roundtrip");
}

void test_ezflash_grouped_condition_to_fcd_repeats_condition() {
    const std::string fcd_input =
        "Press Up+L For Balloon Mode On\n"
        "74000130 01BF\n"
        "82000202 0001\n"
        "72000202 0001\n"
        "8300369E 00C0\n"
        "72000202 0001\n"
        "83003AFA 007D\n"
        "72000202 0001\n"
        "83003AFC 000B\n";

    const auto original = gba::codebreaker::parse(fcd_input, {false});
    const auto ez = gba::ezflash::export_document(original);
    require(ez.success && ez.text.find(
                "IF=80130,BF,01;ON=202,01,00;"
                "IF=202,01,00;ON=4369E,C0,00;43AFA,7D,00,0B,00;") !=
                std::string::npos,
            "FCD conditions were not compacted into the expected EZ groups");

    const auto reparsed = gba::ezflash::parse(ez.text);
    const auto fcd = gba::codebreaker::export_document(reparsed, {});
    require(fcd.success && fcd.text.find(
                "74000130 01BF\n"
                "82000202 0001\n"
                "72000202 0001\n"
                "8300369E 00C0\n"
                "72000202 0001\n"
                "83003AFA 007D\n"
                "72000202 0001\n"
                "83003AFC 000B") != std::string::npos,
            "Grouped EZ condition did not repeat before every FCD write row");

    const auto wide_only = gba::ezflash::parse(
        "[Wide Conditional]\n"
        "IF=202,01,00;ON=43AFA,7D,00,0B,00;\n");
    const auto wide_fcd = gba::codebreaker::export_document(wide_only, {});
    require(wide_fcd.success && wide_fcd.text.find(
                "72000202 0001\n"
                "83003AFA 007D\n"
                "72000202 0001\n"
                "83003AFC 000B") != std::string::npos,
            "A 32-bit EZ write did not repeat its FCD condition for both halves");
}

void test_ezflash_compound_condition_does_not_leak() {
    const std::string input =
        "[Compound]\n"
        "IF=24C10,B7,9E;405C0,3F,EA;ON=25BC4,3F,42;\n";
    const auto document = gba::ezflash::parse(input);

    const auto fcd = gba::codebreaker::export_document(document, {});
    const auto gameshark = gba::gameshark::export_document(document, {});
    const auto armax = gba::armax::export_document(document, {});

    require(fcd.text.find("82025BC4") == std::string::npos,
            "Compound EZ condition leaked its write to CodeBreaker");
    require(gameshark.text.find("25BC4") == std::string::npos,
            "Compound EZ condition leaked its write to GameShark");
    require(armax.text.find("25BC4") == std::string::npos,
            "Compound EZ condition leaked its write to AR MAX");
}

} // namespace gba::tests
