#include "test_support.hpp"

namespace gba::tests {

void test_codebreaker_to_gameshark_raw() {
    const std::string input =
        "Infinite Money:\n"
        "82025BC4 423F\n"
        "82025BC6 000F\n";

    const auto document = gba::codebreaker::parse(input, {false});
    const auto output = gba::gameshark::export_document(document, {false});

    require(output.text.find("12025BC4 0000423F") != std::string::npos &&
            output.text.find("12025BC6 0000000F") != std::string::npos,
            "CodeBreaker writes did not encode as GameShark raw");
    require(output.success, "GameShark raw export unexpectedly failed");
}

void test_gameshark_32bit_to_codebreaker() {
    const std::string input =
        "Long Write:\n"
        "22025BC4 000F423F\n";

    const auto document = gba::gameshark::parse(input, {false});
    const auto output =
        gba::codebreaker::export_document(document, {false, std::nullopt});

    require(output.text.find("82025BC4 423F") != std::string::npos &&
            output.text.find("82025BC6 000F") != std::string::npos,
            "GameShark 32-bit write did not split into CodeBreaker 16-bit rows");
    require(output.success, "CodeBreaker export unexpectedly failed");
}

void test_gameshark_encrypted_roundtrip() {
    const std::string input =
        "Encrypted Datel:\n"
        "12025BC4 0000423F\n"
        "D2024C10 00009EB7\n"
        "02024C10 000000C2\n";

    const auto raw_document = gba::gameshark::parse(input, {false});
    const auto encrypted =
        gba::gameshark::export_document(raw_document, {true});
    require(encrypted.success,
            "GameShark encryption export unexpectedly failed");
    require(encrypted.text.find("12025BC4 0000423F") == std::string::npos,
            "GameShark encrypted output remained raw");

    const auto decrypted_document =
        gba::gameshark::parse(encrypted.text, {true});
    const auto raw_again =
        gba::gameshark::export_document(decrypted_document, {false});

    require(raw_again.text.find("12025BC4 0000423F") != std::string::npos &&
            raw_again.text.find("D2024C10 00009EB7") != std::string::npos &&
            raw_again.text.find("02024C10 000000C2") != std::string::npos,
            "GameShark encrypted roundtrip failed");
}

void test_codebreaker_condition_to_gameshark() {
    const std::string input =
        "Cross Condition:\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n";

    const auto document = gba::codebreaker::parse(input, {false});
    const auto output = gba::gameshark::export_document(document, {false});

    require(output.text.find("D2024C10 00009EB7") != std::string::npos &&
            output.text.find("02024C10 000000C2") != std::string::npos,
            "CodeBreaker condition did not encode as GameShark raw");
}

void test_gameshark_deadface_key_derivation() {
    require(gba::crypto::game_shark_v1_key_from_deadface(0U) ==
                gba::crypto::GameSharkV1Key,
            "GameShark DEADFACE seed zero did not restore the default key");

    const gba::crypto::TeaKey expected_1234{
        0x1040BADEU, 0xABDB5579U, 0x0838B2D6U, 0xDB0B85A9U};
    const gba::crypto::TeaKey expected_5678{
        0xC0DA1968U, 0x4761A0EFU, 0x7B95D423U, 0xF40E4D9CU};
    require(gba::crypto::game_shark_v1_key_from_deadface(0x1234U) ==
                expected_1234 &&
            gba::crypto::game_shark_v1_key_from_deadface(0x5678U) ==
                expected_5678,
            "GameShark DEADFACE ROM-table key derivation changed");
}

void test_gameshark_dynamic_deadface_roundtrip() {
    const std::string raw =
        "[GS Dynamic]\n"
        "DEADFACE 00001234\n"
        "22000010 AABBCCDD\n"
        "DEADFACE 00005678\n"
        "12000020 00001234\n";

    const std::string expected_encrypted =
        "[GS Dynamic]\n"
        "70BDB80D 69F37FCB\n"
        "C8E56E46 70237E64\n"
        "8A821844 DB131810\n"
        "0EE3D99B B834B661\n\n";

    const auto document = gba::gameshark::parse(raw, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 4U &&
            document.entries[0].operations[0].kind ==
                gba::OperationKind::EncryptionSeed &&
            document.entries[0].operations[2].kind ==
                gba::OperationKind::EncryptionSeed,
            "GameShark DEADFACE rows did not parse as rolling key changes");

    const auto encrypted = gba::gameshark::export_document(document, {true});
    require(encrypted.success && encrypted.text == expected_encrypted,
            "GameShark dynamic DEADFACE encrypted vector changed");

    const auto decrypted = gba::gameshark::parse(encrypted.text, {true});
    const auto raw_export = gba::gameshark::export_document(decrypted, {false});
    require(raw_export.success && raw_export.text == raw + "\n",
            "GameShark dynamic DEADFACE stream did not round-trip exactly");
}

void test_gameshark_armax_master_id_roundtrip() {
    const std::string input =
        "Shared Master ID:\n"
        "12345678 001DC0DE\n"
        "F8012346 00000007\n";

    const auto gameshark_document =
        gba::gameshark::parse(input, {false});
    require(gameshark_document.warnings.empty() &&
            gameshark_document.entries.size() == 1U &&
            gameshark_document.entries[0].operations.size() == 2U &&
            gameshark_document.entries[0].operations[0].kind ==
                gba::OperationKind::GameId &&
            gameshark_document.entries[0].operations[1].kind ==
                gba::OperationKind::Hook,
            "GameShark game-ID/hook master rows were not recognized");

    const auto gameshark_raw =
        gba::gameshark::export_document(gameshark_document, {});
    require(gameshark_raw.success &&
            gameshark_raw.text.find("12345678 001DC0DE") !=
                std::string::npos &&
            gameshark_raw.text.find("F8012346 00000007") !=
                std::string::npos,
            "GameShark master ID/hook did not roundtrip exactly");

    const auto armax =
        gba::armax::export_document(gameshark_document, {});
    require(armax.success &&
            armax.text.find("12345678 001DC0DE") != std::string::npos &&
            armax.text.find("C4012346 00000007") != std::string::npos,
            "GameShark master ID/hook did not convert exactly to AR MAX");

    const auto armax_document = gba::armax::parse(armax.text, {false});
    const auto back_to_gameshark =
        gba::gameshark::export_document(armax_document, {});
    require(back_to_gameshark.success &&
            back_to_gameshark.text.find("12345678 001DC0DE") !=
                std::string::npos &&
            back_to_gameshark.text.find("F8012346 00000007") !=
                std::string::npos,
            "AR MAX master ID/hook did not convert back to GameShark");

    const auto fcd =
        gba::codebreaker::export_document(gameshark_document, {});
    require(!fcd.success &&
            fcd.text.find("C0DE") == std::string::npos &&
            fcd.text.find("10012346 0007") != std::string::npos,
            "Foreign 32-bit game ID was truncated instead of being noted by FCD");
}

void test_gameshark_device_button_roundtrip() {
    const std::string input =
        "[Device Button]\n"
        "82112345 0000007F\n"
        "83223456 00001234\n";

    const auto document = gba::gameshark::parse(input, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 4U,
            "GameShark device-button rows did not decode cleanly");

    const auto& operations = document.entries[0].operations;
    require(
        operations[0].kind == gba::OperationKind::IfDeviceButton &&
        operations[0].condition_span == 1U &&
        operations[1].kind == gba::OperationKind::Write &&
        operations[1].width == 1U &&
        operations[1].address == 0x02012345U &&
        operations[1].value == 0x7FU &&
        operations[1].encoding_hint ==
            gba::EncodingHint::GameSharkButtonWrite,
        "GameShark 8-bit device-button write decoded incorrectly");

    require(
        operations[2].kind == gba::OperationKind::IfDeviceButton &&
        operations[3].kind == gba::OperationKind::Write &&
        operations[3].width == 2U &&
        operations[3].address == 0x03023456U &&
        operations[3].value == 0x1234U &&
        operations[3].encoding_hint ==
            gba::EncodingHint::GameSharkButtonWrite,
        "GameShark 16-bit device-button write decoded incorrectly");

    const auto raw =
        gba::gameshark::export_document(document, {false});
    require(raw.success &&
            raw.text.find("82112345 0000007F") != std::string::npos &&
            raw.text.find("83223456 00001234") != std::string::npos,
            "GameShark device-button rows did not round-trip raw");
}

void test_gameshark_device_button_encrypted_roundtrip() {
    const std::string input =
        "[Encrypted Device Button]\n"
        "82112345 0000007F\n"
        "83223456 00001234\n";

    const auto document = gba::gameshark::parse(input, {false});
    const auto encrypted =
        gba::gameshark::export_document(document, {true});
    require(encrypted.success &&
            encrypted.text.find("82112345 0000007F") == std::string::npos,
            "GameShark device-button encrypted output remained raw");

    const auto decrypted =
        gba::gameshark::parse(encrypted.text, {true});
    const auto raw =
        gba::gameshark::export_document(decrypted, {false});

    require(raw.success &&
            raw.text.find("82112345 0000007F") != std::string::npos &&
            raw.text.find("83223456 00001234") != std::string::npos,
            "Encrypted GameShark device-button rows did not round-trip");
}

void test_gameshark_device_button_does_not_leak() {
    const std::string input =
        "[Device Button Safety]\n"
        "82112345 0000007F\n";

    const auto document = gba::gameshark::parse(input, {false});

    const auto fcd =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    require(!fcd.success &&
            fcd.text.find("32012345 007F") == std::string::npos &&
            fcd.text.find("82012345 007F") == std::string::npos,
            "GameShark device-button write leaked into FCD output");

    const auto armax =
        gba::armax::export_document(document, {false});
    require(armax.success &&
            armax.text.find("00000000 10212345") != std::string::npos &&
            armax.text.find("0000007F 00000000") != std::string::npos,
            "GameShark device-button write did not convert to AR MAX");

    const auto ez = gba::ezflash::export_document(document);
    require(!ez.success &&
            ez.text.find("12345,7F") == std::string::npos,
            "GameShark device-button write leaked into EZ-Flash output");
}

void test_gameshark_condition_variants_roundtrip() {
    const std::string input =
        "[Condition Variants]\n"
        "D2000010 00001234\n"
        "02000100 00000011\n"
        "D2000012 00105678\n"
        "12000102 00002222\n"
        "D2000014 00209ABC\n"
        "02000104 00000033\n"
        "D2000016 0030DEF0\n"
        "12000106 00004444\n";

    const auto document = gba::gameshark::parse(input, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 8U,
            "GameShark condition variants did not parse cleanly");

    const auto& operations = document.entries[0].operations;
    require(
        operations[0].kind == gba::OperationKind::IfEqual &&
        operations[2].kind == gba::OperationKind::IfNotEqual &&
        operations[4].kind == gba::OperationKind::IfLessOrEqual &&
        operations[6].kind == gba::OperationKind::IfGreaterOrEqual,
        "GameShark condition subtypes were decoded incorrectly");

    for (std::size_t index : {0U, 2U, 4U, 6U}) {
        require(operations[index].width == 2U &&
                operations[index].condition_span == 1U,
                "GameShark condition width/scope was not preserved");
    }

    const auto raw =
        gba::gameshark::export_document(document, {false});
    require(raw.success &&
            raw.text.find("D2000010 00001234") != std::string::npos &&
            raw.text.find("D2000012 00105678") != std::string::npos &&
            raw.text.find("D2000014 00209ABC") != std::string::npos &&
            raw.text.find("D2000016 0030DEF0") != std::string::npos,
            "GameShark condition variants did not round-trip raw");

    const auto encrypted =
        gba::gameshark::export_document(document, {true});
    require(encrypted.success, "GameShark condition encryption failed");

    const auto decrypted =
        gba::gameshark::parse(encrypted.text, {true});
    const auto decrypted_raw =
        gba::gameshark::export_document(decrypted, {false});
    require(decrypted_raw.success &&
            decrypted_raw.text.find("D2000010 00001234") != std::string::npos &&
            decrypted_raw.text.find("D2000016 0030DEF0") != std::string::npos,
            "Encrypted GameShark condition variants did not round-trip");
}

void test_gameshark_inclusive_conditions_do_not_leak() {
    const std::string input =
        "[Inclusive Safety]\n"
        "D2000014 00209ABC\n"
        "02000104 00000033\n"
        "D2000016 0030DEF0\n"
        "12000106 00004444\n";

    const auto document = gba::gameshark::parse(input, {false});

    const auto fcd =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    require(!fcd.success &&
            fcd.text.find("32000104 0033") == std::string::npos &&
            fcd.text.find("82000106 4444") == std::string::npos,
            "Inclusive GameShark condition write leaked into FCD output");

    const auto armax =
        gba::armax::export_document(document, {false});
    require(!armax.success &&
            armax.text.find("00200104 00000033") == std::string::npos &&
            armax.text.find("02200106 00004444") == std::string::npos,
            "Inclusive GameShark condition write leaked into AR MAX output");

    const auto ez = gba::ezflash::export_document(document);
    require(ez.success &&
            ez.text.find("IFLE=14,BC,9A;ON=104,33;") != std::string::npos &&
            ez.text.find("IFGE=16,F0,DE;ON=106,44,44;") != std::string::npos,
            "Inclusive GameShark conditions did not map to Enhanced v3");
}

void test_gameshark_rom_patch_roundtrip() {
    const std::string input =
        "[ROM Patches]\n"
        "60012345 0000ABCD\n"
        "60054321 10001234\n"
        "60000010 2000BEEF\n";

    const auto document = gba::gameshark::parse(input, {false});
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 3U,
            "GameShark ROM patches did not parse");

