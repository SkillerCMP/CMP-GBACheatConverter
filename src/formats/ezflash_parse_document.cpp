#include "formats/ezflash_parse_internal.hpp"

#include "core/text.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gba::ezflash::parse_detail {

std::string remove_spaces_and_tabs(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (const char c : input) {
        if (c != ' ' && c != '\t') result.push_back(c);
    }
    return result;
}

namespace {

struct ConditionFrame {
    std::size_t operation_index = 0U;
    std::size_t branch_start = 0U;
    std::size_t false_start = 0U;
    bool has_else = false;
};

std::optional<std::pair<std::string_view, std::string_view>> split_command(
    std::string_view segment) {
    const std::size_t colon = segment.find(':');
    if (colon == std::string_view::npos || colon == 0U ||
        colon + 1U >= segment.size() ||
        segment.find('=') != std::string_view::npos) {
        return std::nullopt;
    }
    return std::make_pair(segment.substr(0U, colon),
                          segment.substr(colon + 1U));
}

bool parse_original_value(CheatEntry& entry,
                          std::string_view value,
                          std::vector<std::string>& warnings,
                          std::size_t line_number,
                          std::string_view source_text) {
    const auto runs = parse_payload_runs(value, false, warnings, line_number);
    if (!runs) return false;
    auto operations = make_write_operations(*runs, line_number, source_text);
    if (operations.empty()) return false;
    entry.operations.insert(entry.operations.end(),
                            operations.begin(), operations.end());
    return true;
}

bool parse_enhanced_value(CheatEntry& entry,
                          std::string_view value,
                          std::vector<std::string>& warnings,
                          std::size_t line_number,
                          std::string_view source_text) {
    std::vector<ConditionFrame> stack;
    std::vector<std::size_t> rom_guards;
    std::vector<Operation> operations;

    for (const std::string& raw_segment : split_semicolon_runs(value)) {
        const std::string segment = remove_spaces_and_tabs(raw_segment);
        if (segment == "ELSE") {
            if (stack.empty() || stack.back().has_else ||
                operations.size() == stack.back().branch_start) {
                return false;
            }
            ConditionFrame& frame = stack.back();
            Operation& condition = operations[frame.operation_index];
            condition.condition_span = static_cast<std::uint32_t>(
                operations.size() - frame.branch_start);
            condition.condition_has_else = true;
            frame.has_else = true;
            frame.false_start = operations.size();
            continue;
        }
        if (segment == "ENDIF") {
            if (stack.empty()) return false;
            ConditionFrame frame = stack.back();
            stack.pop_back();
            Operation& condition = operations[frame.operation_index];
            if (frame.has_else) {
                if (operations.size() == frame.false_start) return false;
                condition.condition_else_span = static_cast<std::uint32_t>(
                    operations.size() - frame.false_start);
            } else {
                if (operations.size() == frame.branch_start) return false;
                condition.condition_span = static_cast<std::uint32_t>(
                    operations.size() - frame.branch_start);
            }
            continue;
        }

        const auto command = split_command(segment);
        if (!command) return false;
        const std::string_view key = command->first;
        const std::string_view payload = command->second;

        if (condition_kind_for_key(key)) {
            const auto operation = parse_width_condition_operation(
                key, payload, warnings, line_number, source_text);
            if (!operation) return false;
            operations.push_back(*operation);
            stack.push_back(ConditionFrame{
                operations.size() - 1U, operations.size(), 0U, false});
            continue;
        }

        if (key == "ROMIF") {
            if (!stack.empty() || operations.size() != rom_guards.size()) {
                warnings.push_back(
                    "EZ-Flash line " + std::to_string(line_number) +
                    ": ROMIF must appear before runtime actions");
                return false;
            }
            const auto runs = parse_rom_run(payload, warnings, line_number);
            if (!runs) return false;
            const auto terms = make_condition_terms(*runs);
            if (terms.empty()) return false;
            Operation condition;
            condition.kind = OperationKind::IfEqual;
            condition.address = terms.front().address;
            condition.value = terms.front().value;
            condition.width = terms.front().width;
            condition.condition_span = 0U;
            condition.condition_terms.assign(terms.begin() + 1U, terms.end());
            condition.source_line = line_number;
            condition.source_text = std::string(source_text);
            condition.note = "EZ-Flash Enhanced ROM signature guard";
            operations.push_back(std::move(condition));
            rom_guards.push_back(operations.size() - 1U);
            continue;
        }

        if (key == "ROM") {
            if (!stack.empty()) {
                warnings.push_back(
                    "EZ-Flash line " + std::to_string(line_number) +
                    ": ROM cannot appear inside a runtime IF branch");
                return false;
            }
            const auto runs = parse_rom_run(payload, warnings, line_number);
            if (!runs) return false;
            auto patches = make_rom_patch_operations(
                *runs, line_number, source_text);
            operations.insert(operations.end(),
                              patches.begin(), patches.end());
            continue;
        }

        std::optional<Operation> operation;
        if (key == "W8" || key == "W16" || key == "W32") {
            operation = parse_width_write_operation(
                key, payload, warnings, line_number, source_text);
        } else if (key == "ADD" || key == "SUB" || key == "PTR" ||
                   key == "FILL" || key == "SLIDE") {
            operation = parse_named_runtime_operation(
                key, payload, warnings, line_number, source_text);
        }
        if (!operation) return false;
        operations.push_back(*operation);
    }

    if (!stack.empty() || operations.empty()) return false;
    for (const std::size_t index : rom_guards) {
        if (index + 1U >= operations.size()) return false;
        operations[index].condition_span = static_cast<std::uint32_t>(
            operations.size() - index - 1U);
    }
    entry.operations.insert(entry.operations.end(),
                            operations.begin(), operations.end());
    return true;
}

} // namespace

