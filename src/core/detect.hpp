#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace gba::detect {

enum class Format {
    Unknown,
    FcdRaw,
    FcdEncrypted,
    GameSharkRaw,
    GameSharkEncrypted,
    ActionReplayMaxRaw,
    ActionReplayMaxEncrypted,
    EzFlash
};

enum class Confidence {
    Low,
    Medium,
    High
};

struct Result {
    Format format = Format::Unknown;
    Confidence confidence = Confidence::Low;
    int score = 0;
    int runner_up_score = 0;
    std::vector<std::string> reasons;
};

Result format(std::string_view input);
std::string name(Format format);
std::string confidence_name(Confidence confidence);

} // namespace gba::detect
