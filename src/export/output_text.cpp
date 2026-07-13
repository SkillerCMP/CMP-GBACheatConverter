#include "export/output_modes_internal.hpp"

#include "core/text.hpp"
#include "formats/ezflash.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {

Result export_ezflash(const CheatDocument& document,
                      const Options& options) {
    ezflash::Options ez_options;
    ez_options.mode = options.ezflash_mode == EzFlashMode::Original
        ? ezflash::Mode::Original
        : ezflash::Mode::Enhanced;

    const ezflash::Result converted =
        ezflash::export_document(document, ez_options);

    Result result;
    result.data = bytes_from_text(converted.text);
    result.warnings = converted.warnings;

    if (!converted.text.empty()) {
        const CheatDocument reparsed = ezflash::parse(converted.text);
        result.exported_entries = reparsed.entries.size();
        for (const CheatEntry& entry : reparsed.entries) {
            result.exported_records += entry.operations.size();
        }
        result.warnings.insert(result.warnings.end(),
                               reparsed.warnings.begin(),
                               reparsed.warnings.end());
    }

    // The normal EZ exporter marks the overall conversion unsuccessful when
    // any source entry is incompatible. Native Save Output As follows the
    // other native writers: save all compatible entries and report omissions.
    result.success = !result.data.empty() || document.entries.empty();
    return result;
}

Result export_myboy(const CheatDocument& document) {
    Result result;
    std::ostringstream out;
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<cheats>\n";
    for (const CheatEntry& entry : document.entries) {
        if (const auto fcd = exact_fcd(entry)) {
            out << " <cheat type=\"cb\" name=\"" << xml_escape(entry.name)
                << "\">\n";
            for (const auto& line : *fcd) {
                out << "  <code>" << text::hex(line.first, 8U) << ' '
                    << text::hex(line.second, 4U) << "</code>\n";
                ++result.exported_records;
            }
            out << " </cheat>\n";
            ++result.exported_entries;
        } else if (const auto ar = exact_armax(entry, false)) {
            out << " <cheat type=\"gs3\" name=\"" << xml_escape(entry.name)
                << "\">\n";
            for (const auto& line : *ar) {
                out << "  <code>" << text::hex(line.first, 8U) << ' '
                    << text::hex(line.second, 8U) << "</code>\n";
                ++result.exported_records;
            }
            out << " </cheat>\n";
            ++result.exported_entries;
        } else {
            warn_omitted(result, entry, "My Boy!");
        }
    }
    out << "</cheats>\n";
    result.data = bytes_from_text(out.str());
    result.success = result.exported_entries != 0U || document.entries.empty();
    return result;
}

Result export_mgba(const CheatDocument& document) {
    Result result;
    std::ostringstream out;
    for (const CheatEntry& entry : document.entries) {
        if (const auto fcd = exact_fcd(entry)) {
            out << "!disabled\n# " << single_line_name(entry.name) << '\n'
                << format_8x4(*fcd) << '\n';
            result.exported_records += fcd->size();
            ++result.exported_entries;
        } else if (const auto ar = exact_armax(entry, true)) {
            out << "!disabled\n!PARv3\n# " << single_line_name(entry.name)
                << '\n' << format_8x8(*ar) << '\n';
            result.exported_records += ar->size();
            ++result.exported_entries;
        } else {
            warn_omitted(result, entry, "mGBA");
        }
    }
    result.data = bytes_from_text(out.str());
    result.success = result.exported_entries != 0U || document.entries.empty();
    return result;
}

Result export_libretro(const CheatDocument& document) {
    Result result;
    struct Item { std::string name; std::string code; };
    std::vector<Item> items;
    for (const CheatEntry& entry : document.entries) {
        if (const auto fcd = exact_fcd(entry)) {
            std::ostringstream code;
            for (std::size_t index = 0; index < fcd->size(); ++index) {
                if (index != 0U) code << '+';
                code << text::hex((*fcd)[index].first, 8U) << ' '
                     << text::hex((*fcd)[index].second, 4U);
            }
            items.push_back({quote_escape(entry.name), code.str()});
            result.exported_records += fcd->size();
        } else if (const auto ar = exact_armax(entry, false)) {
            std::ostringstream code;
            for (std::size_t index = 0; index < ar->size(); ++index) {
                if (index != 0U) code << '+';
                code << text::hex((*ar)[index].first, 8U) << ' '
                     << text::hex((*ar)[index].second, 8U);
            }
            items.push_back({quote_escape(entry.name), code.str()});
            result.exported_records += ar->size();
        } else {
            warn_omitted(result, entry, "Libretro / RetroArch");
        }
    }
    result.exported_entries = items.size();
    std::ostringstream out;
    out << "cheats = " << items.size() << "\n\n";
    for (std::size_t index = 0; index < items.size(); ++index) {
        out << "cheat" << index << "_desc = \"" << items[index].name
            << "\"\n";
        out << "cheat" << index << "_code = \"" << items[index].code
            << "\"\n";
        out << "cheat" << index << "_enable = false\n\n";
    }
    result.data = bytes_from_text(out.str());
    result.success = !items.empty() || document.entries.empty();
    return result;
}

Result export_mednafen(const CheatDocument& document,
                       const Options& options) {
    Result result;
    if (options.rom_md5.size() != 32U || options.game_name.empty()) {
        result.success = false;
        result.warnings.push_back(
            "Mednafen export requires a ROM MD5 and game name.");
        return result;
    }
    std::ostringstream out;
    out << '[' << options.rom_md5 << "] " << single_line_name(options.game_name)
        << '\n';
    for (const CheatEntry& entry : document.entries) {
        if (!direct_writes_only(entry)) {
            warn_omitted(result, entry, "Mednafen");
            continue;
        }
        for (const Operation& operation : entry.operations) {
            out << "R I " << static_cast<unsigned>(operation.width)
                << " L 0 " << lower_hex(operation.address) << ' '
                << lower_hex(operation.value) << ' '
                << single_line_name(entry.name) << "\n\n";
            ++result.exported_records;
        }
        ++result.exported_entries;
    }
    result.data = bytes_from_text(out.str());
    result.success = result.exported_entries != 0U || document.entries.empty();
    return result;
}

} // namespace gba::output_modes::detail