    const auto& operations = document.entries[0].operations;
    require(operations[0].kind == gba::OperationKind::RomPatch &&
            operations[0].address == 0x0802468AU &&
            operations[0].value == 0xABCDU &&
            operations[0].encoding_parameter == 0U &&
            operations[1].address == 0x080A8642U &&
            operations[1].encoding_parameter == 0x10000000U &&
            operations[2].address == 0x08000020U &&
            operations[2].encoding_parameter == 0x20000000U,
            "GameShark ROM patch address/mode decoding is incorrect");

    const auto raw = gba::gameshark::export_document(document, {false});
    require(raw.success &&
            raw.text.find("60012345 0000ABCD") != std::string::npos &&
            raw.text.find("60054321 10001234") != std::string::npos &&
            raw.text.find("60000010 2000BEEF") != std::string::npos,
            "GameShark ROM patches did not round-trip raw");

    const auto encrypted = gba::gameshark::export_document(document, {true});
    require(encrypted.success,
            "GameShark ROM patch encryption failed");
    const auto decrypted = gba::gameshark::parse(encrypted.text, {true});
    const auto decrypted_raw =
        gba::gameshark::export_document(decrypted, {false});
    require(decrypted_raw.success &&
            decrypted_raw.text.find("60012345 0000ABCD") != std::string::npos &&
            decrypted_raw.text.find("60054321 10001234") != std::string::npos &&
            decrypted_raw.text.find("60000010 2000BEEF") != std::string::npos,
            "Encrypted GameShark ROM patches did not round-trip");
}

