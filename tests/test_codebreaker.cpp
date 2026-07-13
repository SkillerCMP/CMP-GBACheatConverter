#include "test_support.hpp"

namespace gba::tests {

void test_codebreaker_cipher_roundtrip() {
    gba::codebreaker::Cipher cipher;
    cipher.reseed({0x9ABCDEF0U, 0x1234U});

    const gba::codebreaker::RawLine raw{
        0x82025BC4U, 0x423FU, 1, "82025BC4 423F"
    };

    const auto encrypted = cipher.encrypt(raw);
    const auto decrypted = cipher.decrypt(encrypted);

    require(decrypted.op1 == raw.op1 && decrypted.op2 == raw.op2,
            "CodeBreaker encrypt/decrypt roundtrip failed");
}

void test_fcd_input_seed_helpers() {
    const auto colon = gba::codebreaker::parse_seed_text("9ABCDEF0:1234");
    const auto spaced = gba::codebreaker::parse_seed_text(" 9ABCDEF0 1234 ");
    require(colon && spaced,
            "FCD seed parser rejected a supported separator");
    require(colon->op1 == 0x9ABCDEF0U && colon->op2 == 0x1234U &&
            spaced->op1 == colon->op1 && spaced->op2 == colon->op2,
            "FCD seed parser returned the wrong seed");
    require(gba::codebreaker::format_seed(*colon) == "9ABCDEF0:1234",
            "FCD seed formatter returned the wrong canonical text");

    const auto embedded = gba::codebreaker::find_embedded_seed(
        "[Seed Test]\n9ABCDEF0 1234\n82001000 5678\n");
    require(embedded && embedded->op1 == 0x9ABCDEF0U &&
            embedded->op2 == 0x1234U,
            "Embedded FCD seed was not found");

    require(!gba::codebreaker::find_embedded_seed("9FCF3D80 D0A2\n"),
            "A lone leading-9 encrypted row was incorrectly detected as a key");
    require(!gba::codebreaker::find_embedded_seed("959D3F90 8497\n"),
            "A second lone leading-9 encrypted row was detected as a key");
    require(!gba::codebreaker::find_embedded_seed("9D70F94C 50AC\n"),
            "A third lone leading-9 encrypted row was detected as a key");

    const auto actual_list_key = gba::codebreaker::find_embedded_seed(
        "94345ABD 9812\n9FCF3D80 D0A2\nA8D12C50 10F7\n");
    require(actual_list_key && actual_list_key->op1 == 0x94345ABDU &&
            actual_list_key->op2 == 0x9812U,
            "The real first-row key was not detected from a full code list");

    require(!gba::codebreaker::find_embedded_seed(
                "[Keyless]\n82001000 5678\n9ABCDEF0 1234\n"),
            "A later encrypted-looking row was incorrectly treated as the "
            "initial plaintext FCD seed");
}

void test_fcd_keyless_encrypted_input_seed() {
    gba::CheatDocument source;
    gba::CheatEntry entry;
    entry.name = "Keyless Input";
    gba::Operation write;
    write.kind = gba::OperationKind::Write;
    write.address = 0x02001000U;
    write.value = 0x5678U;
    write.width = 2U;
    entry.operations.push_back(write);
    source.entries.push_back(entry);

    const gba::codebreaker::Seed seed{0x94345ABDU, 0x9812U};
    gba::codebreaker::ExportOptions options;
    options.encrypted = true;
    options.seed = seed;
    const auto encrypted = gba::codebreaker::export_document(source, options);
    require(encrypted.success, "FCD keyless test export failed");

    const std::size_t seed_end = encrypted.text.find('\n');
    require(seed_end != std::string::npos,
            "FCD encrypted output did not contain its seed row");
    const std::string keyless = encrypted.text.substr(seed_end + 1U);

    const auto decoded = gba::codebreaker::parse(keyless, {true, seed, true});
    require(decoded.warnings.empty(),
            "Manual FCD input seed produced an unexpected warning");
    require(direct_write_bytes(decoded) == direct_write_bytes(source),
            "Manual FCD input seed did not decrypt a keyless list");
}

void test_fcd_manual_key_for_leading_9_payload() {
    const gba::codebreaker::Seed key{0x94345ABDU, 0x9812U};
    gba::codebreaker::RawLine raw{
        0x82001000U, 0x5678U, 1, "82001000 5678"
    };
    gba::codebreaker::RawLine encrypted;
    bool found = false;

    for (std::uint32_t offset = 0; offset < 0x1000U; ++offset) {
        raw.op1 = 0x82001000U + offset;
        gba::codebreaker::Cipher cipher;
        cipher.reseed(key);
        encrypted = cipher.encrypt(raw);
        if ((encrypted.op1 >> 28U) == 0x9U) {
            found = true;
            break;
        }
    }

    require(found, "Could not construct a leading-9 encrypted FCD payload");
    const std::string keyless =
        gba::text::hex(encrypted.op1, 8) + " " +
        gba::text::hex(encrypted.op2, 4) + "\n";

    require(!gba::codebreaker::find_embedded_seed(keyless),
            "A lone encrypted 9 payload was incorrectly auto-detected as a key");

    const auto decoded = gba::codebreaker::parse(
        keyless, {true, key, true});
    require(decoded.warnings.empty(),
            "Manual Key produced an unexpected warning for a leading-9 payload");
    require(!decoded.entries.empty() &&
            !decoded.entries.front().operations.empty(),
            "Manual Key did not preserve the leading-9 payload row");
    const auto& operation = decoded.entries.front().operations.front();
    require(operation.kind == gba::OperationKind::Write &&
            operation.address == (raw.op1 & 0x0FFFFFFFU) &&
            operation.value == raw.op2 && operation.width == 2U,
            "Manual Key did not decrypt the leading-9 payload correctly");
}

void test_fcd_embedded_seed_overrides_manual_seed() {
    gba::CheatDocument source;
    gba::CheatEntry entry;
    entry.name = "Embedded Override";
    gba::Operation write;
    write.kind = gba::OperationKind::Write;
    write.address = 0x02002000U;
    write.value = 0xABCDU;
    write.width = 2U;
    entry.operations.push_back(write);
    source.entries.push_back(entry);

    gba::codebreaker::ExportOptions options;
    options.encrypted = true;
    options.seed = gba::codebreaker::Seed{0x9ABCDEF0U, 0x1234U};
    const auto encrypted = gba::codebreaker::export_document(source, options);

    const auto decoded = gba::codebreaker::parse(
        encrypted.text,
        {true, gba::codebreaker::Seed{0x94345ABDU, 0x9812U}});
    require(direct_write_bytes(decoded) == direct_write_bytes(source),
            "Embedded FCD seed did not override a mismatched manual seed");
    require(std::any_of(
                decoded.warnings.begin(), decoded.warnings.end(),
                [](const std::string& warning) {
                    return warning.find("overrides supplied input key") !=
                        std::string::npos;
                }),
            "Mismatched embedded/manual FCD seed warning was not emitted");
}

void test_encrypted_stream_type9_collision() {
    const std::string raw =
        "Round Trip:\n"
        "82025BC4 423F\n"
        "72024C10 9EB7\n"
        "32024C10 00C2\n";

    const gba::codebreaker::Seed seed{0x9ABCDEF0U, 0x1234U};
    const std::string encrypted =
        gba::codebreaker::format_raw(raw, false, seed);

    require(encrypted.find("909EE49B 10CB") != std::string::npos,
            "Expected encrypted type-9 collision vector was not produced");

    const std::string decrypted =
        gba::codebreaker::format_raw(encrypted, true, std::nullopt);

    require(decrypted.find("82025BC4 423F") != std::string::npos &&
            decrypted.find("72024C10 9EB7") != std::string::npos &&
            decrypted.find("32024C10 00C2") != std::string::npos,
            "Encrypted payload beginning with 9 was treated as a seed");
}

void test_codebreaker_slide_parse_and_compact_export() {
    const std::string input =
        "Slide Test:\n"
        "42001000 1000\n"
        "00010004 0002\n";

    const auto document = gba::codebreaker::parse(input, {false});
    require(document.warnings.empty(),
            "Valid CodeBreaker slide produced a parser warning");
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 1U,
            "CodeBreaker slide did not decode to one semantic operation");

