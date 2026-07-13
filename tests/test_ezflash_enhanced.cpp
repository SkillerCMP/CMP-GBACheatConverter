#include "test_support.hpp"

namespace gba::tests {

void test_ezflash_enhanced_v21_gameshark_romif_roundtrip() {
    const std::string input =
        "[Stealing Trainer's Pokemon]\n"
        "E00200FD 09014CD3\n"
        "020239F8 00000004\n"
        "60075060 00002000\n";

    const auto source = gba::gameshark::parse(input, {false});
    gba::ezflash::Options options;
    options.mode = gba::ezflash::Mode::Enhanced;
    const auto enhanced = gba::ezflash::export_document(source, options);
    require(enhanced.success && enhanced.text.find(
                "ROMIF=09014CD3,FD,00;ON=239F8,04;"
                "ROM=080EA0C0,00,20;") != std::string::npos,
            "GameShark guarded ROM patch did not become canonical Enhanced v3 ROMIF");

    const auto reparsed = gba::ezflash::parse(enhanced.text);
    const auto raw = gba::gameshark::export_document(reparsed, {false});
    require(raw.success && raw.text.find(
                "E00200FD 09014CD3\n"
                "020239F8 00000004\n"
                "60075060 00002000") != std::string::npos,
            "Enhanced v3 ROMIF did not round-trip to GameShark mode-0 ROM patch");
}

void test_ezflash_enhanced_v21_armax_if_rom_tail() {
    const std::string encrypted =
        "[AR MAX Mixed]\n"
        "B6C5368A 08BE8FF4\n"
        "6FD608D0 B9151D51\n"
        "084197CA 3EA6DE4F\n"
        "8E883EFF 92E9660D\n";

    const auto source = gba::armax::parse(encrypted, {true});
    gba::ezflash::Options options;
    options.mode = gba::ezflash::Mode::Enhanced;
    const auto enhanced = gba::ezflash::export_document(source, options);
    require(enhanced.success && enhanced.text.find(
                "IF=80130,FF,00;ON=239F8,04;"
                "ROM=0803A830,00,20;") != std::string::npos,
            "AR MAX condition-next-write plus ROM patch did not become Enhanced v3 IF/ON/ROM");

    const auto reparsed = gba::ezflash::parse(enhanced.text);
    require(reparsed.entries.size() == 1U &&
            reparsed.entries[0].operations.size() == 3U &&
            reparsed.entries[0].operations[0].kind ==
                gba::OperationKind::IfEqual &&
            reparsed.entries[0].operations[0].condition_span == 1U &&
            reparsed.entries[0].operations[1].kind ==
                gba::OperationKind::Write &&
            reparsed.entries[0].operations[2].kind ==
                gba::OperationKind::RomPatch,
            "Enhanced IF/ON/ROM parser placed the unconditional ROM tail inside the runtime IF");

    const auto raw = gba::armax::export_document(reparsed, {false});
    require(raw.success && raw.text.find(
                "0A400130 000000FF\n"
                "002239F8 00000004\n"
                "00000000 1801D418\n"
                "00002000 00000000") != std::string::npos,
            "Enhanced v3 IF/ON/ROM did not round-trip to the original AR MAX rows");

    const auto encrypted_roundtrip =
        gba::armax::export_document(reparsed, {true});
    const auto decrypted_roundtrip =
        gba::armax::parse(encrypted_roundtrip.text, {true});
    const auto decrypted_raw =
        gba::armax::export_document(decrypted_roundtrip, {false});
    require(encrypted_roundtrip.success && decrypted_raw.success &&
            decrypted_raw.text.find(
                "0A400130 000000FF\n"
                "002239F8 00000004\n"
                "00000000 1801D418\n"
                "00002000 00000000") != std::string::npos,
            "Enhanced v3 IF/ON/ROM did not round-trip through encrypted AR MAX");
}

void test_ezflash_enhanced_v21_long_rom_byte_lists() {
    const std::string input =
        "[Long ROM Patch]\n"
        "ROM=08010000,01,02,03,04,05,06,07,08;\n";
    const auto parsed = gba::ezflash::parse(input);
    require(parsed.entries.size() == 1U &&
            parsed.entries[0].operations.size() == 2U &&
            std::all_of(parsed.entries[0].operations.begin(),
                        parsed.entries[0].operations.end(),
                        [](const gba::Operation& operation) {
                            return operation.kind ==
                                gba::OperationKind::RomPatch;
                        }),
            "Enhanced ROM= did not preserve an eight-byte patch");

    gba::ezflash::Options options;
    options.mode = gba::ezflash::Mode::Enhanced;
    const auto exported = gba::ezflash::export_document(parsed, options);
    require(exported.success && exported.text.find(
                "ROM=08010000,01,02,03,04,05,06,07,08;") !=
                std::string::npos,
            "Enhanced ROM= byte list over 32 bits did not round-trip");

    const auto detected = gba::detect::format(input);
    require(detected.format == gba::detect::Format::EzFlash &&
            detected.confidence == gba::detect::Confidence::High,
            "Auto Detect did not recognize Enhanced ROM= syntax");
}

void test_ezflash_enhanced_v3_condition_families_and_else() {
    const std::string input =
        "[V3 Conditions]\n"
        "IFNE=202,01,00;ON=10A78,CC;ELSE;ON:10A78,64;ENDIF;\n"
        "[V3 Ordered]\n"
        "IFLT=204,01,00;ON=10A80,01;"
        "IFGT=206,02,00;ON=10A81,02;"
        "IFLE=208,03,00;ON=10A82,03;"
        "IFGE=20A,04,00;ON=10A83,04;\n";

    const auto detected = gba::detect::format(input);
    require(detected.format == gba::detect::Format::EzFlash &&
            detected.confidence == gba::detect::Confidence::High,
            "Auto Detect did not recognize Enhanced v3 condition syntax");

    const auto document = gba::ezflash::parse(input);
    require(document.warnings.empty() && document.entries.size() == 2U,
            "Enhanced v3 condition text did not parse cleanly");
    const auto& first = document.entries[0].operations;
    require(first.size() == 3U &&
            first[0].kind == gba::OperationKind::IfNotEqual &&
            first[0].condition_has_else &&
            first[0].condition_span == 1U &&
            first[0].condition_else_span == 1U,
            "Enhanced v3 IFNE/ELSE metadata was not retained");
    const auto& ordered = document.entries[1].operations;
    require(ordered.size() == 8U &&
            ordered[0].kind == gba::OperationKind::IfLess &&
            ordered[2].kind == gba::OperationKind::IfGreater &&
            ordered[4].kind == gba::OperationKind::IfLessOrEqual &&
            ordered[6].kind == gba::OperationKind::IfGreaterOrEqual,
            "Enhanced v3 ordered conditions decoded incorrectly");

    const auto output = gba::ezflash::export_document(document);
    require(output.success &&
            output.text.find(
                "IFNE=202,01,00;ON=10A78,CC;ELSE;ON:10A78,64;ENDIF;") !=
                std::string::npos &&
            output.text.find("IFLT=204,01,00;ON=10A80,01;") !=
                std::string::npos &&
            output.text.find("IFGT=206,02,00;ON=10A81,02;") !=
                std::string::npos &&
            output.text.find("IFLE=208,03,00;ON=10A82,03;") !=
                std::string::npos &&
            output.text.find("IFGE=20A,04,00;ON=10A83,04;") !=
                std::string::npos,
            "Enhanced v3 condition families did not round-trip");
}

void test_ezflash_enhanced_v3_arithmetic_pointer_fill_slide() {
    const std::string input =
        "[Wide Math]\n"
        "ADD=10A78,01,02,03,04,05,06;SUB:10A80,06,05,04,03,02,01;\n"
        "[Pointer]\n"
        "PTR=10,00000123,7F;\n"
        "[Fill]\n"
        "FILL=10000,00000003,34,12;\n"
        "[Slide]\n"
        "SLIDE=10020,00000004,00000002,FFFFFFFF,34,12;\n";

    const auto document = gba::ezflash::parse(input);
    require(document.warnings.empty() && document.entries.size() == 4U,
            "Enhanced v3 operation text did not parse cleanly");
    const auto& math = document.entries[0].operations;
    require(math.size() == 2U &&
            math[0].kind == gba::OperationKind::Add &&
            math[0].byte_payload.size() == 6U && math[0].width == 0U &&
            math[1].kind == gba::OperationKind::Subtract &&
            math[1].byte_payload.size() == 6U,
            "Enhanced v3 arbitrary-byte arithmetic was not retained");
    const auto& pointer = document.entries[1].operations[0];
    require(pointer.kind == gba::OperationKind::PointerWrite &&
            pointer.pointer_offset == 0x123U &&
            pointer.byte_payload == std::vector<std::uint8_t>{0x7FU},
            "Enhanced v3 PTR decoded incorrectly");
    const auto& fill = document.entries[2].operations[0];
    require(fill.kind == gba::OperationKind::Write && fill.repeat == 3U &&
            fill.address_step == 2 && fill.value_step == 0 &&
            fill.byte_payload ==
                std::vector<std::uint8_t>({0x34U, 0x12U}),
            "Enhanced v3 FILL decoded incorrectly");
    const auto& slide = document.entries[3].operations[0];
    require(slide.kind == gba::OperationKind::Write && slide.repeat == 4U &&
            slide.address_step == 2 && slide.value_step == -1 &&
            slide.byte_payload ==
                std::vector<std::uint8_t>({0x34U, 0x12U}),
            "Enhanced v3 SLIDE decoded incorrectly");

    const auto output = gba::ezflash::export_document(document);
    require(output.success &&
            output.text.find("ADD=10A78,01,02,03,04,05,06;") !=
                std::string::npos &&
            output.text.find("SUB:10A80,06,05,04,03,02,01;") !=
                std::string::npos &&
            output.text.find("PTR=10,00000123,7F;") !=
                std::string::npos &&
            output.text.find("FILL=10000,00000003,34,12;") !=
                std::string::npos &&
            output.text.find(
                "SLIDE=10020,00000004,00000002,FFFFFFFF,34,12;") !=
                std::string::npos,
            "Enhanced v3 arithmetic/PTR/FILL/SLIDE did not round-trip");
}

void test_ezflash_enhanced_v3_romif_with_named_runtime_action() {
    const std::string input =
        "[Guarded Arithmetic]\n"
        "ROMIF=09014CD3,FD,00;ADD=239F8,01;ROM=080EA0C0,00,20;\n";
    const auto document = gba::ezflash::parse(input);
    require(document.warnings.empty() && document.entries.size() == 1U &&
            document.entries[0].operations.size() == 3U &&
            document.entries[0].operations[0].kind ==
                gba::OperationKind::IfEqual &&
            document.entries[0].operations[1].kind ==
                gba::OperationKind::Add &&
            document.entries[0].operations[2].kind ==
                gba::OperationKind::RomPatch,
            "Enhanced v3 ROMIF named action did not parse");
    const auto output = gba::ezflash::export_document(document);
    require(output.success && output.text.find(
                "ROMIF=09014CD3,FD,00;ADD=239F8,01;"
                "ROM=080EA0C0,00,20;") != std::string::npos,
            "Enhanced v3 ROMIF named action did not round-trip");
}

} // namespace gba::tests
