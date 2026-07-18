#include "test_support.hpp"

namespace gba::tests {

void test_armax_encrypted_vector_decrypt() {
    const std::string encrypted =
        "B6C5368A 08BEAFF4\n"
        "6FD608D0 B9151D51\n"
        "084197CA 3EA6DE4F\n"
        "8E883EFF 92E9660D\n";

    const auto result =
        gba::armax::transform_text(encrypted, true, false);

    const std::string expected =
        "78914CD3 8AB26DFD\n"
        "002239F8 00000004\n"
        "00000000 1801D418\n"
        "00002000 00000000\n";

    require(result.text == expected,
            "Action Replay MAX encrypted vector did not decrypt exactly");
    require(result.success,
            "Action Replay MAX static-key decrypt unexpectedly failed");
}

void test_armax_static_roundtrip() {
    const std::string raw =
        "AR MAX Round Trip:\n"
        "04225BC4 000F423F\n"
        "4A224C10 00009EB7\n"
        "00224C10 000000C2\n"
        "00224C11 000000D3\n";

    const auto encrypted =
        gba::armax::transform_text(raw, false, true);
    const auto decrypted =
        gba::armax::transform_text(encrypted.text, true, false);

    require(encrypted.success && decrypted.success,
            "Action Replay MAX static roundtrip reported failure");
    require(decrypted.text == raw,
            "Action Replay MAX raw/encrypted roundtrip changed the text");
}

void test_codebreaker_to_armax_raw() {
    const std::string input =
        "Infinite Money:\n"
        "82025BC4 423F\n"
        "82025BC6 000F\n";

    const auto document = gba::codebreaker::parse(input, {false});
    const auto output = gba::armax::export_document(document, {false});

    require(output.text.find("02225BC4 0000423F") != std::string::npos &&
            output.text.find("02225BC6 0000000F") != std::string::npos,
            "CodeBreaker writes did not encode as Action Replay MAX raw");
    require(output.success,
            "Action Replay MAX raw export unexpectedly failed");
}

void test_armax_signed_comparison_does_not_leak_to_fcd() {
    const std::string input =
        "[Signed AR MAX]\n"
        "1A200010 00008000\n"
        "00200020 00000001\n";

    const auto document = gba::armax::parse(input, {false});
    const auto fcd = gba::codebreaker::export_document(document, {false, std::nullopt});
    require(!fcd.success &&
            fcd.text.find("32000020 0001") == std::string::npos,
            "Signed AR MAX comparison leaked its controlled write into FCD");
}

void test_armax_deadface_key_derivation() {
    const auto initial =
        gba::crypto::pro_action_replay_v3_key_from_deadface(0x0000U);
    require(initial == gba::crypto::ProActionReplayV3Key,
            "DEADFACE 0000 did not reproduce the initial AR MAX key");

    const auto changed =
        gba::crypto::pro_action_replay_v3_key_from_deadface(0x1234U);
    require(changed[0] == 0x6D09D152U &&
            changed[1] == 0x8D29F172U &&
            changed[2] == 0xC05C24A5U &&
            changed[3] == 0x9E3A0283U,
            "AR MAX DEADFACE key vector is incorrect");
}

void test_armax_dynamic_deadface_roundtrip() {
    const std::string raw =
        "[Dynamic Seeds]\n"
        "DEADFACE 00001234\n"
        "04200010 AABBCCDD\n"
        "DEADFACE 00005678\n"
        "02200020 00001234\n";
    const std::string expected_encrypted =
        "[Dynamic Seeds]\n"
        "9C7FE77C A5352DCA\n"
        "D9B2E263 140B0C5A\n"
        "40EB583B 1F570BE9\n"
        "6238630A 9B5AFDBB\n";

    const auto encrypted = gba::armax::transform_text(raw, false, true);
    require(encrypted.success && encrypted.text == expected_encrypted,
            "AR MAX dynamic DEADFACE encryption vector failed");
    const auto decrypted =
        gba::armax::transform_text(encrypted.text, true, false);
    require(decrypted.success && decrypted.text == raw,
            "AR MAX dynamic DEADFACE decrypt roundtrip failed");

    const auto document = gba::armax::parse(encrypted.text, {true});
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 4U &&
            document.entries[0].operations[0].kind ==
                gba::OperationKind::EncryptionSeed &&
            document.entries[0].operations[1].kind ==
                gba::OperationKind::Write &&
            document.entries[0].operations[1].address == 0x02000010U &&
            document.entries[0].operations[1].value == 0xAABBCCDDU &&
            document.entries[0].operations[3].address == 0x02000020U &&
            document.entries[0].operations[3].value == 0x1234U,
            "Semantic AR MAX parse did not follow dynamic DEADFACE keys");