void test_gameshark_rom_patch_dependency_safety() {
    const std::string input =
        "[Patch Dependency]\n"
        "60012345 0000ABCD\n"
        "02000100 12345678\n";
    const auto document = gba::gameshark::parse(input, {false});

    const auto fcd =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    require(!fcd.success && fcd.text.empty() &&
            fcd.warnings.end() != std::find_if(
                fcd.warnings.begin(), fcd.warnings.end(),
                [](const std::string& warning) {
                    return warning.find("entire dependent entry was skipped") !=
                           std::string::npos;
                }),
            "GameShark ROM-patch entry leaked into FCD output");

    const auto armax = gba::armax::export_document(document, {false});
    require(!armax.success && armax.text.empty(),
            "GameShark ROM-patch entry leaked into AR MAX output");

    gba::ezflash::Options enhanced_options;
    enhanced_options.mode = gba::ezflash::Mode::Enhanced;
    const auto enhanced =
        gba::ezflash::export_document(document, enhanced_options);
    require(enhanced.success &&
            enhanced.text.find("ON=100,78;ROM:0802468A,CD,AB;") !=
                std::string::npos,
            "GameShark mode-0 ROM patch did not convert to Enhanced .cht");

    gba::ezflash::Options original_options;
    original_options.mode = gba::ezflash::Mode::Original;
    const auto original =
        gba::ezflash::export_document(document, original_options);
    require(!original.success && original.text.empty(),
            "EZ-Flash Original partially emitted a ROM-patch entry");
}