    const auto& operation = document.entries[0].operations[0];
    require(operation.kind == gba::OperationKind::Write &&
            operation.address == 0x02001000U &&
            operation.value == 0x1000U &&
            operation.width == 2U &&
            operation.repeat == 4U &&
            operation.address_step == 2 &&
            operation.value_step == 1,
            "CodeBreaker slide fields decoded incorrectly");

    const auto raw = gba::codebreaker::export_document(document, {});
    require(raw.text.find("42001000 1000\n00010004 0002") !=
                std::string::npos,
            "CodeBreaker slide was expanded instead of compactly re-emitted");
}

void test_codebreaker_slide_to_ez() {
    const std::string input =
        "Slide EZ:\n"
        "42001000 1000\n"
        "00010004 0002\n";

    const auto document = gba::codebreaker::parse(input, {false});
    const auto output = gba::ezflash::export_document(document);
    require(output.text.find(
                "SLIDE=1000,00000004,00000002,00000001,00,10;") !=
                std::string::npos,
            "CodeBreaker slide did not convert to Enhanced v3 SLIDE");
}

void test_codebreaker_packed_list_parse_and_export() {
    const std::string input =
        "Packed Test:\n"
        "52002000 0005\n"
        "3412CDAB 010F\n"
        "68245713 0000\n";

    const auto document = gba::codebreaker::parse(input, {false});
    require(document.warnings.empty(),
            "Valid packed list produced a parser warning");
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 5U,
            "Packed list created the wrong number of writes");

    const std::uint32_t expected_values[] = {
        0x1234U, 0xABCDU, 0x0F01U, 0x2468U, 0x1357U
    };
    for (std::size_t index = 0; index < 5U; ++index) {
        const auto& operation = document.entries[0].operations[index];
        require(operation.kind == gba::OperationKind::Write &&
                operation.address == 0x02002000U + index * 2U &&
                operation.value == expected_values[index] &&
                operation.width == 2U &&
                operation.repeat == 1U,
                "Packed list value/address decoding failed");
    }

    const auto raw = gba::codebreaker::export_document(document, {});
    require(raw.text.find(
                "52002000 0005\n"
                "3412CDAB 010F\n"
                "68245713 0000") != std::string::npos,
            "Packed list was not compactly re-emitted");
    require(raw.text.find("82002000 0000") == std::string::npos,
            "Packed list parser retained the old zero placeholder write");
}