    const auto reencrypted = gba::armax::export_document(document, {true});
    require(reencrypted.success &&
            reencrypted.text == expected_encrypted + "\n",
            "Semantic AR MAX export did not apply dynamic DEADFACE keys");
}

void test_armax_pointer_roundtrip() {
    const std::string raw =
        "[Pointer Writes]\n"
        "40200010 0001237F\n"
        "42300020 0123ABCD\n"
        "44300024 89ABCDEF\n";
    const auto document = gba::armax::parse(raw, {false});
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 3U,
            "AR MAX pointer rows were not parsed");

    const auto& p8 = document.entries[0].operations[0];
    const auto& p16 = document.entries[0].operations[1];
    const auto& p32 = document.entries[0].operations[2];
    require(p8.kind == gba::OperationKind::PointerWrite &&
            p8.address == 0x02000010U && p8.pointer_offset == 0x123U &&
            p8.width == 1U && p8.value == 0x7FU,
            "AR MAX 8-bit pointer decode failed");
    require(p16.kind == gba::OperationKind::PointerWrite &&
            p16.address == 0x03000020U && p16.pointer_offset == 0x246U &&
            p16.width == 2U && p16.value == 0xABCDU,
            "AR MAX 16-bit pointer decode failed");
    require(p32.kind == gba::OperationKind::PointerWrite &&
            p32.address == 0x03000024U && p32.pointer_offset == 0U &&
            p32.width == 4U && p32.value == 0x89ABCDEFU,
            "AR MAX 32-bit pointer decode failed");

    const auto raw_export = gba::armax::export_document(document, {false});
    require(raw_export.success && raw_export.text == raw + "\n",
            "AR MAX pointer raw roundtrip failed");
    const auto encrypted = gba::armax::export_document(document, {true});
    const auto decrypted =
        gba::armax::transform_text(encrypted.text, true, false);
    require(encrypted.success && decrypted.success &&
            decrypted.text == raw + "\n",
            "AR MAX pointer encrypted roundtrip failed");
}

void test_armax_pointer_cross_family_safety() {
    const std::string raw =
        "[Pointer Safety]\n"
        "40200010 0001237F\n"
        "02200040 00001234\n";
    const auto document = gba::armax::parse(raw, {false});

    const auto fcd =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    require(!fcd.success &&
            fcd.text.find("0001237F") == std::string::npos &&
            fcd.text.find("82000040 1234") != std::string::npos,
            "AR MAX pointer became a direct FCD write");

    const auto gs = gba::gameshark::export_document(document, {false});
    require(!gs.success &&
            gs.text.find("0001237F") == std::string::npos &&
            gs.text.find("12000040 00001234") != std::string::npos,
            "AR MAX pointer became a direct GameShark write");

    const auto ez = gba::ezflash::export_document(document);
    require(ez.success &&
            ez.text.find("PTR:W8,10,00000123,7F;") != std::string::npos &&
            ez.text.find("W16:40,1234;") != std::string::npos,
            "AR MAX pointer did not map to Enhanced v4 PTR");
}

