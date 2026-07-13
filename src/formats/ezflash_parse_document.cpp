#include "formats/ezflash_parse_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::ezflash::parse_detail {

std::string remove_spaces_and_tabs(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    for (const char ch : input) {
        if (ch != ' ' && ch != '\t') {
            output.push_back(ch);
        }
    }
    return output;
}

CheatDocument parse_document(std::string_view input) {
    CheatDocument document;
    CheatEntry* current = nullptr;

    const auto ensure_current = [&]() -> CheatEntry& {
        if (!current) {
            document.entries.push_back(
                CheatEntry{"Converted EZ-Flash Code", {}});
            current = &document.entries.back();
        }
        return *current;
    };

    const auto append_condition_and_actions =
        [&](CheatEntry& entry,
            const std::vector<ParsedRun>& condition_runs,
            std::vector<Operation> actions,
            std::size_t line_number,
            std::string_view source_text,
            std::string_view note) -> bool {
            std::vector<ConditionTerm> terms =
                make_condition_terms(condition_runs);
            if (terms.empty() || actions.empty()) {
                return false;
            }

            Operation condition;
            condition.kind = OperationKind::IfEqual;
            condition.address = terms.front().address;
            condition.value = terms.front().value;
            condition.width = terms.front().width;
            condition.condition_span =
                static_cast<std::uint32_t>(actions.size());
            condition.source_line = line_number;
            condition.source_text = std::string(source_text);
            condition.note = std::string(note);
            if (terms.size() != 1U ||
                (terms.front().width != 1U &&
                 terms.front().width != 2U &&
                 terms.front().width != 4U)) {
                condition.condition_terms = std::move(terms);
            }

            entry.operations.push_back(std::move(condition));
            entry.operations.insert(entry.operations.end(),
                                    actions.begin(), actions.end());
            return true;
        };

    const auto parse_canonical_romif =
        [&](CheatEntry& entry,
            std::string_view payload,
            std::size_t line_number,
            std::string_view source_text) -> bool {
            enum class RomIfPhase { Condition, Runtime, Rom };
            RomIfPhase mode = RomIfPhase::Condition;
            std::vector<ParsedRun> conditions;
            std::vector<Operation> actions;

            for (const std::string& raw_segment :
                 split_semicolon_runs(payload)) {
                std::string_view segment = raw_segment;
                std::string_view runtime_name;
                std::size_t runtime_prefix = 0U;

                if (segment.rfind("ON=", 0U) == 0U ||
                    segment.rfind("ON:", 0U) == 0U) {
                    mode = RomIfPhase::Runtime;
                    segment.remove_prefix(3U);
                } else if (segment.rfind("ROM=", 0U) == 0U ||
                           segment.rfind("ROM:", 0U) == 0U) {
                    mode = RomIfPhase::Rom;
                    segment.remove_prefix(4U);
                } else {
                    for (const std::string_view candidate :
                         {std::string_view{"ADD"}, std::string_view{"SUB"},
                          std::string_view{"PTR"}, std::string_view{"FILL"},
                          std::string_view{"SLIDE"}}) {
                        const std::string equals =
                            std::string(candidate) + "=";
                        const std::string colon =
                            std::string(candidate) + ":";
                        if (segment.rfind(equals, 0U) == 0U ||
                            segment.rfind(colon, 0U) == 0U) {
                            runtime_name = candidate;
                            runtime_prefix = candidate.size() + 1U;
                            break;
                        }
                    }
                }
                if (segment.empty()) return false;

                if (!runtime_name.empty()) {
                    const auto operation = parse_named_runtime_operation(
                        runtime_name, segment.substr(runtime_prefix),
                        document.warnings, line_number, source_text);
                    if (!operation) return false;
                    actions.push_back(*operation);
                    mode = RomIfPhase::Runtime;
                } else if (mode == RomIfPhase::Condition) {
                    const auto runs = parse_rom_payload_runs(
                        segment, document.warnings, line_number);
                    if (!runs) return false;
                    conditions.insert(
                        conditions.end(), runs->begin(), runs->end());
                } else if (mode == RomIfPhase::Runtime) {
                    const auto runs = parse_payload_runs(
                        segment, false, document.warnings, line_number);
                    if (!runs) return false;
                    auto writes = make_write_operations(
                        *runs, line_number, source_text);
                    actions.insert(actions.end(),
                                   writes.begin(), writes.end());
                } else {
                    const auto runs = parse_rom_payload_runs(
                        segment, document.warnings, line_number);
                    if (!runs) return false;
                    auto patches = make_rom_patch_operations(
                        *runs, line_number, source_text);
                    actions.insert(actions.end(),
                                   patches.begin(), patches.end());
                }
            }

            return append_condition_and_actions(
                entry, conditions, std::move(actions), line_number,
                source_text, "EZ-Flash Enhanced v3 canonical ROMIF guard");
        };

    const auto parse_action_segments =
        [&](std::string_view payload,
            std::size_t line_number,
            std::string_view source_text,
            bool allow_rom,
            std::vector<Operation>& actions) -> bool {
            for (const std::string& raw_segment :
                 split_semicolon_runs(payload)) {
                std::string_view segment = raw_segment;
                std::string_view name;
                std::size_t prefix = 0U;
                for (const std::string_view candidate :
                     {std::string_view{"ADD"}, std::string_view{"SUB"},
                      std::string_view{"PTR"}, std::string_view{"FILL"},
                      std::string_view{"SLIDE"}}) {
                    const std::string equals = std::string(candidate) + "=";
                    const std::string colon = std::string(candidate) + ":";
                    if (segment.rfind(equals, 0U) == 0U ||
                        segment.rfind(colon, 0U) == 0U) {
                        name = candidate;
                        prefix = candidate.size() + 1U;
                        break;
                    }
                }
                if (!name.empty()) {
                    const auto operation = parse_named_runtime_operation(
                        name, segment.substr(prefix), document.warnings,
                        line_number, source_text);
                    if (!operation) return false;
                    actions.push_back(*operation);
                    continue;
                }
                if (segment.rfind("ROM:", 0U) == 0U ||
                    segment.rfind("ROM=", 0U) == 0U) {
                    if (!allow_rom) return false;
                    const auto runs = parse_rom_payload_runs(
                        segment.substr(4U), document.warnings, line_number);
                    if (!runs) return false;
                    auto patches = make_rom_patch_operations(
                        *runs, line_number, source_text);
                    actions.insert(actions.end(),
                                   patches.begin(), patches.end());
                    continue;
                }

                const auto runs = parse_payload_runs(
                    segment, false, document.warnings, line_number);
                if (!runs) return false;
                auto writes = make_write_operations(
                    *runs, line_number, source_text);
                actions.insert(actions.end(), writes.begin(), writes.end());
            }
            return !actions.empty();
        };

    const auto parse_runtime_option =
        [&](CheatEntry& entry,
            std::string_view first_key,
            std::string_view full_line,
            std::size_t line_number) -> bool {
            struct Block {
                OperationKind kind = OperationKind::IfEqual;
                std::vector<ParsedRun> conditions;
                std::vector<Operation> true_actions;
                std::vector<Operation> false_actions;
                bool has_else = false;
                bool open = false;
            } block;

            std::vector<Operation> parsed;
            std::vector<Operation> independent_rom;
            enum class Context { None, Condition, TrueWrite, FalseWrite };
            Context context = Context::None;

            const auto finish_block = [&]() -> bool {
                if (!block.open) return true;
                std::vector<ConditionTerm> terms =
                    make_condition_terms(block.conditions);
                if (terms.empty() || block.true_actions.empty()) return false;
                Operation condition;
                condition.kind = block.kind;
                condition.address = terms.front().address;
                condition.value = terms.front().value;
                condition.width = terms.front().width;
                condition.condition_span = static_cast<std::uint32_t>(
                    block.true_actions.size());
                condition.condition_else_span = static_cast<std::uint32_t>(
                    block.false_actions.size());
                condition.condition_has_else = block.has_else;
                condition.source_line = line_number;
                condition.source_text = std::string(full_line);
                condition.note = "EZ-Flash Enhanced v3 runtime condition";
                if (terms.size() != 1U ||
                    (terms.front().width != 1U &&
                     terms.front().width != 2U &&
                     terms.front().width != 4U)) {
                    condition.condition_terms = std::move(terms);
                }
                parsed.push_back(std::move(condition));
                parsed.insert(parsed.end(), block.true_actions.begin(),
                              block.true_actions.end());
                parsed.insert(parsed.end(), block.false_actions.begin(),
                              block.false_actions.end());
                block = Block{};
                context = Context::None;
                return true;
            };

            const auto start_condition =
                [&](std::string_view key,
                    std::string_view payload) -> bool {
                    if (!finish_block()) return false;
                    const auto kind = condition_kind_for_key(key);
                    if (!kind) return false;
                    const auto runs = parse_payload_runs(
                        payload, true, document.warnings, line_number);
                    if (!runs) return false;
                    block.kind = *kind;
                    block.conditions = *runs;
                    block.open = true;
                    context = Context::Condition;
                    return true;
                };

            std::vector<std::string> segments =
                split_semicolon_runs(full_line);
            if (segments.empty()) return false;

            bool first_segment = true;
            for (const std::string& raw_segment : segments) {
                std::string_view segment = raw_segment;

                if (segment == "ELSE") {
                    if (!block.open || block.has_else ||
                        block.true_actions.empty()) return false;
                    block.has_else = true;
                    context = Context::FalseWrite;
                    first_segment = false;
                    continue;
                }
                if (segment == "ENDIF") {
                    if (!finish_block()) return false;
                    first_segment = false;
                    continue;
                }

                bool handled_condition = false;
                for (const std::string_view key :
                     {std::string_view{"IFNE"}, std::string_view{"IFLT"},
                      std::string_view{"IFGT"}, std::string_view{"IFLE"},
                      std::string_view{"IFGE"}, std::string_view{"IF"}}) {
                    const std::string prefix = std::string(key) + "=";
                    if (segment.rfind(prefix, 0U) == 0U) {
                        if (!start_condition(key, segment.substr(prefix.size())))
                            return false;
                        handled_condition = true;
                        break;
                    }
                }
                if (handled_condition) {
                    first_segment = false;
                    continue;
                }

                if (segment.rfind("ROM=", 0U) == 0U) {
                    if (!finish_block()) return false;
                    const auto runs = parse_rom_payload_runs(
                        segment.substr(4U), document.warnings, line_number);
                    if (!runs) return false;
                    auto patches = make_rom_patch_operations(
                        *runs, line_number, full_line);
                    independent_rom.insert(independent_rom.end(),
                                           patches.begin(), patches.end());
                    context = Context::None;
                    first_segment = false;
                    continue;
                }

                if (segment.rfind("ON=", 0U) == 0U ||
                    segment.rfind("ON:", 0U) == 0U) {
                    const std::string_view payload = segment.substr(3U);
                    std::vector<Operation> actions;
                    if (!parse_action_segments(payload, line_number,
                                               full_line, false, actions)) {
                        return false;
                    }
                    if (block.open) {
                        auto& destination = block.has_else
                            ? block.false_actions : block.true_actions;
                        destination.insert(destination.end(),
                                           actions.begin(), actions.end());
                        context = block.has_else
                            ? Context::FalseWrite : Context::TrueWrite;
                    } else {
                        parsed.insert(parsed.end(),
                                      actions.begin(), actions.end());
                        context = Context::TrueWrite;
                    }
                    first_segment = false;
                    continue;
                }

                std::string_view named;
                std::size_t named_prefix = 0U;
                for (const std::string_view candidate :
                     {std::string_view{"ADD"}, std::string_view{"SUB"},
                      std::string_view{"PTR"}, std::string_view{"FILL"},
                      std::string_view{"SLIDE"}}) {
                    const std::string eq = std::string(candidate) + "=";
                    const std::string colon = std::string(candidate) + ":";
                    if (segment.rfind(eq, 0U) == 0U ||
                        segment.rfind(colon, 0U) == 0U) {
                        named = candidate;
                        named_prefix = candidate.size() + 1U;
                        break;
                    }
                }
                if (!named.empty()) {
                    const auto operation = parse_named_runtime_operation(
                        named, segment.substr(named_prefix), document.warnings,
                        line_number, full_line);
                    if (!operation) return false;
                    if (block.open) {
                        auto& destination = block.has_else
                            ? block.false_actions : block.true_actions;
                        destination.push_back(*operation);
                    } else {
                        parsed.push_back(*operation);
                    }
                    context = Context::None;
                    first_segment = false;
                    continue;
                }

                if (first_segment && condition_kind_for_key(first_key)) {
                    if (!start_condition(first_key, segment)) return false;
                    first_segment = false;
                    continue;
                }
                if (first_segment &&
                    (first_key == "ADD" || first_key == "SUB" ||
                     first_key == "PTR" || first_key == "FILL" ||
                     first_key == "SLIDE")) {
                    const auto operation = parse_named_runtime_operation(
                        first_key, segment, document.warnings,
                        line_number, full_line);
                    if (!operation) return false;
                    parsed.push_back(*operation);
                    context = Context::None;
                    first_segment = false;
                    continue;
                }

                if (context == Context::Condition && block.open) {
                    const auto runs = parse_payload_runs(
                        segment, true, document.warnings, line_number);
                    if (!runs) return false;
                    block.conditions.insert(block.conditions.end(),
                                            runs->begin(), runs->end());
                } else {
                    std::vector<Operation> actions;
                    if (!parse_action_segments(segment, line_number,
                                               full_line, false, actions)) {
                        return false;
                    }
                    if (block.open) {
                        auto& destination = block.has_else
                            ? block.false_actions : block.true_actions;
                        destination.insert(destination.end(),
                                           actions.begin(), actions.end());
                    } else {
                        parsed.insert(parsed.end(),
                                      actions.begin(), actions.end());
                    }
                }
                first_segment = false;
            }

            if (!finish_block()) return false;
            parsed.insert(parsed.end(), independent_rom.begin(),
                          independent_rom.end());
            if (parsed.empty()) return false;
            entry.operations.insert(entry.operations.end(),
                                    parsed.begin(), parsed.end());
            return true;
        };

    const auto parse_key_line =
        [&](std::string logical_line, std::size_t line_number) {
            logical_line = remove_spaces_and_tabs(logical_line);
            if (logical_line.empty()) return;

            CheatEntry& entry = ensure_current();
            const std::size_t operations_before = entry.operations.size();

            if (logical_line.rfind("ROM=", 0U) == 0U) {
                const auto runs = parse_rom_payload_runs(
                    std::string_view(logical_line).substr(4U),
                    document.warnings, line_number);
                if (runs) {
                    auto patches = make_rom_patch_operations(
                        *runs, line_number, logical_line);
                    entry.operations.insert(entry.operations.end(),
                                            patches.begin(), patches.end());
                    return;
                }
            } else if (logical_line.rfind("ROMIF=", 0U) == 0U) {
                if (parse_canonical_romif(
                        entry, std::string_view(logical_line).substr(6U),
                        line_number, logical_line)) {
                    return;
                }
            } else {
                const std::size_t equals = logical_line.find('=');
                if (equals != std::string::npos) {
                    const std::string_view key =
                        std::string_view(logical_line).substr(0U, equals);
                    if (key == "ON" || condition_kind_for_key(key) ||
                        key == "ADD" || key == "SUB" || key == "PTR" ||
                        key == "FILL" || key == "SLIDE") {
                        if (parse_runtime_option(
                                entry, key, logical_line, line_number)) {
                            return;
                        }
                    }
                }
            }

            entry.operations.resize(operations_before);
            document.warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": the complete Enhanced entry was discarded");
        };

    const std::vector<std::string> lines = text::split_lines(input);
    bool in_block_comment = false;
    std::string pending_key;
    std::size_t pending_line = 0U;

    const auto flush_pending = [&]() {
        if (!pending_key.empty()) {
            parse_key_line(pending_key, pending_line);
            pending_key.clear();
            pending_line = 0U;
        }
    };

    for (std::size_t line_index = 0U;
         line_index < lines.size();
         ++line_index) {
        const std::size_t line_number = line_index + 1U;
        std::string line = text::trim(lines[line_index]);

        if (in_block_comment) {
            if (line.find("*/") != std::string::npos) {
                in_block_comment = false;
            }
            continue;
        }

        if (line.rfind("--", 0U) == 0U) {
            flush_pending();
            break;
        }

        if (line.find("/*") != std::string::npos) {
            flush_pending();
            in_block_comment = true;
            continue;
        }
        if (line.find("*/") != std::string::npos) {
            flush_pending();
            continue;
        }

        if (line.empty() || line.front() == '#' ||
            line.front() == '=' || line.front() == '/' ||
            line.front() == ';') {
            flush_pending();
            continue;
        }

        if (line.size() >= 2U &&
            line.front() == '[' && line.back() == ']') {
            flush_pending();
            std::string name = text::trim(
                std::string_view(line).substr(1U, line.size() - 2U));
            if (name.empty()) {
                name = "Converted EZ-Flash Code";
            }
            document.entries.push_back(CheatEntry{name, {}});
            current = &document.entries.back();
            continue;
        }

        std::string compact = remove_spaces_and_tabs(line);
        std::string key_candidate = compact;
        const std::size_t comment = key_candidate.find('#');
        if (comment != std::string::npos) {
            key_candidate.resize(comment);
        }

        const bool starts_key =
            key_candidate.rfind("ON=", 0U) == 0U ||
            key_candidate.rfind("IF=", 0U) == 0U ||
            key_candidate.rfind("IFNE=", 0U) == 0U ||
            key_candidate.rfind("IFLT=", 0U) == 0U ||
            key_candidate.rfind("IFGT=", 0U) == 0U ||
            key_candidate.rfind("IFLE=", 0U) == 0U ||
            key_candidate.rfind("IFGE=", 0U) == 0U ||
            key_candidate.rfind("ADD=", 0U) == 0U ||
            key_candidate.rfind("SUB=", 0U) == 0U ||
            key_candidate.rfind("PTR=", 0U) == 0U ||
            key_candidate.rfind("FILL=", 0U) == 0U ||
            key_candidate.rfind("SLIDE=", 0U) == 0U ||
            key_candidate.rfind("ROM=", 0U) == 0U ||
            key_candidate.rfind("ROMIF=", 0U) == 0U;
        if (starts_key) {
            flush_pending();
            pending_key = std::move(key_candidate);
            pending_line = line_number;
            continue;
        }

        if (!pending_key.empty()) {
            if (compact.find('=') != std::string::npos) {
                flush_pending();
                parse_key_line(key_candidate, line_number);
            } else {
                pending_key += compact;
            }
            continue;
        }

        parse_key_line(key_candidate, line_number);
    }

    flush_pending();
    return document;
}

} // namespace gba::ezflash::parse_detail
