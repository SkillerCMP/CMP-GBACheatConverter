#include "core/detect.hpp"

namespace gba::detect {

std::string name(Format format) {
    switch (format) {
    case Format::FcdRaw:
        return "RAW - CodeBreaker / GameShark SP / Xploder Advance";
    case Format::FcdEncrypted:
        return "Encrypted - CodeBreaker / GameShark SP / Xploder Advance";
    case Format::GameSharkRaw:
        return "RAW - GameShark Advance / Action Replay GBX";
    case Format::GameSharkEncrypted:
        return "Encrypted - GameShark Advance / Action Replay GBX";
    case Format::ActionReplayMaxRaw:
        return "RAW - Action Replay MAX";
    case Format::ActionReplayMaxEncrypted:
        return "Encrypted - Action Replay MAX";
    case Format::EzFlash:
        return "RAW - EZ-Flash";
    case Format::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

std::string confidence_name(Confidence confidence) {
    switch (confidence) {
    case Confidence::High:
        return "High";
    case Confidence::Medium:
        return "Medium";
    case Confidence::Low:
        return "Low";
    }
    return "Low";
}

} // namespace gba::detect
