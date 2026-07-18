#include "formats/mgba_cheats.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::mgba_cheats {
namespace {

std::string lower_copy(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return out;
}

bool is_hex(std::string_view value) {
    return !value.empty() &&
        std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isxdigit(ch) != 0;
        });
}

bool is_vba_row(std::string_view raw) {
    const std::string line = text::trim(raw);
    if (line.size() != 11U && line.size() != 13U && line.size() != 17U) {
        return false;
    }
    if (line[8] != ':' || !is_hex(std::string_view(line).substr(0U, 8U))) {
        return false;
    }
    return is_hex(std::string_view(line).substr(9U));
}

bool is_code_row(std::string_view line) {
    return text::is_code_line_8x4(line) ||
           text::is_code_line_8x8(line) || is_vba_row(line);
}

std::optional<MgbaCodeFamily> parse_family(std::string_view directive) {
    if (directive == "GSAv1") {
        return MgbaCodeFamily::GameSharkV1Encrypted;
    }
    if (directive == "GSAv1 raw") {
        return MgbaCodeFamily::GameSharkV1Raw;
    }
    if (directive == "PARv3") {
        return MgbaCodeFamily::ProActionReplayV3Encrypted;
    }
    if (directive == "PARv3 raw") {
        return MgbaCodeFamily::ProActionReplayV3Raw;
    }
    return std::nullopt;
}

std::string cleaned_name(std::string_view raw, std::size_t index) {
    std::string name = text::trim(raw);
    if (name.empty()) name = "Cheat " + std::to_string(index + 1U);
    for (char& ch : name) {
        if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
    }
    return name;
}

} // namespace

std::string_view directive_name(MgbaCodeFamily family) {
    switch (family) {
    case MgbaCodeFamily::AutoDetect: return {};
    case MgbaCodeFamily::GameSharkV1Encrypted: return "GSAv1";
    case MgbaCodeFamily::GameSharkV1Raw: return "GSAv1 raw";
    case MgbaCodeFamily::ProActionReplayV3Encrypted: return "PARv3";
    case MgbaCodeFamily::ProActionReplayV3Raw: return "PARv3 raw";
    }
    return {};
}

bool looks_like(std::string_view text_value) {
    bool header = false;
    bool row = false;
    bool directive = false;
    for (const std::string& raw : text::split_lines(
             text::normalize_newlines_lf(text::strip_utf8_bom(text_value)))) {
        const std::string line = text::trim(raw);
        if (line.empty()) continue;
        if (line.front() == '#') header = true;
        if (is_code_row(line)) row = true;
        if (line.front() == '!') {
            const std::string body = text::trim(
                std::string_view(line).substr(1U));
            const std::string lower = lower_copy(body);
            if (lower == "disabled" || lower == "reset" ||
                parse_family(body).has_value()) {
                directive = true;
            }
        }
    }
    return header && row && directive;
}

ParseResult parse(std::string_view text_value, bool force_extension_hint) {
    ParseResult result;
    const std::string normalized = text::normalize_newlines_lf(
        text::strip_utf8_bom(text_value));
    result.recognized = force_extension_hint || looks_like(normalized);
    if (!result.recognized) return result;

    Record current;
    bool has_current = false;
    bool next_disabled = false;
    MgbaCodeFamily inherited_family = MgbaCodeFamily::AutoDetect;
    std::optional<MgbaCodeFamily> persistent_directive;
    bool saw_structure = false;

    const auto finalize = [&]() {
        if (!has_current) return;
        current.name = cleaned_name(current.name, result.records.size());
        inherited_family = current.family;
        result.records.push_back(std::move(current));
        current = Record{};
        has_current = false;
    };

    for (const std::string& raw : text::split_lines(normalized)) {
        const std::string line = text::trim(raw);
        if (line.empty()) continue;

        if (line.front() == '!') {
            const std::string body = text::trim(
                std::string_view(line).substr(1U));
            const std::string lower = lower_copy(body);
            if (lower == "disabled") {
                next_disabled = true;
                saw_structure = true;
                continue;
            }
            if (lower == "reset") {
                persistent_directive.reset();
                saw_structure = true;
                continue;
            }
            if (const auto family = parse_family(body)) {
                persistent_directive = *family;
                saw_structure = true;
                continue;
            }
            result.warnings.push_back(
                "Ignored unknown mGBA directive '!" + body + "'.");
            continue;
        }

        if (line.front() == '#') {
            finalize();
            current = Record{};
            has_current = true;
            current.enabled = !next_disabled;
            next_disabled = false;
            current.family = inherited_family;
            if (persistent_directive) current.family = *persistent_directive;
            current.name = text::trim(std::string_view(line).substr(1U));
            saw_structure = true;
            continue;
        }

        if (!is_code_row(line)) {
            result.success = false;
            result.warnings.push_back(
                "An mGBA cheat row is not an 8+4, 8+8, or VBA "
                "ADDRESS:VALUE code.");
            return result;
        }

        if (!has_current) {
            // This mirrors mCheatParseFile: a headerless set is created with
            // auto detection, and pending family directives are not applied.
            current = Record{};
            has_current = true;
            current.enabled = !next_disabled;
            next_disabled = false;
            current.family = MgbaCodeFamily::AutoDetect;
        }
        current.code_lines.push_back(line);
        saw_structure = true;
    }
    finalize();

    if (!saw_structure || result.records.empty()) {
        result.success = false;
        result.warnings.push_back(
            "The mGBA .cheats file contains no cheat sets.");
        return result;
    }
    result.success = true;
    return result;
}

std::string serialize(const std::vector<Record>& records) {
    std::ostringstream out;
    for (const Record& record : records) {
        if (!record.enabled) out << "!disabled\n";
        if (const std::string_view directive = directive_name(record.family);
            !directive.empty()) {
            out << '!' << directive << '\n';
        }
        out << "# " << cleaned_name(record.name, 0U) << '\n';
        for (const std::string& raw : record.code_lines) {
            out << text::trim(raw) << '\n';
        }
    }
    return out.str();
}

} // namespace gba::mgba_cheats
