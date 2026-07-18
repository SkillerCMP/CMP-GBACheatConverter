#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gba::output_modes {

enum class Format {
    ArmaxDsc,
    VisualBoyAdvanceClt,
    MyBoyCht,
    MisterGg,
    MisterZip,
    MednafenCht,
    MgbaCheats,
    LibretroCht,
    EzFlashCht
};

enum class EzFlashMode {
    Original,
    Enhanced
};

struct Options {
    // Used by AR MAX .dsc and Mednafen headers.
    std::string game_name;
    // Required by Mednafen. Lowercase hexadecimal MD5 is preferred.
    std::string rom_md5;
    // Selects the kernel-compatible syntax for EZ-Flash .cht output.
    EzFlashMode ezflash_mode = EzFlashMode::Enhanced;
};

struct Result {
    std::vector<std::uint8_t> data;
    std::vector<std::string> warnings;
    bool success = true;
    std::size_t exported_entries = 0;
    std::size_t exported_records = 0;
};

Result export_document(const CheatDocument& document,
                       Format format,
                       const Options& options = {});

} // namespace gba::output_modes