void test_gameshark_slowdown_roundtrip_and_safety() {
    const std::string input =
        "[Slowdown]\n"
        "80F00000 00001234\n"
        "22000100 89ABCDEF\n";
    const auto document = gba::gameshark::parse(input, {false});
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 2U &&
            document.entries[0].operations[0].kind ==
                gba::OperationKind::DeviceSlowdown &&
            document.entries[0].operations[0].value == 0x1234U,
            "GameShark slowdown row did not parse");

    const auto raw = gba::gameshark::export_document(document, {false});
    require(raw.success &&
            raw.text.find("80F00000 00001234") != std::string::npos &&
            raw.text.find("22000100 89ABCDEF") != std::string::npos,
            "GameShark slowdown row did not round-trip raw");

    const auto encrypted = gba::gameshark::export_document(document, {true});
    require(encrypted.success,
            "GameShark slowdown encryption failed");
    const auto decrypted = gba::gameshark::parse(encrypted.text, {true});
    const auto decrypted_raw =
        gba::gameshark::export_document(decrypted, {false});
    require(decrypted_raw.success &&
            decrypted_raw.text.find("80F00000 00001234") != std::string::npos,
            "Encrypted GameShark slowdown row did not round-trip");

    const auto fcd =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    require(!fcd.success &&
            fcd.text.find("82000100 CDEF") != std::string::npos &&
            fcd.text.find("82000102 89AB") != std::string::npos &&
            fcd.text.find("80F00000") == std::string::npos,
            "GameShark slowdown became an executable FCD operation");
}