void test_armax_deadface_across_entries() {
    const std::string raw =
        "[Seed Entry]\n"
        "DEADFACE 00001234\n"
        "\n"
        "[Following Entry]\n"
        "04200010 AABBCCDD\n";
    const auto document = gba::armax::parse(raw, {false});
    const auto encrypted = gba::armax::export_document(document, {true});
    require(encrypted.success &&
            encrypted.text.find("9C7FE77C A5352DCA") != std::string::npos &&
            encrypted.text.find("D9B2E263 140B0C5A") != std::string::npos,
            "AR MAX DEADFACE key did not continue across cheat entries");
    const auto decrypted =
        gba::armax::transform_text(encrypted.text, true, false);
    require(decrypted.success &&
            decrypted.text.find("04200010 AABBCCDD") != std::string::npos,
            "Cross-entry AR MAX DEADFACE stream did not decrypt");
}

void test_armax_special_mask_rejects_reserved_aliases() {
    const std::string input =
        "[AR Reserved Aliases]\n"
        "00000000 11000010\n"
        "00000000 19000020\n";

    const auto document = gba::armax::parse(input, {false});
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 2U &&
            document.entries[0].operations[0].kind ==
                gba::OperationKind::Unsupported &&
            document.entries[0].operations[1].kind ==
                gba::OperationKind::Unsupported,
            "AR MAX reserved-bit aliases were accepted as valid special rows");
}

void test_armax_patch_and_slowdown_roundtrip() {
    const std::string input =
        "[AR Specials]\n"
        "00000000 18000010\n"
        "00001234 DEADBEEF\n"
        "00000000 08001234\n";

    const auto document = gba::armax::parse(input, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 2U &&
            document.entries[0].operations[0].kind ==
                gba::OperationKind::RomPatch &&
            document.entries[0].operations[0].address == 0x08000020U &&
            document.entries[0].operations[0].value == 0x1234U &&
            document.entries[0].operations[0].encoding_auxiliary ==
                0xDEADBEEFU &&
            document.entries[0].operations[1].kind ==
                gba::OperationKind::DeviceSlowdown,
            "AR MAX ROM patch/slowdown metadata decoded incorrectly");

    const auto raw = gba::armax::export_document(document, {false});
    require(raw.success && raw.text == input + "\n",
            "AR MAX ROM patch/slowdown did not raw round-trip exactly");

    const auto encrypted = gba::armax::export_document(document, {true});
    const auto decrypted = gba::armax::parse(encrypted.text, {true});
    const auto raw_again = gba::armax::export_document(decrypted, {false});
    require(encrypted.success && raw_again.success &&
            raw_again.text == input + "\n",
            "AR MAX ROM patch/slowdown did not encrypted round-trip exactly");
}

void test_armax_foreign_seed_is_not_emitted() {
    gba::CheatDocument document;
    gba::CheatEntry entry;
    entry.name = "Foreign Seed";
    gba::Operation seed;
    seed.kind = gba::OperationKind::EncryptionSeed;
    seed.address = 0xDEADFACEU;
    seed.value = 0x1234U;
    entry.operations.push_back(seed);
    gba::Operation write;
    write.kind = gba::OperationKind::Write;
    write.address = 0x02000010U;
    write.value = 0xAABBCCDDU;
    write.width = 4U;
    entry.operations.push_back(write);
    document.entries.push_back(entry);

    const auto output = gba::armax::export_document(document, {false});
    require(output.success &&
            output.text.find("DEADFACE") == std::string::npos &&
            output.text.find("04200010 AABBCCDD") != std::string::npos,
            "Foreign encryption metadata was emitted as an AR MAX DEADFACE row");
}

