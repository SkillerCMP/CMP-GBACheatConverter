#include "import/native_input.hpp"

#include "core/text.hpp"
#include "formats/vba_clt_codec.hpp"
#include "import/native_input_internal.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::native_input {
namespace {

struct Candidate {
    Result result;
    int score = 0;
};

DetectionConfidence confidence_from_score(int score) {
    if (score >= 100) return DetectionConfidence::Exact;
    if (score >= 95) return DetectionConfidence::High;
    if (score >= 85) return DetectionConfidence::Medium;
    if (score > 0) return DetectionConfidence::Low;
    return DetectionConfidence::None;
}

void set_confidence(Result& result, DetectionConfidence confidence) {
    result.detection_confidence = confidence;
}

void add_candidate(std::vector<Candidate>& candidates,
                   Result result,
                   int score) {
    if (!result.recognized) return;
    result.detection_confidence = confidence_from_score(score);
    candidates.push_back({std::move(result), score});
}

Result choose_text_candidate(std::string_view filename,
                             std::string_view extension,
                             std::string_view text_value) {
    std::vector<Candidate> candidates;

    add_candidate(candidates, detail::import_myboy(filename, text_value),
                  extension == ".cht" ? 100 : 96);
    add_candidate(candidates, detail::import_mgba(filename, text_value),
                  extension == ".cheats" ? 100 : 94);
    add_candidate(candidates, detail::import_libretro(filename, text_value),
                  extension == ".cht" ? 98 : 95);
    add_candidate(candidates, detail::import_mednafen(filename, text_value),
                  extension == ".cht" ? 98 : 95);
    add_candidate(candidates, detail::import_ezflash(filename, text_value),
                  extension == ".cht" ? 92 : 86);

    if (candidates.empty()) return {};

    std::stable_sort(candidates.begin(), candidates.end(),
        [](const Candidate& left, const Candidate& right) {
            if (left.score != right.score) return left.score > right.score;
            // A structurally valid parse wins over a recognized malformed
            // candidate at the same confidence.
            return left.result.success && !right.result.success;
        });

    const Candidate& best = candidates.front();
    if (candidates.size() > 1U &&
        candidates[1U].score == best.score &&
        candidates[1U].result.source_format != best.result.source_format) {
        Result ambiguous;
        ambiguous.recognized = true;
        ambiguous.success = false;
        ambiguous.source_format = SourceFormat::Unknown;
        ambiguous.source_name = "Ambiguous native cheat file";
        ambiguous.detection_confidence = confidence_from_score(best.score);
        ambiguous.competing_formats.push_back(best.result.source_format);
        ambiguous.competing_formats.push_back(
            candidates[1U].result.source_format);
        ambiguous.warnings.push_back(
            "The file matches both " +
            std::string(source_format_name(best.result.source_format)) +
            " and " +
            std::string(source_format_name(
                candidates[1U].result.source_format)) +
            " at the same confidence. Select an explicit --from format.");
        return ambiguous;
    }

    Result selected = best.result;
    for (std::size_t index = 1U; index < candidates.size(); ++index) {
        const SourceFormat competing =
            candidates[index].result.source_format;
        if (competing == selected.source_format) continue;
        selected.competing_formats.push_back(competing);
        selected.warnings.push_back(
            "Detection note: the file also matched " +
            std::string(source_format_name(competing)) + " at " +
            std::string(confidence_name(
                candidates[index].result.detection_confidence)) +
            " confidence; selected " +
            std::string(source_format_name(selected.source_format)) + ".");
    }
    return selected;
}

} // namespace

Result import_file(std::string_view filename,
                   const std::vector<std::uint8_t>& data) {
    const std::string extension = detail::filename_extension(filename);

    if (detail::equals_ascii(data, 0U, "ARDS000000000001")) {
        Result result = detail::import_armax_dsc(filename, data);
        set_confidence(result, DetectionConfidence::Exact);
        return result;
    }
    if (extension == ".gg") {
        Result result = detail::import_mister(filename, data);
        set_confidence(result, DetectionConfidence::Exact);
        return result;
    }
    if (const auto signature = detail::read_u32(data, 0U);
        signature && (*signature == 0x04034B50U ||
                      *signature == 0x02014B50U)) {
        Result mister = detail::import_mister(filename, data);
        if (mister.recognized) {
            set_confidence(mister, DetectionConfidence::Exact);
            return mister;
        }
    }
    if (vba_clt::has_supported_header(data)) {
        Result result = detail::import_vba_clt(filename, data);
        set_confidence(result, DetectionConfidence::Exact);
        return result;
    }

    if (extension == ".dsc") {
        Result result = detail::recognized_error(
            SourceFormat::ArmaxDsc, "Action Replay MAX .dsc",
            "The selected .dsc file does not have the ARDS000000000001 "
            "signature.");
        set_confidence(result, DetectionConfidence::Exact);
        return result;
    }
    if (extension == ".clt") {
        Result result = detail::recognized_error(
            SourceFormat::VisualBoyAdvanceClt,
            "VisualBoy Advance-M .clt",
            "The selected .clt file does not have a supported VBA-M header.");
        set_confidence(result, DetectionConfidence::Exact);
        return result;
    }
    if (detail::has_nul_byte(data)) {
        return {};
    }

    const std::string text_value = text::strip_utf8_bom(
        detail::bytes_as_text(data));
    Result selected = choose_text_candidate(
        filename, extension, text_value);
    if (selected.recognized) return selected;

    if (extension == ".cheats") {
        Result result = detail::recognized_error(
            SourceFormat::MgbaCheats, "mGBA .cheats",
            "The selected .cheats file does not contain supported mGBA "
            "cheat blocks.");
        set_confidence(result, DetectionConfidence::Exact);
        return result;
    }
    return {};
}

std::string_view source_format_name(SourceFormat format) {
    switch (format) {
    case SourceFormat::Unknown: return "Unknown";
    case SourceFormat::ArmaxDsc: return "Action Replay MAX .dsc";
    case SourceFormat::VisualBoyAdvanceClt:
        return "VisualBoy Advance-M .clt";
    case SourceFormat::MyBoyCht: return "My Boy! .cht";
    case SourceFormat::MisterZip: return "MiSTer GBA .gg / .zip";
    case SourceFormat::MednafenCht: return "Mednafen .cht";
    case SourceFormat::MgbaCheats: return "mGBA .cheats";
    case SourceFormat::LibretroCht: return "Libretro / RetroArch .cht";
    case SourceFormat::EzFlashCht: return "EZ-Flash .cht";
    }
    return "Unknown";
}

std::string_view source_format_cli_name(SourceFormat format) {
    switch (format) {
    case SourceFormat::Unknown: return "unknown";
    case SourceFormat::ArmaxDsc: return "armax-dsc";
    case SourceFormat::VisualBoyAdvanceClt: return "vba-clt";
    case SourceFormat::MyBoyCht: return "myboy-cht";
    case SourceFormat::MisterZip: return "mister";
    case SourceFormat::MednafenCht: return "mednafen-cht";
    case SourceFormat::MgbaCheats: return "mgba-cheats";
    case SourceFormat::LibretroCht: return "retroarch-cht";
    case SourceFormat::EzFlashCht: return "ezflash-cht";
    }
    return "unknown";
}

std::string_view confidence_name(DetectionConfidence confidence) {
    switch (confidence) {
    case DetectionConfidence::None: return "none";
    case DetectionConfidence::Low: return "low";
    case DetectionConfidence::Medium: return "medium";
    case DetectionConfidence::High: return "high";
    case DetectionConfidence::Exact: return "exact";
    }
    return "none";
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