void test_codebreaker_packed_list_to_ez() {
    const std::string input =
        "Packed EZ:\n"
        "52002000 0005\n"
        "3412CDAB 010F\n"
        "68245713 0000\n";

    const auto document = gba::codebreaker::parse(input, {false});
    const auto output = gba::ezflash::export_document(document);
    require(output.text.find(
                "ON=2000,34,12,CD,AB,01,0F,68,24,57,13;") !=
                std::string::npos,
            "Packed list did not expand correctly for EZ-Flash");
}

void test_codebreaker_multiline_continuation_type9_collision() {
    const std::string input =
        "Continuation Collision:\n"
        "42003000 1000\n"
        "90010004 0002\n";

    const auto document = gba::codebreaker::parse(input, {false});
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 1U,
            "Type-9-looking slide continuation was mistaken for a seed");
    const auto& operation = document.entries[0].operations[0];
    require(operation.repeat == 4U &&
            operation.value_step == 0x9001 &&
            operation.address_step == 2,
            "Type-9-looking continuation did not decode as slide data");
}

void test_fcd_multiline_encrypted_roundtrip() {
    const std::string input =
        "Encrypted Multi-Line:\n"
        "42001000 1000\n"
        "00010004 0002\n"
        "52002000 0005\n"
        "3412CDAB 010F\n"
        "68245713 0000\n";

    const auto raw_document = gba::codebreaker::parse(input, {false});
    gba::codebreaker::ExportOptions encrypted_options;
    encrypted_options.encrypted = true;
    encrypted_options.seed =
        gba::codebreaker::Seed{0x9ABCDEF0U, 0x1234U};
    const auto encrypted = gba::codebreaker::export_document(
        raw_document, encrypted_options);
    require(encrypted.success,
            "FCD multi-line encrypted export failed");

    const auto decrypted_document =
        gba::codebreaker::parse(encrypted.text, {true});
    const auto raw_again =
        gba::codebreaker::export_document(decrypted_document, {});

    require(raw_again.text.find(
                "42001000 1000\n"
                "00010004 0002") != std::string::npos,
            "Encrypted FCD slide did not roundtrip");
    require(raw_again.text.find(
                "52002000 0005\n"
                "3412CDAB 010F\n"
                "68245713 0000") != std::string::npos,
            "Encrypted FCD packed list did not roundtrip");
}

