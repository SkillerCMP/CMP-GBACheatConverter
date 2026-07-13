#include "test_support.hpp"

namespace gba::tests {

void test_native_output_modes() {
    gba::CheatDocument document;

    gba::CheatEntry direct;
    direct.name = "Direct Writes";
    for (const auto& values : std::vector<std::tuple<std::uint32_t,
                                                     std::uint32_t,
                                                     std::uint8_t>>{
             {0x02001000U, 0x12U, std::uint8_t{1}},
             {0x02001002U, 0x3456U, std::uint8_t{2}},
             {0x02001004U, 0x89ABCDEFU, std::uint8_t{4}}}) {
        gba::Operation operation;
        operation.kind = gba::OperationKind::Write;
        operation.address = std::get<0>(values);
        operation.value = std::get<1>(values);
        operation.width = std::get<2>(values);
        direct.operations.push_back(operation);
    }
    document.entries.push_back(direct);

    gba::CheatEntry conditional;
    conditional.name = "Conditional Write";
    gba::Operation condition;
    condition.kind = gba::OperationKind::IfEqual;
    condition.address = 0x02002000U;
    condition.value = 1U;
    condition.width = 1U;
    condition.condition_span = 1U;
    conditional.operations.push_back(condition);
    gba::Operation controlled;
    controlled.kind = gba::OperationKind::Write;
    controlled.address = 0x02002001U;
    controlled.value = 0x77U;
    controlled.width = 1U;
    conditional.operations.push_back(controlled);
    document.entries.push_back(conditional);

    const auto ar_only = gba::armax::parse(
        "[AR Patch]\n"
        "00000000 18000010\n"
        "00001234 DEADBEEF\n", {false});
    require(ar_only.entries.size() == 1U && ar_only.warnings.empty(),
            "AR-only native test entry did not parse");
    document.entries.push_back(ar_only.entries.front());

    const auto as_string = [](const gba::output_modes::Result& result) {
        return std::string(result.data.begin(), result.data.end());
    };

    {
        gba::output_modes::Options options;
        options.ezflash_mode = gba::output_modes::EzFlashMode::Original;
        const auto result = gba::output_modes::export_document(
            document, gba::output_modes::Format::EzFlashCht, options);
        const std::string text = as_string(result);
        require(result.success && result.exported_entries == 1U,
                "EZ-Flash Original native export failed");
        require(text.find("ON=") != std::string::npos &&
                text.find("IF=") == std::string::npos,
                "EZ-Flash Original native output is not ON=-only");
        require(text.find("[Direct Writes]") != std::string::npos,
                "EZ-Flash Original native output lost the cheat name");
        const auto reparsed = gba::ezflash::parse(text);
        require(std::none_of(
                    reparsed.entries.begin(), reparsed.entries.end(),
                    [](const gba::CheatEntry& entry) {
                        return std::any_of(
                            entry.operations.begin(), entry.operations.end(),
                            [](const gba::Operation& operation) {
                                return operation.address == 0x02002001U;
                            });
                    }),
                "EZ-Flash Original native output leaked a conditioned write");
        require(!result.warnings.empty(),
                "EZ-Flash Original native output did not warn about omissions");
    }
    {
        gba::output_modes::Options options;
        options.ezflash_mode = gba::output_modes::EzFlashMode::Enhanced;
        const auto result = gba::output_modes::export_document(
            document, gba::output_modes::Format::EzFlashCht, options);
        const std::string text = as_string(result);
        require(result.success && result.exported_entries == 3U,
                "EZ-Flash Enhanced native export failed");
        require(text.find("ON=") != std::string::npos &&
                text.find("IF=") != std::string::npos &&
                text.find("ROM=") != std::string::npos,
                "EZ-Flash Enhanced native output lost IF or ROM operations");
        require(text.find("[Direct Writes]") != std::string::npos &&
                text.find("[Conditional Write]") != std::string::npos &&
                text.find("[AR Patch]") != std::string::npos,
                "EZ-Flash Enhanced native output lost cheat names");
        const auto reparsed = gba::ezflash::parse(text);
        require(std::any_of(
                    reparsed.entries.begin(), reparsed.entries.end(),
                    [](const gba::CheatEntry& entry) {
                        return std::any_of(
                            entry.operations.begin(), entry.operations.end(),
                            [](const gba::Operation& operation) {
                                return operation.kind ==
                                    gba::OperationKind::IfEqual;
                            });
                    }),
                "EZ-Flash Enhanced native output did not reparse its IF group");
    }

    {
        const auto result = gba::output_modes::export_document(
            document, gba::output_modes::Format::MyBoyCht);
        const std::string text = as_string(result);
        require(result.success && result.exported_entries == 3U,
                "My Boy native export failed");
        require(text.find("<cheats>") != std::string::npos &&
                text.find("type=\"cb\"") != std::string::npos &&
                text.find("type=\"gs3\"") != std::string::npos,
                "My Boy mixed native XML structure is missing");
    }
    {
        const auto result = gba::output_modes::export_document(
            document, gba::output_modes::Format::MgbaCheats);
        const std::string text = as_string(result);
        require(result.success && text.find("!disabled") != std::string::npos &&
                text.find("!PARv3") != std::string::npos,
                "mGBA mixed native export failed");
    }
    {
        const auto result = gba::output_modes::export_document(
            document, gba::output_modes::Format::LibretroCht);
        const std::string text = as_string(result);
        require(result.success &&
                text.find("cheats = 3") != std::string::npos &&
                text.find("cheat0_enable = false") != std::string::npos,
                "Libretro native export failed");
    }
    {
        gba::output_modes::Options options;
        options.game_name = "Test Game";
        options.rom_md5 = "0123456789abcdef0123456789abcdef";
        const auto result = gba::output_modes::export_document(
            document, gba::output_modes::Format::MednafenCht, options);
        const std::string text = as_string(result);
        require(result.success && result.exported_entries == 1U,
                "Mednafen direct-write export failed");
        require(text.find("[0123456789abcdef0123456789abcdef] Test Game") !=
                    std::string::npos,
                "Mednafen header is incorrect");
        require(text.find("Conditional Write") == std::string::npos,
                "Mednafen leaked a conditioned write");
        require(!result.warnings.empty(),
                "Mednafen did not warn about omitted conditions");
    }
    {
        const auto result = gba::output_modes::export_document(
            document, gba::output_modes::Format::VisualBoyAdvanceClt);
        require(result.success && result.data.size() == 8012U,
                "VisualBoy Advance .clt size is incorrect");
        const auto read_u32 = [&](std::size_t offset) {
            return static_cast<std::uint32_t>(result.data[offset]) |
                   (static_cast<std::uint32_t>(result.data[offset + 1U]) << 8U) |
                   (static_cast<std::uint32_t>(result.data[offset + 2U]) << 16U) |
                   (static_cast<std::uint32_t>(result.data[offset + 3U]) << 24U);
        };
        require(read_u32(0U) == 1U && read_u32(4U) == 1U &&
                read_u32(8U) == 3U,
                "VisualBoy Advance .clt header is incorrect");
        require(!result.warnings.empty(),
                "VisualBoy Advance did not warn about omitted conditions");
    }
    {
        gba::output_modes::Options options;
        options.game_name = "Test Game";
        const auto result = gba::output_modes::export_document(
            document, gba::output_modes::Format::ArmaxDsc, options);
        require(result.success && result.data.size() > 0x178U,
                "AR MAX .dsc export failed");
        require(std::equal(result.data.begin(), result.data.begin() + 16,
                           std::string("ARDS000000000001").begin()),
                "AR MAX .dsc signature is incorrect");
    }
    {
        const auto result = gba::output_modes::export_document(
            document, gba::output_modes::Format::MisterZip);
        require(result.success && result.exported_entries == 2U,
                "MiSTer export failed");
        require(result.data.size() > 4U && result.data[0] == 'P' &&
                result.data[1] == 'K' && result.data[2] == 3U &&
                result.data[3] == 4U,
                "MiSTer output is not a ZIP archive");
        const std::string bytes = as_string(result);
        require(bytes.find("Direct Writes.gg") != std::string::npos &&
                bytes.find("Conditional Write.gg") != std::string::npos,
                "MiSTer ZIP is missing cheat files");
    }
}

} // namespace gba::tests
