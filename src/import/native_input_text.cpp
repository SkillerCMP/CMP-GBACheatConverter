#include "import/native_input_internal.hpp"

#include "core/text.hpp"
#include "formats/codebreaker.hpp"
#include "formats/ezflash.hpp"
#include "formats/retroarch_cht.hpp"
#include "formats/mgba_cheats.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace gba::native_input::detail {
namespace {

std::string lower_copy(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return out;
}



} // namespace

bool looks_like_myboy(std::string_view text_value) {
    const std::string lower = lower_copy(text_value);
    return lower.find("<cheats") != std::string::npos &&
           lower.find("<cheat") != std::string::npos &&
           lower.find("<code>") != std::string::npos;
}

bool looks_like_mgba(std::string_view text_value) {
    return mgba_cheats::looks_like(text_value);
}

bool looks_like_libretro(std::string_view text_value) {
    return retroarch_cht::parse(text_value).recognized;
}

bool looks_like_ezflash(std::string_view text_value) {
    // E7 revision 6 permits a complete database of standalone
    // CodeName=commands rows with no section heading, so recognition must be
    // based on a successful parse rather than the presence of [Group].
    return !ezflash::parse(text_value).entries.empty();
}

Result import_ezflash(std::string_view, std::string_view text_value) {
    const std::string source_name = "EZ-Flash .cht";
    const std::string normalized = text::normalize_newlines_lf(
        text::strip_utf8_bom(text_value));
    if (!looks_like_ezflash(normalized)) return {};
    const CheatDocument document = ezflash::parse(normalized);
    if (document.entries.empty()) {
        return recognized_error(
            SourceFormat::EzFlashCht, source_name,
            document.warnings.empty()
                ? "The EZ-Flash .cht file contains no cheat entries."
                : document.warnings.front());
    }
    Result result;
    result.recognized = true;
    result.success = true;
    result.source_format = SourceFormat::EzFlashCht;
    result.input_format = InputFormat::EzFlash;
    result.source_name = source_name;
    result.text = normalized;
    if (!result.text.empty() && result.text.back() != '\n') {
        result.text.push_back('\n');
    }
    result.document = document;
    result.has_document = true;
    result.warnings = document.warnings;
    return result;
}


} // namespace gba::native_input::detail