void test_fcd_condition_variants_roundtrip() {
    const std::string input =
        "Condition Variants:\n"
        "72001000 1111\n"
        "82002000 0001\n"
        "A2001002 2222\n"
        "82002002 0002\n"
        "B2001004 3333\n"
        "82002004 0003\n"
        "C2001006 4444\n"
        "82002006 0004\n"
        "F2001008 000F\n"
        "82002008 0005\n"
        "D0000020 FFFE\n"
        "8200200A 0006\n";

    const auto document = gba::codebreaker::parse(input, {false});
    require(document.warnings.empty(),
            "Valid FCD condition variants produced parser warnings");
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 12U,
            "FCD condition variants decoded to the wrong operation count");

    const auto& button = document.entries[0].operations[10];
    require(button.kind == gba::OperationKind::IfNand &&
            button.address == 0x04000130U &&
            button.value == 0xFFFEU &&
            button.condition_span == 1U,
            "FCD button activator did not decode as KEYINPUT NAND");

    const auto raw = gba::codebreaker::export_document(document, {});
    require(raw.success &&
            raw.text.find("D0000020 FFFE\n8200200A 0006") !=
                std::string::npos,
            "FCD button activator did not roundtrip in raw output");
}

void test_fcd_button_activator_encrypted_roundtrip() {
    const std::string input =
        "Button Activator:\n"
        "D0000020 FFFE\n"
        "82002000 1234\n";

    const auto document = gba::codebreaker::parse(input, {false});
    gba::codebreaker::ExportOptions options;
    options.encrypted = true;
    options.seed = gba::codebreaker::Seed{0x9ABCDEF0U, 0x1234U};
    const auto encrypted = gba::codebreaker::export_document(document, options);
    require(encrypted.success,
            "FCD button activator encrypted export failed");

    const auto decrypted = gba::codebreaker::parse(encrypted.text, {true});
    const auto raw = gba::codebreaker::export_document(decrypted, {});
    require(raw.text.find("D0000020 FFFE\n82002000 1234") !=
                std::string::npos,
            "FCD button activator did not survive encrypted roundtrip");
}