void test_gameshark_noncanonical_patch_is_not_ram_write() {
    const std::string input =
        "[Bad Patch]\n"
        "6F012345 0000ABCD\n";
    const auto document = gba::gameshark::parse(input, {false});
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 1U &&
            document.entries[0].operations[0].kind ==
                gba::OperationKind::Unsupported,
            "Noncanonical GameShark type-6 row was not rejected safely");

    const auto fcd =
        gba::codebreaker::export_document(document, {false, std::nullopt});
    require(!fcd.success && fcd.text.empty(),
            "Noncanonical GameShark patch was converted to a RAM write");
}

void test_gameshark_assignment_list_roundtrip() {
    const std::string input =
        "[Assignment List]\n"
        "30000004 02004000\n"
        "02001000 02002000\n"
        "02003000 00000000\n";

    const auto document = gba::gameshark::parse(input, {false});
    require(document.warnings.empty(),
            "Valid GameShark assignment list produced warnings");
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 4U,
            "GameShark assignment list did not expand to four semantic writes");

    const auto& operations = document.entries[0].operations;
    require(operations[0].address == 0x02004000U &&
            operations[1].address == 0x02001000U &&
            operations[2].address == 0x02002000U &&
            operations[3].address == 0x02003000U,
            "GameShark assignment-list addresses were decoded incorrectly");
    require(std::all_of(
                operations.begin(), operations.end(),
                [](const gba::Operation& operation) {
                    return operation.kind == gba::OperationKind::Write &&
                           operation.width == 4U &&
                           operation.value == 0x02004000U &&
                           operation.encoding_hint ==
                               gba::EncodingHint::GameSharkAssignmentList;
                }),
            "GameShark assignment-list writes lost their group metadata");

    const auto raw =
        gba::gameshark::export_document(document, {false});
    require(raw.success &&
            raw.text.find("30000004 02004000") != std::string::npos &&
            raw.text.find("02001000 02002000") != std::string::npos &&
            raw.text.find("02003000 00000000") != std::string::npos,
            "GameShark assignment list did not compactly round-trip");
}