void test_armax_block_if_else_roundtrip() {
    const std::string raw =
        "[AR MAX Block IF ELSE]\n"
        "8A200010 00001234\n"
        "04200100 11111111\n"
        "02200104 00002222\n"
        "00000000 60000000\n"
        "00200106 00000033\n"
        "00000000 40000000\n";

    const auto document = gba::armax::parse(raw, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 4U,
            "AR MAX block IF/ELSE did not parse cleanly");

    const auto& condition = document.entries[0].operations[0];
    require(condition.kind == gba::OperationKind::IfEqual &&
            condition.width == 2U &&
            condition.address == 0x02000010U &&
            condition.value == 0x1234U &&
            condition.condition_span == 2U &&
            condition.condition_else_span == 1U &&
            condition.condition_has_else &&
            condition.encoding_hint ==
                gba::EncodingHint::ActionReplayMaxBlock,
            "AR MAX block IF/ELSE branch metadata is incorrect");

    const auto exported = gba::armax::export_document(document, {false});
    require(exported.success &&
            exported.text.find("8A200010 00001234") != std::string::npos &&
            exported.text.find("00000000 60000000") != std::string::npos &&
            exported.text.find("00000000 40000000") != std::string::npos,
            "AR MAX block IF/ELSE did not round-trip in raw form");

    const auto encrypted = gba::armax::export_document(document, {true});
    require(encrypted.success,
            "AR MAX block IF/ELSE encrypted export failed");
    const auto decrypted_document =
        gba::armax::parse(encrypted.text, {true});
    const auto decrypted =
        gba::armax::export_document(decrypted_document, {false});
    require(decrypted.success &&
            decrypted.text.find("8A200010 00001234") != std::string::npos &&
            decrypted.text.find("00000000 60000000") != std::string::npos &&
            decrypted.text.find("00000000 40000000") != std::string::npos,
            "AR MAX encrypted block IF/ELSE did not round-trip");
}

void test_armax_empty_else_roundtrip() {
    const std::string raw =
        "[AR MAX Empty ELSE]\n"
        "8A200010 00001234\n"
        "04200100 11111111\n"
        "00000000 60000000\n"
        "00000000 40000000\n";

    const auto document = gba::armax::parse(raw, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 2U &&
            document.entries[0].operations[0].condition_has_else &&
            document.entries[0].operations[0].condition_else_span == 0U,
            "AR MAX empty ELSE marker was not retained");

    const auto exported = gba::armax::export_document(document, {false});
    require(exported.success &&
            exported.text.find("00000000 60000000") != std::string::npos &&
            exported.text.find("00000000 40000000") != std::string::npos,
            "AR MAX empty ELSE marker did not round-trip");
}

void test_armax_block_else_cross_family_safety() {
    const std::string raw =
        "[AR MAX ELSE Safety]\n"
        "8A200010 00001234\n"
        "04200100 11111111\n"
        "00000000 60000000\n"
        "04200104 22222222\n"
        "00000000 40000000\n";
    const auto document = gba::armax::parse(raw, {false});

    const auto fcd =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    require(!fcd.success &&
            fcd.text.find("11111111") == std::string::npos &&
            fcd.text.find("22222222") == std::string::npos,
            "AR MAX ELSE branches leaked into FCD output");

    const auto gameshark =
        gba::gameshark::export_document(document, {false});
    require(!gameshark.success &&
            gameshark.text.find("11111111") == std::string::npos &&
            gameshark.text.find("22222222") == std::string::npos,
            "AR MAX ELSE branches leaked into GameShark output");

    const auto ez = gba::ezflash::export_document(document);
    require(ez.success &&
            ez.text.find("=IF:W16,10,1234;W32:100,11111111;") !=
                std::string::npos &&
            ez.text.find("ELSE;W32:104,22222222;ENDIF;") !=
                std::string::npos,
            "AR MAX ELSE block did not map to Enhanced v4");
}

