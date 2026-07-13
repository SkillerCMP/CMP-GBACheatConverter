#include "import/native_input.hpp"

#include "core/text.hpp"
#include "import/native_input_internal.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace gba::native_input {

Result import_file(std::string_view filename,
                   const std::vector<std::uint8_t>& data) {
    const std::string extension = detail::filename_extension(filename);

    if (detail::equals_ascii(data, 0U, "ARDS000000000001")) {
        return detail::import_armax_dsc(filename, data);
    }
    if (const auto signature = detail::read_u32(data, 0U);
        signature && *signature == 0x04034B50U) {
        return detail::import_mister(filename, data);
    }
    if (const auto version = detail::read_u32(data, 0U);
        version && *version == 1U) {
        if (const auto list_type = detail::read_u32(data, 4U);
            list_type && *list_type == 1U) {
            return detail::import_vba_clt(filename, data);
        }
    }

    if (extension == ".dsc") {
        return detail::recognized_error(
            SourceFormat::ArmaxDsc, "Action Replay MAX .dsc",
            "The selected .dsc file does not have the ARDS000000000001 "
            "signature.");
    }
    if (extension == ".clt") {
        return detail::recognized_error(
            SourceFormat::VisualBoyAdvanceClt,
            "VisualBoy Advance .clt",
            "The selected .clt file does not have a supported VBA header.");
    }
    if (extension == ".zip") {
        return detail::recognized_error(
            SourceFormat::MisterZip, "MiSTer .zip",
            "The selected .zip file does not have a supported ZIP header.");
    }
    if (detail::has_nul_byte(data)) {
        return {};
    }

    const std::string text_value = text::strip_utf8_bom(
        detail::bytes_as_text(data));
    if (detail::looks_like_myboy(text_value)) {
        return detail::import_myboy(filename, text_value);
    }
    if (detail::looks_like_mgba(text_value)) {
        return detail::import_mgba(filename, text_value);
    }
    if (detail::looks_like_libretro(text_value)) {
        return detail::import_libretro(filename, text_value);
    }
    if (detail::looks_like_mednafen(text_value)) {
        return detail::import_mednafen(filename, text_value);
    }
    if (detail::looks_like_ezflash(text_value)) {
        return detail::import_ezflash(filename, text_value);
    }

    if (extension == ".cheats") {
        return detail::recognized_error(
            SourceFormat::MgbaCheats, "mGBA .cheats",
            "The selected .cheats file does not contain supported mGBA "
            "cheat blocks.");
    }
    return {};
}

std::string_view source_format_name(SourceFormat format) {
    switch (format) {
    case SourceFormat::Unknown: return "Unknown";
    case SourceFormat::ArmaxDsc: return "Action Replay MAX .dsc";
    case SourceFormat::VisualBoyAdvanceClt:
        return "VisualBoy Advance .clt";
    case SourceFormat::MyBoyCht: return "My Boy! .cht";
    case SourceFormat::MisterZip: return "MiSTer .zip";
    case SourceFormat::MednafenCht: return "Mednafen .cht";
    case SourceFormat::MgbaCheats: return "mGBA .cheats";
    case SourceFormat::LibretroCht: return "Libretro / RetroArch .cht";
    case SourceFormat::EzFlashCht: return "EZ-Flash .cht";
    }
    return "Unknown";
}

std::string_view input_format_name(InputFormat format) {
    switch (format) {
    case InputFormat::FcdRaw:
        return "CodeBreaker / GameShark SP / Xploder Advance Raw";
    case InputFormat::FcdEncrypted:
        return "CodeBreaker / GameShark SP / Xploder Advance Encrypted";
    case InputFormat::GameSharkRaw:
        return "GameShark Advance / Action Replay GBX Raw";
    case InputFormat::GameSharkEncrypted:
        return "GameShark Advance / Action Replay GBX Encrypted";
    case InputFormat::ActionReplayMaxRaw:
        return "Action Replay MAX Raw";
    case InputFormat::ActionReplayMaxEncrypted:
        return "Action Replay MAX Encrypted";
    case InputFormat::EzFlash:
        return "EZ-Flash";
    }
    return "Unknown";
}

} // namespace gba::native_input