void test_gameshark_assignment_list_encrypted_roundtrip() {
    const std::string input =
        "[Encrypted Assignment]\n"
        "30000004 02004000\n"
        "02001000 02002000\n"
        "02003000 00000000\n";

    const auto document = gba::gameshark::parse(input, {false});
    const auto encrypted =
        gba::gameshark::export_document(document, {true});
    require(encrypted.success &&
            encrypted.text.find("30000004 02004000") == std::string::npos,
            "GameShark assignment-list encryption remained raw");

    const auto decrypted =
        gba::gameshark::parse(encrypted.text, {true});
    const auto raw =
        gba::gameshark::export_document(decrypted, {false});

    require(raw.success &&
            raw.text.find("30000004 02004000") != std::string::npos &&
            raw.text.find("02001000 02002000") != std::string::npos &&
            raw.text.find("02003000 00000000") != std::string::npos,
            "Encrypted GameShark assignment list did not round-trip");
}

void test_gameshark_assignment_list_cross_family_expansion() {
    const std::string input =
        "[Assignment Expansion]\n"
        "30000004 02004000\n"
        "02001000 02002000\n"
        "02003000 00000000\n";

    const auto document = gba::gameshark::parse(input, {false});

    const auto armax =
        gba::armax::export_document(document, {false});
    require(armax.success &&
            armax.text.find("04204000 02004000") != std::string::npos &&
            armax.text.find("04201000 02004000") != std::string::npos &&
            armax.text.find("04202000 02004000") != std::string::npos &&
            armax.text.find("04203000 02004000") != std::string::npos,
            "GameShark assignment list did not expand safely to AR MAX");

    const auto ez = gba::ezflash::export_document(document);
    require(ez.success &&
            ez.text.find("ON=4000,00,40,00,02;") != std::string::npos &&
            ez.text.find("1000,00,40,00,02;") != std::string::npos &&
            ez.text.find("2000,00,40,00,02;") != std::string::npos &&
            ez.text.find("3000,00,40,00,02;") != std::string::npos,
            "GameShark assignment list did not expand safely to EZ-Flash");
}

void test_gameshark_multiline_condition_roundtrip() {
    const std::string input =
        "[Conditional Assignment]\n"
        "E0041234 02000010\n"
        "30000004 02004000\n"
        "02001000 02002000\n"
        "02003000 00000000\n";

    const auto document = gba::gameshark::parse(input, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 5U,
            "GameShark multiline condition did not parse cleanly");

    const auto& condition = document.entries[0].operations[0];
    require(condition.kind == gba::OperationKind::IfEqual &&
            condition.width == 2U &&
            condition.address == 0x02000010U &&
            condition.value == 0x1234U &&
            condition.condition_span == 4U,
            "GameShark multiline condition fields were decoded incorrectly");

    const auto raw =
        gba::gameshark::export_document(document, {false});
    require(raw.success &&
            raw.text.find("E0041234 02000010") != std::string::npos &&
            raw.text.find("30000004 02004000") != std::string::npos,
            "GameShark multiline condition did not compactly round-trip");

    const auto ez = gba::ezflash::export_document(document);
    require(ez.success &&
            ez.text.find("IF=10,34,12;ON=") != std::string::npos &&
            ez.text.find("4000,00,40,00,02;") != std::string::npos &&
            ez.text.find("3000,00,40,00,02;") != std::string::npos,
            "GameShark multiline condition did not preserve its full EZ scope");
}