void test_armax_block_without_else_to_ez() {
    const std::string raw =
        "[AR MAX Block to EZ]\n"
        "8A200010 00001234\n"
        "04200100 11111111\n"
        "02200104 00002222\n"
        "00200106 00000033\n"
        "00000000 40000000\n";
    const auto document = gba::armax::parse(raw, {false});
    const auto ez = gba::ezflash::export_document(document);
    require(ez.success &&
            ez.text.find("=IF:W16,10,1234;") != std::string::npos &&
            ez.text.find("W32:100,11111111;W16:104,2222;W8:106,33;ENDIF;") !=
                std::string::npos,
            "AR MAX block without ELSE did not convert to one EZ IF group");
}

void test_armax_button_operations_roundtrip() {
    const std::string raw =
        "[AR MAX Device Button]\n"
        "00000000 10200100\n"
        "0000007F A5A5A5A5\n"
        "00000000 12200102\n"
        "00001234 00000000\n"
        "00000000 14200104\n"
        "89ABCDEF 12345678\n";

    const auto document = gba::armax::parse(raw, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 6U,
            "AR MAX device-button rows did not parse cleanly");

    const auto& operations = document.entries[0].operations;
    require(operations[0].kind == gba::OperationKind::IfDeviceButton &&
            operations[0].condition_span == 1U &&
            operations[1].kind == gba::OperationKind::Write &&
            operations[1].width == 1U &&
            operations[1].address == 0x02000100U &&
            operations[1].value == 0x7FU &&
            operations[1].encoding_parameter == 0xA5A5A5A5U,
            "AR MAX 8-bit device-button write decoded incorrectly");
    require(operations[3].width == 2U &&
            operations[3].address == 0x02000102U &&
            operations[3].value == 0x1234U,
            "AR MAX 16-bit device-button write decoded incorrectly");
    require(operations[5].width == 4U &&
            operations[5].address == 0x02000104U &&
            operations[5].value == 0x89ABCDEFU &&
            operations[5].encoding_parameter == 0x12345678U,
            "AR MAX 32-bit device-button write decoded incorrectly");

    const auto exported = gba::armax::export_document(document, {false});
    require(exported.success &&
            exported.text.find("00000000 10200100") != std::string::npos &&
            exported.text.find("0000007F A5A5A5A5") != std::string::npos &&
            exported.text.find("00000000 12200102") != std::string::npos &&
            exported.text.find("00001234 00000000") != std::string::npos &&
            exported.text.find("00000000 14200104") != std::string::npos &&
            exported.text.find("89ABCDEF 12345678") != std::string::npos,
            "AR MAX device-button rows did not round-trip exactly");

    const auto encrypted = gba::armax::export_document(document, {true});
    const auto decrypted_document =
        gba::armax::parse(encrypted.text, {true});
    const auto decrypted =
        gba::armax::export_document(decrypted_document, {false});
    require(encrypted.success && decrypted.success &&
            decrypted.text.find("00000000 10200100") != std::string::npos &&
            decrypted.text.find("00000000 14200104") != std::string::npos,
            "AR MAX device-button encrypted round-trip failed");

    const auto gameshark =
        gba::gameshark::export_document(document, {false});
    require(!gameshark.success &&
            gameshark.text.find("82100100 0000007F") != std::string::npos &&
            gameshark.text.find("82200102 00001234") != std::string::npos &&
            gameshark.text.find("89ABCDEF") == std::string::npos,
            "AR MAX device-button cross-conversion to GameShark is incorrect");

    const auto fcd =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    const auto ez = gba::ezflash::export_document(document);
    require(!fcd.success && !ez.success &&
            fcd.text.find("007F") == std::string::npos &&
            ez.text.find("7F") == std::string::npos,
            "AR MAX device-button writes leaked into unsupported outputs");
}

void test_armax_unterminated_block_is_safe() {
    const std::string raw =
        "[Broken Block]\n"
        "8A200010 00001234\n"
        "04200100 11111111\n";
    const auto document = gba::armax::parse(raw, {false});
    require(!document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.empty(),
            "Unterminated AR MAX block was not discarded safely");
}

} // namespace gba::tests
