#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gba::native_input {

enum class SourceFormat {
    Unknown,
    ArmaxDsc,
    VisualBoyAdvanceClt,
    MyBoyCht,
    MisterZip,
    MednafenCht,
    MgbaCheats,
    LibretroCht,
    EzFlashCht
};

enum class InputFormat {
    FcdRaw,
    FcdEncrypted,
    GameSharkRaw,
    GameSharkEncrypted,
    ActionReplayMaxRaw,
    ActionReplayMaxEncrypted,
    EzFlash
};

struct Result {
    bool recognized = false;
    bool success = false;
    SourceFormat source_format = SourceFormat::Unknown;
    InputFormat input_format = InputFormat::FcdRaw;
    std::string source_name;
    std::string text;
    std::vector<std::string> warnings;
};

// Imports one of the native files produced by File > Save Output As.
// Content signatures are authoritative; filename is used only to resolve
// text formats that intentionally share an extension such as .cht.
Result import_file(std::string_view filename,
                   const std::vector<std::uint8_t>& data);

std::string_view source_format_name(SourceFormat format);
std::string_view input_format_name(InputFormat format);

} // namespace gba::native_input
