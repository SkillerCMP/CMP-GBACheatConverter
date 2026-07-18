#include "core/cmp.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gba::cmp {
namespace {

bool is_group_open(std::string_view raw) {
    const std::string line = text::trim(raw);
    return line.size() >= 3U && line.front() == '!' &&
           line.rfind("!!", 0U) != 0U && line.back() == ':';
}

std::string group_name(std::string_view raw) {
    std::string line = text::trim(raw);
    if (!line.empty() && line.front() == '!') line.erase(line.begin());
    if (!line.empty() && line.back() == ':') line.pop_back();
    return text::trim(line);
}

bool is_code_row(std::string_view raw) {
    std::string line = text::trim(raw);
    if (!line.empty() && line.front() == '$') {
        line = text::trim(std::string_view(line).substr(1U));
    }
    return text::is_code_line_8x4(line) || text::is_code_line_8x8(line);
}

std::string clean_code_row(std::string_view raw) {
    std::string line = text::trim(raw);
    if (!line.empty() && line.front() == '$') {
        line = text::trim(std::string_view(line).substr(1U));
    }
    return line;
}

std::string make_synthetic(SourceKind kind, std::size_t index) {
    return kind == SourceKind::GroupHeader
        ? "__CMP_GROUP_" + std::to_string(index) + "__"
        : "__CMP_ENTRY_" + std::to_string(index) + "__";
}

bool is_comment(std::string_view raw) {
    const std::string line = text::trim(raw);
    return line.empty() || line.front() == '#' || line.front() == ';' ||
           line.rfind("//", 0U) == 0U || line.rfind("/*", 0U) == 0U ||
           line.rfind("--", 0U) == 0U;
}

std::string display_for_synthetic(
    std::string_view value,
    const std::vector<SourceItem>& items) {
    for (const SourceItem& item : items) {
        if (item.synthetic_name == value) {
            return item.kind == SourceKind::Entry
                ? item.display_name
                : (item.group_path.empty()
                       ? std::string("CMP group")
                       : item.group_path.back());
        }
    }
    return std::string(value);
}

void replace_all(std::string& value,
                 std::string_view from,
                 std::string_view to) {
    if (from.empty()) return;
    std::size_t position = 0U;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }
}

} // namespace

bool looks_like(std::string_view input) {
    const auto lines = text::split_lines(text::normalize_newlines_lf(input));
    bool saw_group = false;
    bool saw_cmp_row = false;
    for (const std::string& raw : lines) {
        const std::string line = text::trim(raw);
        if (line.empty()) continue;
        if (is_group_open(line) || line == "!!") saw_group = true;
        if (line.front() == '+' || line.front() == '$' ||
            line.rfind("%Credits:", 0U) == 0U || is_code_row(line)) {
            saw_cmp_row = true;
        }
    }
    return saw_group && saw_cmp_row;
}

