#include "export/output_modes_internal.hpp"

#include "formats/mister_gg.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {
namespace {

std::string safe_filename(std::string_view raw) {
    std::string name = single_line_name(raw);
    for (char& ch : name) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (uch < 0x20U || ch == '/' || ch == '\\' || ch == ':' ||
            ch == '*' || ch == '?' || ch == '"' || ch == '<' ||
            ch == '>' || ch == '|') {
            ch = '_';
        }
    }
    while (!name.empty() && (name.back() == ' ' || name.back() == '.')) {
        name.pop_back();
    }
    if (name.empty()) name = "Unnamed Cheat";
    if (name.size() > 180U) name.resize(180U);
    return name;
}

} // namespace


Result export_mister_gg(const CheatDocument& document) {
    Result result;
    if (document.entries.empty()) {
        return result;
    }
    if (document.entries.size() != 1U) {
        result.success = false;
        result.warnings.push_back(
            "Raw MiSTer .gg output requires exactly one cheat entry; use "
            "mister-zip for multiple cheats.");
        return result;
    }

    const CheatEntry& entry = document.entries.front();
    const mister_gg::EncodeResult encoded = mister_gg::encode_entry(entry);
    if (!encoded.success) {
        result.success = false;
        result.warnings.push_back(
            "Could not export '" + single_line_name(entry.name) +
            "' as raw MiSTer .gg: " + encoded.error);
        return result;
    }
    result.data = encoded.data;
    result.exported_entries = 1U;
    result.exported_records = encoded.record_count;
    return result;
}

Result export_mister(const CheatDocument& document) {
    Result result;
    std::vector<ZipItem> items;
    std::vector<std::string> used_names;
    for (const CheatEntry& entry : document.entries) {
        const mister_gg::EncodeResult encoded = mister_gg::encode_entry(entry);
        if (!encoded.success) {
            result.warnings.push_back(
                "Omitted '" + single_line_name(entry.name) +
                "' from MiSTer: " + encoded.error);
            continue;
        }

        std::string base = safe_filename(entry.name);
        std::string filename = base + ".gg";
        for (unsigned duplicate = 2U;
             std::find(used_names.begin(), used_names.end(), filename) !=
                 used_names.end(); ++duplicate) {
            filename = base + " (" + std::to_string(duplicate) + ").gg";
        }
        used_names.push_back(filename);
        items.push_back({
            filename,
            std::string(encoded.data.begin(), encoded.data.end())
        });
        result.exported_records += encoded.record_count;
        ++result.exported_entries;
    }
    result.data = store_zip(items);
    result.success = !items.empty() || document.entries.empty();
    return result;
}

} // namespace gba::output_modes::detail