void test_fcd_button_activator_does_not_leak() {
    const std::string input =
        "Button Safety:\n"
        "D0000020 FFFE\n"
        "82002000 1234\n";

    const auto document = gba::codebreaker::parse(input, {false});

    const auto ez = gba::ezflash::export_document(document);
    require(!ez.success &&
            ez.text.find("ON=2000,34,12") == std::string::npos,
            "Unsupported FCD button activator leaked its write to EZ-Flash");

    const auto gs = gba::gameshark::export_document(document, {});
    require(!gs.success &&
            gs.text.find("10002000 00001234") == std::string::npos,
            "Unsupported FCD button activator leaked its write to GameShark");
    const std::string annotated = gba::inline_notes::apply(
        gs.text,
        document,
        gs.warnings,
        {gba::inline_notes::Style::Slash, true});
    require(annotated.find("// Conversion Note:") != std::string::npos &&
            annotated.find("no GameShark/AR GBX-compatible operations") ==
                std::string::npos,
            "Specific button note did not suppress redundant no-output note");

    const auto armax = gba::armax::export_document(document, {});
    require(!armax.success &&
            armax.text.find("02002000 00001234") == std::string::npos,
            "Unsupported FCD button activator leaked its write to AR MAX");
}

void test_fcd_conditioned_slide_roundtrip() {
    const std::string input =
        "Conditional Slide:\n"
        "72001000 1111\n"
        "42002000 1000\n"
        "00010004 0002\n";

    const auto document = gba::codebreaker::parse(input, {false});
    require(document.entries[0].operations.size() == 2U &&
            document.entries[0].operations[0].condition_span == 1U,
            "Conditional slide did not retain one logical controlled operation");

    const auto raw = gba::codebreaker::export_document(document, {});
    require(raw.success && raw.text.find(
                "72001000 1111\n"
                "42002000 1000\n"
                "00010004 0002") != std::string::npos,
            "Conditional slide did not roundtrip compactly");

    const auto ez = gba::ezflash::export_document(document);
    require(ez.text.find(
                "IF=1000,11,11;SLIDE=2000,00000004,00000002,00000001,00,10;") !=
                std::string::npos,
            "Conditional slide did not convert to Enhanced v3 SLIDE");
}

void test_fcd_conditioned_packed_list_scope() {
    const std::string input =
        "Conditional Packed:\n"
        "72001000 1111\n"
        "52002000 0005\n"
        "3412CDAB 010F\n"
        "68245713 0000\n";

    const auto document = gba::codebreaker::parse(input, {false});
    require(document.warnings.empty(),
            "Valid conditioned packed list produced parser warnings");
    require(document.entries.size() == 1U &&
            document.entries[0].operations.size() == 6U &&
            document.entries[0].operations[0].condition_span == 5U,
            "Packed-list condition scope was not expanded to all writes");

    const auto raw = gba::codebreaker::export_document(document, {});
    require(raw.success && raw.text.find(
                "72001000 1111\n"
                "52002000 0005\n"
                "3412CDAB 010F\n"
                "68245713 0000") != std::string::npos,
            "Conditioned packed list did not roundtrip compactly");

    const auto ez = gba::ezflash::export_document(document);
    require(ez.success && ez.text.find(
                "IF=1000,11,11;ON=2000,34,12,CD,AB,01,0F,68,24,57,13;") !=
                std::string::npos,
            "Conditioned packed list did not preserve its full scope in EZ");

    const auto gs = gba::gameshark::export_document(document, {});
    require(gs.success &&
            gs.text.find("E0051111 02001000") != std::string::npos &&
            gs.text.find("12002000 00001234") != std::string::npos &&
            gs.text.find("12002008 00001357") != std::string::npos,
            "Multi-write FCD condition did not map to GameShark multiline IF");

    gba::codebreaker::ExportOptions encrypted_options;
    encrypted_options.encrypted = true;
    encrypted_options.seed =
        gba::codebreaker::Seed{0x9ABCDEF0U, 0x1234U};
    const auto encrypted =
        gba::codebreaker::export_document(document, encrypted_options);
    require(encrypted.success,
            "Conditioned packed list encrypted export failed");
    const auto decoded = gba::codebreaker::parse(encrypted.text, {true});
    const auto raw_again = gba::codebreaker::export_document(decoded, {});
    require(raw_again.text.find(
                "72001000 1111\n"
                "52002000 0005\n"
                "3412CDAB 010F\n"
                "68245713 0000") != std::string::npos,
            "Conditioned packed list did not survive encrypted roundtrip");
}

