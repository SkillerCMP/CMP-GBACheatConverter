#include "test_support.hpp"

namespace gba::tests {

void test_auto_detect_ezflash() {
    const auto result = gba::detect::format(
        "[EZ Test]\nON=IF:W16,24C10,9EB7;W8:24C10,C2;ENDIF;\n");
    require(result.format == gba::detect::Format::EzFlash,
            "Auto Detect did not recognize EZ-Flash syntax");
    require(result.confidence == gba::detect::Confidence::High,
            "EZ-Flash detection was not high confidence");
}

void test_auto_detect_fcd_raw_and_encrypted() {
    const std::string raw =
        "FCD Raw:\n"
        "82025BC4 423F\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n";
    const auto raw_result = gba::detect::format(raw);
    require(raw_result.format == gba::detect::Format::FcdRaw,
            "Auto Detect did not recognize raw FCD 8+4 input");

    const gba::codebreaker::Seed seed{0x9ABCDEF0U, 0x1234U};
    const std::string encrypted =
        gba::codebreaker::format_raw(raw, false, seed);
    const auto encrypted_result = gba::detect::format(encrypted);
    require(encrypted_result.format == gba::detect::Format::FcdEncrypted,
            "Auto Detect did not recognize encrypted FCD input");
}

void test_auto_detect_gameshark_raw_and_encrypted() {
    const std::string raw =
        "GSA:\n"
        "12025BC4 0000423F\n"
        "D2024C10 00009EB7\n"
        "02024C10 000000C2\n";
    const auto raw_result = gba::detect::format(raw);
    require(raw_result.format == gba::detect::Format::GameSharkRaw,
            "Auto Detect did not recognize raw GameShark/AR GBX input");

    const auto document = gba::gameshark::parse(raw, {false});
    const auto encrypted = gba::gameshark::export_document(document, {true});
    const auto encrypted_result = gba::detect::format(encrypted.text);
    require(encrypted_result.format ==
                gba::detect::Format::GameSharkEncrypted,
            "Auto Detect did not recognize encrypted GameShark/AR GBX input");
}

void test_auto_detect_armax_raw_and_encrypted() {
    const std::string encrypted =
        "B6C5368A 08BEAFF4\n"
        "6FD608D0 B9151D51\n"
        "084197CA 3EA6DE4F\n"
        "8E883EFF 92E9660D\n";
    const std::string raw =
        "78914CD3 8AB26DFD\n"
        "002239F8 00000004\n"
        "00000000 1801D418\n"
        "00002000 00000000\n";

    const auto raw_result = gba::detect::format(raw);
    require(raw_result.format == gba::detect::Format::ActionReplayMaxRaw,
            "Auto Detect did not recognize raw AR MAX input");

    const auto encrypted_result = gba::detect::format(encrypted);
    require(encrypted_result.format ==
                gba::detect::Format::ActionReplayMaxEncrypted,
            "Auto Detect did not recognize encrypted AR MAX input");
}

void test_auto_detect_rejects_non_code_text() {
    const auto result = gba::detect::format("This is not a cheat code.\n");
    require(result.format == gba::detect::Format::Unknown,
            "Auto Detect guessed a format for non-code text");
}

void test_auto_detect_gameshark_device_button() {
    const std::string raw =
        "[Button Detect]\n"
        "82112345 0000007F\n"
        "83223456 00001234\n";

    const auto raw_detect = gba::detect::format(raw);
    require(raw_detect.format == gba::detect::Format::GameSharkRaw,
            "Auto Detect did not recognize raw GameShark button writes");

    const auto document = gba::gameshark::parse(raw, {false});
    const auto encrypted =
        gba::gameshark::export_document(document, {true});
    const auto encrypted_detect =
        gba::detect::format(encrypted.text);
    require(encrypted_detect.format ==
                gba::detect::Format::GameSharkEncrypted,
            "Auto Detect did not recognize encrypted GameShark button writes");
}

void test_auto_detect_gameshark_rom_patch_and_slowdown() {
    const std::string raw =
        "[Special Detect]\n"
        "60012345 1000ABCD\n"
        "80F00000 00001234\n";
    const auto raw_detect = gba::detect::format(raw);
    require(raw_detect.format == gba::detect::Format::GameSharkRaw,
            "Auto Detect did not recognize GameShark ROM patch/slowdown rows");

    const auto document = gba::gameshark::parse(raw, {false});
    const auto encrypted = gba::gameshark::export_document(document, {true});
    const auto encrypted_detect = gba::detect::format(encrypted.text);
    require(encrypted_detect.format ==
                gba::detect::Format::GameSharkEncrypted,
            "Auto Detect did not recognize encrypted GameShark specials");
}

void test_auto_detect_gameshark_assignment_list() {
    const std::string raw =
        "[Assignment Detect]\n"
        "30000004 02004000\n"
        "02001000 02002000\n"
        "02003000 00000000\n";

    const auto raw_detect = gba::detect::format(raw);
    require(raw_detect.format == gba::detect::Format::GameSharkRaw,
            "Auto Detect did not recognize a raw GameShark assignment list");

    const auto document = gba::gameshark::parse(raw, {false});
    const auto encrypted =
        gba::gameshark::export_document(document, {true});
    const auto encrypted_detect =
        gba::detect::format(encrypted.text);
    require(encrypted_detect.format ==
                gba::detect::Format::GameSharkEncrypted,
            "Auto Detect did not recognize an encrypted GameShark assignment list");
}

void test_auto_detect_armax_dynamic_and_pointer() {
    const std::string raw =
        "[Detect]\n"
        "DEADFACE 00001234\n"
        "40200010 0001237F\n"
        "04200010 AABBCCDD\n";
    const auto raw_detect = gba::detect::format(raw);
    require(raw_detect.format == gba::detect::Format::ActionReplayMaxRaw,
            "Auto Detect missed raw AR MAX pointer/DEADFACE input");

    const auto encrypted = gba::armax::transform_text(raw, false, true);
    const auto encrypted_detect = gba::detect::format(encrypted.text);
    require(encrypted_detect.format ==
                gba::detect::Format::ActionReplayMaxEncrypted,
            "Auto Detect did not follow AR MAX dynamic encryption keys");
}

void test_auto_detect_armax_block_and_button() {
    const std::string raw =
        "[AR MAX Detect Blocks]\n"
        "8A200010 00001234\n"
        "04200100 11111111\n"
        "00000000 60000000\n"
        "04200104 22222222\n"
        "00000000 40000000\n"
        "00000000 10200108\n"
        "0000007F 00000000\n";

    const auto raw_detect = gba::detect::format(raw);
    require(raw_detect.format ==
                gba::detect::Format::ActionReplayMaxRaw,
            "Auto Detect missed raw AR MAX block/button input");

    const auto encrypted =
        gba::armax::transform_text(raw, false, true);
    const auto encrypted_detect = gba::detect::format(encrypted.text);
    require(encrypted_detect.format ==
                gba::detect::Format::ActionReplayMaxEncrypted,
            "Auto Detect missed encrypted AR MAX block/button input");
}


void test_auto_detect_single_armax_encrypted_mirror_write() {
    const auto result = gba::detect::format("3C8BBA54 A8648690\n");
    require(result.format ==
                gba::detect::Format::ActionReplayMaxEncrypted,
            "Auto Detect did not recognize the single encrypted AR MAX "
            "mirrored-RAM write");
}

void test_auto_detect_ezflash_masked_condition() {
    const auto result = gba::detect::format(
        "[Moon Jump]\n"
        "ON=IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;\n");
    require(result.format == gba::detect::Format::EzFlash,
            "Auto Detect did not recognize an IFM Enhanced option");
}

} // namespace gba::tests
