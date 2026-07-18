#include "core/cmp.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gba::cmp {
namespace {

bool path_is_prefix(const std::vector<std::string>& prefix,
                    const std::vector<std::string>& path) {
    return prefix.size() <= path.size() &&
           std::equal(prefix.begin(), prefix.end(), path.begin());
}

std::vector<Operation> inherited_operations(
    const CheatDocument& document,
    const std::vector<std::string>& path) {
    std::vector<const CmpGroup*> groups;
    for (const CmpGroup& group : document.cmp_groups) {
        if (!group.path.empty() && path_is_prefix(group.path, path)) {
            groups.push_back(&group);
        }
    }
    std::stable_sort(groups.begin(), groups.end(),
        [](const CmpGroup* left, const CmpGroup* right) {
            if (left->path.size() != right->path.size()) {
                return left->path.size() < right->path.size();
            }
            return left->order < right->order;
        });

    std::vector<Operation> result;
    for (const CmpGroup* group : groups) {
        result.insert(result.end(), group->header_operations.begin(),
                      group->header_operations.end());
    }
    return result;
}

std::string join_path(const std::vector<std::string>& path) {
    std::string result;
    for (const std::string& part : path) {
        if (!result.empty()) result += " / ";
        result += part;
    }
    return result;
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

std::string synthetic_name(std::size_t index) {
    return "__CMP_OUTPUT_" + std::to_string(index) + "__";
}

struct Event {
    enum class Kind { Group, Entry } kind = Kind::Entry;
    std::vector<std::string> path;
    std::size_t order = 0U;
    const CmpGroup* group = nullptr;
    const CheatEntry* entry = nullptr;
    std::string display_name;
    std::string credits;
};

std::vector<std::string> output_path_for_entry(
    const CheatEntry& entry, bool use_ez_group) {
    if (!entry.cmp_group_path.empty()) return entry.cmp_group_path;
    if (use_ez_group) {
        std::string name = entry.ezflash_group_name;
        if (entry.ezflash_group_mode == EzFlashGroupMode::ZeroOrOne) {
            name += "|ONE";
        }
        return {std::move(name)};
    }
    return {"Codes"};
}

std::string output_name_for_entry(const CheatEntry& entry,
                                  bool use_ez_group) {
    if (use_ez_group && !entry.ezflash_option_name.empty()) {
        return entry.ezflash_option_name;
    }
    return entry.name;
}

std::pair<std::string, std::string> split_inline_credits(
    std::string name, std::string credits) {
    if (!credits.empty()) return {std::move(name), std::move(credits)};
    const std::string marker = " , by ";
    const std::size_t by = name.find(marker);
    if (by == std::string::npos) {
        return {std::move(name), std::move(credits)};
    }
    const std::size_t credit_start = by + marker.size();
    const std::size_t crypt = name.find(" , Crypt_", credit_start);
    credits = text::trim(std::string_view(name).substr(
        credit_start,
        crypt == std::string::npos
            ? std::string::npos
            : crypt - credit_start));
    name = text::trim(std::string_view(name).substr(0U, by));
    return {std::move(name), std::move(credits)};
}

std::string restore_names(std::string value,
                          const PreparedOutput& prepared) {
    for (const auto& mapping : prepared.name_map) {
        replace_all(value, mapping.first, mapping.second);
    }
    return value;
}

std::string format_cmp_rows(std::string_view raw,
                            const PreparedOutput& prepared) {
    std::vector<std::string> lines =
        text::split_lines(text::normalize_newlines_lf(raw));
    while (!lines.empty() && text::trim(lines.front()).empty()) {
        lines.erase(lines.begin());
    }
    while (!lines.empty() && text::trim(lines.back()).empty()) {
        lines.pop_back();
    }

    std::ostringstream output;
    for (const std::string& raw_line : lines) {
        std::string line = restore_names(raw_line, prepared);
        const std::string trimmed = text::trim(line);
        if (trimmed.empty()) {
            output << '\n';
        } else if (text::is_code_line_8x4(trimmed) ||
                   text::is_code_line_8x8(trimmed)) {
            output << '$' << trimmed << '\n';
        } else if (trimmed.front() == '$') {
            output << trimmed << '\n';
        } else {
            output << line << '\n';
        }
    }
    return output.str();
}

} // namespace

CheatDocument flatten_for_device_output(const CheatDocument& document) {
    bool has_cmp = !document.cmp_groups.empty();
    if (!has_cmp) {
        has_cmp = std::any_of(
            document.entries.begin(), document.entries.end(),
            [](const CheatEntry& entry) {
                return !entry.cmp_group_path.empty();
            });
    }
    if (!has_cmp) return document;

    CheatDocument result;
    result.warnings = document.warnings;
    result.entries.reserve(document.entries.size());
    for (const CheatEntry& source : document.entries) {
        CheatEntry entry = source;
        std::vector<Operation> inherited =
            inherited_operations(document, source.cmp_group_path);
        inherited.insert(inherited.end(), entry.operations.begin(),
                         entry.operations.end());
        entry.operations = std::move(inherited);
        entry.cmp_group_path.clear();
        entry.cmp_order = 0U;
        result.entries.push_back(std::move(entry));
    }
    return result;
}

CheatDocument prepare_for_ezflash(const CheatDocument& document) {
    bool has_cmp = !document.cmp_groups.empty();
    if (!has_cmp) {
        has_cmp = std::any_of(
            document.entries.begin(), document.entries.end(),
            [](const CheatEntry& entry) {
                return !entry.cmp_group_path.empty();
            });
    }
    if (!has_cmp) return document;

    CheatDocument result;
    result.warnings = document.warnings;
    result.entries.reserve(document.entries.size());
    for (const CheatEntry& source : document.entries) {
        CheatEntry entry = source;
        std::vector<Operation> inherited =
            inherited_operations(document, source.cmp_group_path);
        inherited.insert(inherited.end(), entry.operations.begin(),
                         entry.operations.end());
        entry.operations = std::move(inherited);
        if (!source.cmp_group_path.empty()) {
            std::vector<std::string> path = source.cmp_group_path;
            constexpr std::string_view one_suffix = "|ONE";
            std::string& leaf = path.back();
            if (leaf.size() >= one_suffix.size() &&
                leaf.compare(leaf.size() - one_suffix.size(),
                             one_suffix.size(), one_suffix) == 0) {
                leaf.resize(leaf.size() - one_suffix.size());
                leaf = text::trim(leaf);
                entry.ezflash_group_mode = EzFlashGroupMode::ZeroOrOne;
            } else {
                entry.ezflash_group_mode = EzFlashGroupMode::MultiSelect;
            }
            entry.ezflash_group_name = join_path(path);
            entry.ezflash_option_name = source.name;
        }
        entry.cmp_group_path.clear();
        entry.cmp_order = 0U;
        result.entries.push_back(std::move(entry));
    }
    return result;
}

PreparedOutput prepare_output(const CheatDocument& document) {
    PreparedOutput prepared;
    prepared.document.warnings = document.warnings;

    std::vector<Event> events;
    events.reserve(document.cmp_groups.size() + document.entries.size());
    std::size_t generated_order = 1000000U;

    for (const CmpGroup& group : document.cmp_groups) {
        if (group.path.empty()) continue;
        Event event;
        event.kind = Event::Kind::Group;
        event.path = group.path;
        event.order = group.order == 0U ? generated_order++ : group.order;
        event.group = &group;
        events.push_back(std::move(event));
    }

    // Ensure EZ-Flash groups and any implied CMP paths get an opening event.
    std::unordered_map<std::string, std::size_t> ez_group_counts;
    for (const CheatEntry& entry : document.entries) {
        if (!entry.ezflash_group_name.empty()) {
            ++ez_group_counts[entry.ezflash_group_name];
        }
    }

    std::unordered_map<std::string, std::size_t> implied_paths;
    for (const CheatEntry& entry : document.entries) {
        const bool use_ez_group = entry.cmp_group_path.empty() &&
            !entry.ezflash_group_name.empty() &&
            (entry.ezflash_option_name != "ON" ||
             ez_group_counts[entry.ezflash_group_name] > 1U);
        const std::vector<std::string> path =
            output_path_for_entry(entry, use_ez_group);
        const std::string key = join_path(path);
        const std::size_t order = entry.cmp_order == 0U
            ? generated_order++ : entry.cmp_order;
        const auto found = implied_paths.find(key);
        if (found == implied_paths.end() || order < found->second) {
            implied_paths[key] = order;
        }

        Event event;
        event.kind = Event::Kind::Entry;
        event.path = path;
        event.order = order;
        event.entry = &entry;
        auto metadata = split_inline_credits(
            output_name_for_entry(entry, use_ez_group), entry.credits);
        event.display_name = std::move(metadata.first);
        event.credits = std::move(metadata.second);
        events.push_back(std::move(event));
    }

    for (const auto& path_order : implied_paths) {
        const std::size_t implied_order = path_order.second;
        // Existing explicit group events already establish their paths.
        const auto entry_event = std::find_if(
            events.begin(), events.end(),
            [implied_order](const Event& event) {
                return event.kind == Event::Kind::Entry &&
                       event.order == implied_order;
            });
        if (entry_event == events.end()) continue;
        const bool explicit_group = std::any_of(
            events.begin(), events.end(),
            [&](const Event& event) {
                return event.kind == Event::Kind::Group &&
                       event.path == entry_event->path;
            });
        if (!explicit_group) {
            Event group_event;
            group_event.kind = Event::Kind::Group;
            group_event.path = entry_event->path;
            group_event.order = implied_order;
            events.push_back(std::move(group_event));
        }
    }

    std::stable_sort(events.begin(), events.end(),
        [](const Event& left, const Event& right) {
            if (left.order != right.order) return left.order < right.order;
            return left.kind == Event::Kind::Group &&
                   right.kind == Event::Kind::Entry;
        });

    std::vector<std::string> open_path;
    std::size_t synthetic_index = 0U;
    const auto transition_to = [&](const std::vector<std::string>& target,
                                   PreparedOutput& target_output,
                                   std::vector<std::string>& current) {
        std::size_t common = 0U;
        while (common < current.size() && common < target.size() &&
               current[common] == target[common]) {
            ++common;
        }
        while (current.size() > common) {
            target_output.layout.push_back(
                RenderItem{RenderKind::GroupClose, {}, {}, {}, {}});
            current.pop_back();
        }
        while (current.size() < target.size()) {
            current.push_back(target[current.size()]);
            target_output.layout.push_back(RenderItem{
                RenderKind::GroupOpen, current.back(), {}, {}, {}});
        }
    };

    for (const Event& event : events) {
        transition_to(event.path, prepared, open_path);
        if (event.kind == Event::Kind::Group) {
            if (!event.group || event.group->header_operations.empty()) {
                continue;
            }
            const std::string synthetic = synthetic_name(synthetic_index++);
            CheatEntry entry;
            entry.name = synthetic;
            entry.operations = event.group->header_operations;
            prepared.document.entries.push_back(std::move(entry));
            prepared.layout.push_back(RenderItem{
                RenderKind::GroupHeader, {}, synthetic,
                event.path.back(), {}});
            prepared.name_map.emplace_back(synthetic, event.path.back());
            continue;
        }

        const std::string synthetic = synthetic_name(synthetic_index++);
        CheatEntry entry = *event.entry;
        entry.name = synthetic;
        entry.cmp_group_path.clear();
        entry.cmp_order = 0U;
        entry.ezflash_group_name.clear();
        entry.ezflash_option_name.clear();
        entry.ezflash_group_mode = EzFlashGroupMode::None;
        prepared.document.entries.push_back(std::move(entry));
        prepared.layout.push_back(RenderItem{
            RenderKind::Entry, {}, synthetic, event.display_name,
            event.credits});
        prepared.name_map.emplace_back(synthetic, event.display_name);
    }

    while (!open_path.empty()) {
        prepared.layout.push_back(
            RenderItem{RenderKind::GroupClose, {}, {}, {}, {}});
        open_path.pop_back();
    }
    return prepared;
}

std::string render_output(std::string_view exported_text,
                          const PreparedOutput& prepared) {
    std::unordered_map<std::string, std::string> chunks;
    std::unordered_map<std::string, std::string> header_to_name;
    for (const auto& mapping : prepared.name_map) {
        header_to_name.emplace('[' + mapping.first + ']', mapping.first);
    }

    std::string preamble;
    std::string current_name;
    const auto lines = text::split_lines(
        text::normalize_newlines_lf(exported_text));
    for (const std::string& line : lines) {
        const auto header = header_to_name.find(text::trim(line));
        if (header != header_to_name.end()) {
            current_name = header->second;
            continue;
        }
        if (current_name.empty()) {
            preamble += line;
            preamble.push_back('\n');
        } else {
            chunks[current_name] += line;
            chunks[current_name].push_back('\n');
        }
    }

    std::ostringstream output;
    bool preamble_written = false;
    for (const RenderItem& item : prepared.layout) {
        switch (item.kind) {
        case RenderKind::GroupOpen:
            output << '!' << item.group_name << ":\n";
            if (!preamble_written) {
                output << format_cmp_rows(preamble, prepared);
                preamble_written = true;
            }
            break;
        case RenderKind::GroupClose:
            output << "!!\n";
            break;
        case RenderKind::GroupHeader: {
            const auto found = chunks.find(item.synthetic_name);
            if (found != chunks.end()) {
                output << format_cmp_rows(found->second, prepared);
            }
            break;
        }
        case RenderKind::Entry: {
            output << '+' << item.display_name << '\n';
            if (!item.credits.empty()) {
                output << "%Credits: " << item.credits << '\n';
            }
            const auto found = chunks.find(item.synthetic_name);
            if (found != chunks.end()) {
                output << format_cmp_rows(found->second, prepared);
            }
            break;
        }
        }
    }
    if (!preamble_written) {
        output << format_cmp_rows(preamble, prepared);
    }
    return output.str();
}

void restore_warning_names(std::vector<std::string>& warnings,
                           const PreparedOutput& prepared) {
    for (std::string& warning : warnings) {
        warning = restore_names(std::move(warning), prepared);
    }
}

} // namespace gba::cmp