void test_fcd_conditioned_short_packed_list_scope() {
    const std::string input =
        "Conditional Short Packed:\n"
        "72001000 1111\n"
        "52002000 0002\n"
        "3412CDAB 0000\n";

    const auto document = gba::codebreaker::parse(input, {false});
    require(document.entries[0].operations[0].condition_span == 2U,
            "Two-value packed-list condition span was not retained");
    const auto raw = gba::codebreaker::export_document(document, {});
    require(raw.success && raw.text.find(
                "72001000 1111\n"
                "52002000 0002\n"
                "3412CDAB 0000") != std::string::npos,
            "Short conditioned packed list was expanded and lost its scope");
}

void test_fcd_master_and_logical_roundtrip() {
    const std::string input =
        "Master And Logic:\n"
        "00001234 000A\n"
        "10012346 0007\n"
        "22002000 00F0\n"
        "62002002 0FF0\n"
        "E2002004 0001\n";

    const auto document = gba::codebreaker::parse(input, {false});
    require(document.warnings.empty() &&
            document.entries.size() == 1U &&
            document.entries[0].operations.size() == 5U,
            "Valid FCD master/logical rows did not parse cleanly");

    const auto& operations = document.entries[0].operations;
    require(operations[0].kind == gba::OperationKind::GameId &&
            operations[1].kind == gba::OperationKind::Hook &&
            operations[2].kind == gba::OperationKind::Or &&
            operations[3].kind == gba::OperationKind::And &&
            operations[4].kind == gba::OperationKind::Add,
            "FCD master/logical rows decoded to the wrong semantic kinds");

    const auto raw = gba::codebreaker::export_document(document, {});
    require(raw.success && raw.text.find(
                "00001234 000A\n"
                "10012346 0007\n"
                "22002000 00F0\n"
                "62002002 0FF0\n"
                "E2002004 0001") != std::string::npos,
            "FCD master/logical rows did not roundtrip exactly");

    gba::codebreaker::ExportOptions encrypted_options;
    encrypted_options.encrypted = true;
    encrypted_options.seed =
        gba::codebreaker::Seed{0x9ABCDEF0U, 0x1234U};
    const auto encrypted = gba::codebreaker::export_document(
        document, encrypted_options);
    require(encrypted.success,
            "FCD master/logical encrypted export failed");

    const auto decoded = gba::codebreaker::parse(encrypted.text, {true});
    const auto raw_again = gba::codebreaker::export_document(decoded, {});
    require(raw_again.text.find(
                "00001234 000A\n"
                "10012346 0007\n"
                "22002000 00F0\n"
                "62002002 0FF0\n"
                "E2002004 0001") != std::string::npos,
            "FCD master/logical rows did not survive encryption");

    const auto xploder = gba::xploder::export_document(document, {});
    require(xploder.success && xploder.text == raw.text,
            "Xploder did not inherit exact FCD master/logical output");
}

void test_fcd_hook_cross_family_conversion() {
    const std::string input =
        "Hook Conversion:\n"
        "10012346 0007\n"
        "82002000 1234\n";

    const auto document = gba::codebreaker::parse(input, {false});

    const auto gameshark = gba::gameshark::export_document(document, {});
    require(gameshark.success &&
            gameshark.text.find("F8012346 00000007") != std::string::npos &&
            gameshark.text.find("12002000 00001234") != std::string::npos,
            "FCD hook did not convert exactly to GameShark/AR GBX");

    const auto gameshark_document =
        gba::gameshark::parse(gameshark.text, {false});
    const auto back_from_gameshark =
        gba::codebreaker::export_document(gameshark_document, {});
    require(back_from_gameshark.success &&
            back_from_gameshark.text.find("10012346 0007") !=
                std::string::npos,
            "GameShark hook did not convert back to FCD");

    const auto armax = gba::armax::export_document(document, {});
    require(armax.success &&
            armax.text.find("C4012346 00000007") != std::string::npos,
            "FCD hook did not convert exactly to Action Replay MAX");

    const auto armax_document = gba::armax::parse(armax.text, {false});
    const auto back_from_armax =
        gba::codebreaker::export_document(armax_document, {});
    require(back_from_armax.success &&
            back_from_armax.text.find("10012346 0007") !=
                std::string::npos,
            "Action Replay MAX hook did not convert back to FCD");
}