void test_gameshark_type3_arithmetic_roundtrip() {
    const std::string input =
        "[GS Arithmetic]\n"
        "3010007F 02000010\n"
        "30200005 02000014\n"
        "30301234 02000018\n"
        "30405678 0200001C\n"
        "30500000 02000020\n"
        "89ABCDEF DEADC0DE\n"
        "30600000 02000024\n"
        "10203040 CAFEBABE\n";

    const auto document = gba::gameshark::parse(input, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 6U,
            "GameShark type-3 arithmetic variants did not parse cleanly");

    const auto& operations = document.entries[0].operations;
    require(operations[0].kind == gba::OperationKind::Add &&
            operations[0].value == 0x7FU &&
            operations[1].kind == gba::OperationKind::Subtract &&
            operations[1].value == 0x5U &&
            operations[2].kind == gba::OperationKind::Add &&
            operations[2].value == 0x1234U &&
            operations[3].kind == gba::OperationKind::Subtract &&
            operations[3].value == 0x5678U &&
            operations[4].kind == gba::OperationKind::Add &&
            operations[4].value == 0x89ABCDEFU &&
            operations[4].encoding_auxiliary == 0xDEADC0DEU &&
            operations[5].kind == gba::OperationKind::Subtract &&
            operations[5].value == 0x10203040U &&
            operations[5].encoding_auxiliary == 0xCAFEBABEU,
            "GameShark type-3 arithmetic operands were decoded incorrectly");

    const auto raw = gba::gameshark::export_document(document, {false});
    require(raw.success && raw.text == input + "\n",
            "GameShark type-3 arithmetic did not raw round-trip exactly");

    const auto encrypted = gba::gameshark::export_document(document, {true});
    const auto decrypted = gba::gameshark::parse(encrypted.text, {true});
    const auto raw_again = gba::gameshark::export_document(decrypted, {false});
    require(encrypted.success && raw_again.success &&
            raw_again.text == input + "\n",
            "GameShark type-3 arithmetic did not encrypted round-trip exactly");
}

void test_gameshark_multiline_condition_subtypes() {
    const std::string input =
        "[GS Condition Subtypes]\n"
        "E0011234 02000010\n"
        "02001000 00000001\n"
        "E0015678 12000020\n"
        "02001001 00000002\n"
        "E0019ABC 22000030\n"
        "02001002 00000003\n"
        "E001DEF0 32000040\n"
        "02001003 00000004\n";

    const auto document = gba::gameshark::parse(input, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 8U,
            "GameShark multiline condition subtypes did not parse cleanly");

    const auto& operations = document.entries[0].operations;
    require(operations[0].kind == gba::OperationKind::IfEqual &&
            operations[2].kind == gba::OperationKind::IfNotEqual &&
            operations[4].kind == gba::OperationKind::IfLessOrEqual &&
            operations[6].kind == gba::OperationKind::IfGreaterOrEqual,
            "GameShark multiline condition subtype mapping is incorrect");

    const auto raw = gba::gameshark::export_document(document, {false});
    require(raw.success && raw.text == input + "\n",
            "GameShark multiline condition subtypes did not raw round-trip exactly");

    const auto encrypted = gba::gameshark::export_document(document, {true});
    const auto decrypted = gba::gameshark::parse(encrypted.text, {true});
    const auto raw_again = gba::gameshark::export_document(decrypted, {false});
    require(encrypted.success && raw_again.success &&
            raw_again.text == input + "\n",
            "GameShark multiline condition subtypes did not encrypted round-trip");
}

} // namespace gba::tests