NormalizedInput normalize_input(std::string_view input) {
    NormalizedInput result;
    result.recognized = looks_like(input);
    if (!result.recognized) {
        result.text = std::string(input);
        return result;
    }

    const std::vector<std::string> lines =
        text::split_lines(text::normalize_newlines_lf(input));
    std::vector<std::string> output_lines(lines.size());
    std::vector<std::string> group_stack;
    std::vector<std::size_t> group_item_stack;
    std::optional<std::size_t> current_entry;
    std::size_t order = 1U;

    const auto start_item = [&](SourceKind kind,
                                std::vector<std::string> path,
                                std::string display,
                                std::size_t line_index,
                                NormalizedInput& target) {
        SourceItem item;
        item.kind = kind;
        item.synthetic_name = make_synthetic(kind, target.items.size());
        item.group_path = std::move(path);
        item.display_name = std::move(display);
        item.order = order++;
        target.items.push_back(std::move(item));
        output_lines[line_index] =
            '[' + target.items.back().synthetic_name + ']';
        return target.items.size() - 1U;
    };

    for (std::size_t index = 0U; index < lines.size(); ++index) {
        const std::string line = text::trim(lines[index]);
        if (line.empty() || is_comment(line)) continue;

        if (is_group_open(line)) {
            const std::string name = group_name(line);
            if (name.empty()) {
                result.warnings.push_back(
                    "CMP line " + std::to_string(index + 1U) +
                    ": empty group name was ignored");
                continue;
            }
            group_stack.push_back(name);
            current_entry.reset();
            const std::size_t item_index = start_item(
                SourceKind::GroupHeader, group_stack, name, index, result);
            group_item_stack.push_back(item_index);
            continue;
        }

        if (line == "!!") {
            current_entry.reset();
            if (group_stack.empty()) {
                result.warnings.push_back(
                    "CMP line " + std::to_string(index + 1U) +
                    ": unmatched !! was ignored");
            } else {
                group_stack.pop_back();
                group_item_stack.pop_back();
            }
            continue;
        }

        if (line.front() == '+') {
            std::string name = text::trim(std::string_view(line).substr(1U));
            if (name.empty()) {
                result.warnings.push_back(
                    "CMP line " + std::to_string(index + 1U) +
                    ": empty +Code name was ignored");
                current_entry.reset();
                continue;
            }
            current_entry = start_item(
                SourceKind::Entry, group_stack, std::move(name), index, result);
            continue;
        }

        if (line.rfind("%Credits:", 0U) == 0U) {
            if (!current_entry) {
                result.warnings.push_back(
                    "CMP line " + std::to_string(index + 1U) +
                    ": %Credits has no active +Code entry");
            } else {
                result.items[*current_entry].credits = text::trim(
                    std::string_view(line).substr(9U));
            }
            continue;
        }

        if (is_code_row(line)) {
            const std::string code = clean_code_row(line);
            if (current_entry) {
                output_lines[index] = code;
            } else if (!group_item_stack.empty()) {
                output_lines[index] = code;
            } else {
                result.warnings.push_back(
                    "CMP line " + std::to_string(index + 1U) +
                    ": code row outside a group or +Code entry was ignored");
            }
            continue;
        }

        // Accept the common relaxed database form where a plain code name
        // is followed by one or more code rows. This applies both to
        // standalone codes and to entries inside !Group:/!! blocks.
        if (line.front() != '!' && line.front() != '%' &&
            line.front() != '^') {
            std::size_t next = index + 1U;
            while (next < lines.size() &&
                   (text::trim(lines[next]).empty() ||
                    is_comment(lines[next]))) {
                ++next;
            }
            if (next < lines.size() && is_code_row(lines[next])) {
                current_entry = start_item(
                    SourceKind::Entry, group_stack, line, index, result);
                continue;
            }
        }
    }

    if (!group_stack.empty()) {
        result.warnings.push_back(
            "CMP input ended with " + std::to_string(group_stack.size()) +
            " unclosed group(s); they were closed at end of input");
    }

    std::string normalized;
    for (std::size_t index = 0U; index < output_lines.size(); ++index) {
        normalized += output_lines[index];
        if (index + 1U < output_lines.size()) normalized.push_back('\n');
    }
    result.text = std::move(normalized);
    return result;
}

CheatDocument attach_layout(const NormalizedInput& normalized,
                            CheatDocument parsed) {
    if (!normalized.recognized) return parsed;

    CheatDocument result;
    result.warnings = normalized.warnings;
    result.warnings.insert(result.warnings.end(),
                           parsed.warnings.begin(), parsed.warnings.end());

    std::unordered_map<std::string, CheatEntry> parsed_by_name;
    for (CheatEntry& entry : parsed.entries) {
        parsed_by_name.emplace(entry.name, std::move(entry));
    }

    for (const SourceItem& item : normalized.items) {
        CheatEntry decoded;
        const auto found = parsed_by_name.find(item.synthetic_name);
        if (found != parsed_by_name.end()) {
            decoded = std::move(found->second);
            parsed_by_name.erase(found);
        }

        if (item.kind == SourceKind::GroupHeader) {
            CmpGroup group;
            group.path = item.group_path;
            group.header_operations = std::move(decoded.operations);
            group.order = item.order;
            result.cmp_groups.push_back(std::move(group));
            continue;
        }

        decoded.name = item.display_name;
        decoded.credits = item.credits;
        decoded.cmp_group_path = item.group_path;
        decoded.cmp_order = item.order;
        result.entries.push_back(std::move(decoded));
    }

    for (auto& [name, entry] : parsed_by_name) {
        entry.name = name;
        result.entries.push_back(std::move(entry));
    }

    for (std::string& warning : result.warnings) {
        for (const SourceItem& item : normalized.items) {
            replace_all(warning, item.synthetic_name,
                        display_for_synthetic(item.synthetic_name,
                                              normalized.items));
        }
    }
    return result;
}

} // namespace gba::cmp