void test_master_hook_safety_no_leak() {
    const std::string fcd_input =
        "Hook Safety:\n"
        "10012346 0007\n"
        "82002000 1234\n";
    const auto fcd_document = gba::codebreaker::parse(fcd_input, {false});

    const auto ez = gba::ezflash::export_document(fcd_document);
    require(!ez.success && ez.text.find("ON=") == std::string::npos,
            "FCD hook dependency leaked its write to EZ-Flash");

    const std::string wide_gameshark_hook =
        "Wide Hook:\n"
        "F8012346 12345678\n"
        "10002000 00001234\n";
    const auto gameshark_document =
        gba::gameshark::parse(wide_gameshark_hook, {false});
    const auto fcd =
        gba::codebreaker::export_document(gameshark_document, {});
    require(!fcd.success &&
            fcd.text.find("82002000 1234") == std::string::npos,
            "Unrepresentable GameShark hook leaked dependent writes to FCD");
    require(!fcd.warnings.empty() &&
            fcd.warnings.front().find("entire dependent entry was skipped") !=
                std::string::npos,
            "FCD hook safety did not explain the skipped dependent entry");

    const std::string odd_gameshark_hook =
        "Odd Hook:\n"
        "F8012347 00000007\n"
        "12002000 00001234\n";
    const auto odd_document =
        gba::gameshark::parse(odd_gameshark_hook, {false});
    const auto armax = gba::armax::export_document(odd_document, {});
    require(!armax.success && armax.text.empty(),
            "Odd GameShark hook leaked dependent writes to AR MAX");
}

void test_fcd_logical_cross_family_behavior() {
    const std::string input =
        "Logical Operations:\n"
        "22002000 00F0\n"
        "62002002 0FF0\n"
        "E2002004 0001\n";

    const auto document = gba::codebreaker::parse(input, {false});

    const auto armax = gba::armax::export_document(document, {});
    require(!armax.success &&
            armax.text.find("82202004 00000001") != std::string::npos,
            "FCD ADD did not map to Action Replay MAX while OR/AND were noted");

    const auto armax_document = gba::armax::parse(armax.text, {false});
    require(armax_document.entries.size() == 1U &&
            armax_document.entries[0].operations.size() == 1U &&
            armax_document.entries[0].operations[0].kind ==
                gba::OperationKind::Add,
            "Action Replay MAX ADD mapping did not decode semantically");

    const auto gameshark = gba::gameshark::export_document(document, {});
    require(!gameshark.success && gameshark.text.empty(),
            "FCD logical operations were incorrectly emitted as GameShark writes");

    const auto ez = gba::ezflash::export_document(document);
    require(!ez.success && ez.text.empty(),
            "FCD logical operations were incorrectly emitted as EZ writes");
}

void test_fcd_unsigned_comparison_to_armax() {
    const std::string input =
        "[Unsigned FCD]\n"
        "B2000010 8000\n"
        "32000020 0001\n"
        "C2000030 8000\n"
        "32000040 0002\n";

    const auto document = gba::codebreaker::parse(input, {false});
    const auto armax = gba::armax::export_document(document, {false});
    require(armax.success &&
            armax.text.find("32200010 00008000") != std::string::npos &&
            armax.text.find("2A200030 00008000") != std::string::npos,
            "FCD unsigned comparisons changed signedness in AR MAX output");
}

} // namespace gba::tests