CheatDocument parse_document(std::string_view input) {
    CheatDocument document;
    const std::vector<std::string> lines = text::split_lines(input);
    std::string current_section;
    EzFlashGroupMode current_group_mode = EzFlashGroupMode::None;
    std::string pending_option;
    std::size_t pending_line = 0U;
    bool in_block_comment = false;

    const auto parse_option = [&](std::string logical_line,
                                  std::size_t line_number) {
        const std::size_t comment = logical_line.find('#');
        if (comment != std::string::npos) logical_line.resize(comment);
        logical_line = text::trim(logical_line);
        if (logical_line.empty()) return;

        const std::size_t equals = logical_line.find('=');
        if (equals == std::string::npos || equals == 0U ||
            equals + 1U >= logical_line.size()) {
            document.warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": expected Code Name=command:value");
            return;
        }
        const std::string option_name = text::trim(
            std::string_view(logical_line).substr(0U, equals));
        const std::string value = remove_spaces_and_tabs(
            std::string_view(logical_line).substr(equals + 1U));
        if (option_name.empty() || value.empty()) return;

        CheatEntry entry;
        if (current_section.empty()) {
            entry.name = option_name;
            entry.ezflash_group_mode = EzFlashGroupMode::None;
        } else {
            entry.name = current_section + " - " + option_name;
            entry.ezflash_group_name = current_section;
            entry.ezflash_option_name = option_name;
            entry.ezflash_group_mode = current_group_mode;
        }
        const bool enhanced = value.find(':') != std::string::npos;
        const bool parsed = enhanced
            ? parse_enhanced_value(entry, value, document.warnings,
                                   line_number, logical_line)
            : parse_original_value(entry, value, document.warnings,
                                   line_number, logical_line);
        if (!parsed) {
            document.warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": the complete code '" + option_name + "' was discarded");
            return;
        }
        document.entries.push_back(std::move(entry));
    };

    const auto flush_pending = [&]() {
        if (!pending_option.empty()) {
            parse_option(pending_option, pending_line);
            pending_option.clear();
            pending_line = 0U;
        }
    };

    for (std::size_t line_index = 0U; line_index < lines.size(); ++line_index) {
        const std::size_t line_number = line_index + 1U;
        std::string line = text::trim(lines[line_index]);

        if (in_block_comment) {
            if (line.find("*/") != std::string::npos) in_block_comment = false;
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
        if (line.empty() || line.front() == '#' || line.front() == '/' ||
            line.front() == ';') {
            continue;
        }
        if (line.size() >= 2U && line.front() == '[' && line.back() == ']') {
            flush_pending();
            std::string section = text::trim(
                std::string_view(line).substr(1U, line.size() - 2U));
            current_group_mode = EzFlashGroupMode::MultiSelect;
            constexpr std::string_view one_suffix = "|ONE";
            if (section.size() >= one_suffix.size() &&
                section.compare(section.size() - one_suffix.size(),
                                one_suffix.size(), one_suffix) == 0) {
                section.resize(section.size() - one_suffix.size());
                section = text::trim(section);
                current_group_mode = EzFlashGroupMode::ZeroOrOne;
            }
            if (section.empty()) {
                document.warnings.push_back(
                    "EZ-Flash line " + std::to_string(line_number) +
                    ": empty group heading was ignored");
                current_section = "Converted EZ-Flash Code";
            } else {
                current_section = std::move(section);
            }
            continue;
        }

        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) line.resize(comment);
        line = text::trim(line);
        if (line.empty()) continue;

        if (line.find('=') != std::string::npos) {
            flush_pending();
            pending_option = line;
            pending_line = line_number;
        } else if (!pending_option.empty()) {
            pending_option += remove_spaces_and_tabs(line);
        } else {
            document.warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": continuation row has no code header");
        }
    }
    flush_pending();

    const auto group_key = [](const CheatEntry& entry) {
        return entry.ezflash_group_name +
            (entry.ezflash_group_mode == EzFlashGroupMode::ZeroOrOne
                ? "\x1fONE" : "\x1fMULTI");
    };
    std::unordered_map<std::string, std::size_t> group_counts;
    for (const CheatEntry& entry : document.entries) {
        if (!entry.ezflash_group_name.empty()) {
            ++group_counts[group_key(entry)];
        }
    }
    for (CheatEntry& entry : document.entries) {
        if (!entry.ezflash_group_name.empty() &&
            group_counts[group_key(entry)] == 1U &&
            entry.ezflash_option_name == "ON") {
            entry.name = entry.ezflash_group_name;
        }
    }
    return document;
}

} // namespace gba::ezflash::parse_detail
