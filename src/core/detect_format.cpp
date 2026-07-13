#include "core/detect.hpp"

#include "core/detect_internal.hpp"
#include "core/text.hpp"
#include "crypto/tea.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace gba::detect {

using internal::Candidate;
using internal::Line88;
using internal::decrypt_armax_rows;
using internal::decrypt_rows;
using internal::parse_8x8;
using internal::score_armax;
using internal::score_gameshark;
using internal::starts_with;

Result format(std::string_view input) {
    Result result;
    const auto lines = text::split_lines(input);

    std::vector<std::string> nonempty;
    std::vector<Line88> rows88;
    std::vector<std::string> rows84;
    int ez_rows = 0;

    for (const std::string& raw : lines) {
        const std::string line = text::trim(raw);
        if (line.empty()) {
            continue;
        }
        nonempty.push_back(line);
        if (text::is_code_line_8x4(line)) {
            rows84.push_back(line);
        } else if (const auto parsed = parse_8x8(line)) {
            rows88.push_back(*parsed);
        }

        if (starts_with(line, "ON=") || starts_with(line, "IF=") ||
            starts_with(line, "IFNE=") || starts_with(line, "IFLT=") ||
            starts_with(line, "IFGT=") || starts_with(line, "IFLE=") ||
            starts_with(line, "IFGE=") || starts_with(line, "ADD=") ||
            starts_with(line, "SUB=") || starts_with(line, "PTR=") ||
            starts_with(line, "FILL=") || starts_with(line, "SLIDE=") ||
            starts_with(line, "ROM=") || starts_with(line, "ROMIF=")) {
            ++ez_rows;
        }
    }

    if (nonempty.empty()) {
        result.reasons.push_back("input is empty");
        return result;
    }

    if (ez_rows > 0) {
        result.format = Format::EzFlash;
        result.confidence = Confidence::High;
        result.score = ez_rows * 20;
        result.reasons.push_back("found EZ-Flash Original/Enhanced operation syntax");
        return result;
    }

    if (!rows84.empty() && !rows88.empty()) {
        result.reasons.push_back("input mixes 8+4 and 8+8 code rows");
        return result;
    }

    if (!rows84.empty()) {
        const auto first = text::parse_hex_u32(rows84.front().substr(0, 8));
        const bool seed = first && ((*first >> 28U) == 0x9U);
        result.format = seed ? Format::FcdEncrypted : Format::FcdRaw;
        result.confidence = rows84.size() >= 2U
            ? Confidence::High
            : Confidence::Medium;
        result.score = static_cast<int>(rows84.size()) * 12 + (seed ? 8 : 0);
        result.reasons.push_back("found Future Console Design 8+4 code rows");
        if (seed) {
            result.reasons.push_back("first code row is a plaintext 9-type cipher seed");
        }
        return result;
    }

    if (rows88.empty()) {
        result.reasons.push_back("no supported cheat-code row shape was found");
        return result;
    }

    std::vector<Candidate> candidates;
    candidates.push_back(score_gameshark(rows88, Format::GameSharkRaw));
    candidates.push_back(score_gameshark(
        decrypt_rows(rows88, crypto::GameSharkV1Key),
        Format::GameSharkEncrypted));
    candidates.push_back(score_armax(rows88, Format::ActionReplayMaxRaw));
    candidates.push_back(score_armax(
        decrypt_armax_rows(rows88),
        Format::ActionReplayMaxEncrypted));

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& left, const Candidate& right) {
                  return left.score > right.score;
              });

    const Candidate& best = candidates[0];
    const Candidate& second = candidates[1];
    result.score = best.score;
    result.runner_up_score = second.score;
    result.reasons = best.reasons;

    const int margin = best.score - second.score;
    const int minimum = static_cast<int>(rows88.size()) * 6;
    if (best.score < minimum || margin < 3) {
        result.format = Format::Unknown;
        result.confidence = Confidence::Low;
        result.reasons.push_back(
            "8+8 candidates were too close or insufficiently plausible");
        return result;
    }

    result.format = best.format;
    if (margin >= 12 && best.score >= static_cast<int>(rows88.size()) * 11) {
        result.confidence = Confidence::High;
    } else if (margin >= 6) {
        result.confidence = Confidence::Medium;
    } else {
        result.confidence = Confidence::Low;
    }
    result.reasons.push_back(
        "best 8+8 score " + std::to_string(best.score) +
        ", runner-up " + std::to_string(second.score));
    return result;
}

} // namespace gba::detect
