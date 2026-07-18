#pragma once

#include "formats/ezflash.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::ezflash::parse_detail {

struct ParsedRun {
    std::uint32_t address = 0;
    std::vector<std::uint8_t> bytes;
};

std::optional<std::uint32_t> expand_compact_address(
    std::uint32_t compact,
    bool condition);
std::vector<std::string> split_semicolon_runs(std::string_view payload);
std::optional<std::vector<ParsedRun>> parse_run(
    std::string_view raw,
    bool condition,
    std::vector<std::string>& warnings,
    std::size_t line_number);
std::optional<std::vector<ParsedRun>> parse_rom_run(
    std::string_view raw,
    std::vector<std::string>& warnings,
    std::size_t line_number);
std::optional<std::vector<ParsedRun>> parse_rom_payload_runs(
    std::string_view payload,
    std::vector<std::string>& warnings,
    std::size_t line_number);
std::uint32_t little_endian_value(
    const std::vector<std::uint8_t>& bytes,
    std::size_t first,
    std::size_t count);
void append_write_run(CheatEntry& entry,
                      const ParsedRun& run,
                      std::size_t source_line,
                      std::string_view source_text);
std::vector<Operation> make_write_operations(
    const std::vector<ParsedRun>& runs,
    std::size_t source_line,
    std::string_view source_text);
void append_rom_patch_run(CheatEntry& entry,
                          const ParsedRun& run,
                          std::size_t source_line,
                          std::string_view source_text);
std::vector<Operation> make_rom_patch_operations(
    const std::vector<ParsedRun>& runs,
    std::size_t source_line,
    std::string_view source_text);
std::optional<std::vector<ParsedRun>> parse_payload_runs(
    std::string_view payload,
    bool condition,
    std::vector<std::string>& warnings,
    std::size_t line_number);
std::vector<ConditionTerm> make_condition_terms(
    const std::vector<ParsedRun>& runs);
std::optional<OperationKind> condition_kind_for_key(std::string_view key);
bool condition_key_is_masked(std::string_view key);
Operation make_payload_operation(OperationKind kind,
                                 std::uint32_t address,
                                 std::vector<std::uint8_t> bytes,
                                 std::size_t source_line,
                                 std::string_view source_text,
                                 std::string_view note);
std::optional<std::vector<std::string>> split_csv_tokens(
    std::string_view payload);
std::optional<std::vector<std::uint8_t>> parse_byte_tokens(
    const std::vector<std::string>& tokens,
    std::size_t first,
    std::vector<std::string>& warnings,
    std::size_t line_number,
    std::string_view label);
std::optional<Operation> parse_width_write_operation(
    std::string_view name,
    std::string_view payload,
    std::vector<std::string>& warnings,
    std::size_t line_number,
    std::string_view source_text);
std::optional<Operation> parse_width_condition_operation(
    std::string_view name,
    std::string_view payload,
    std::vector<std::string>& warnings,
    std::size_t line_number,
    std::string_view source_text);
std::optional<Operation> parse_named_runtime_operation(
    std::string_view name,
    std::string_view payload,
    std::vector<std::string>& warnings,
    std::size_t line_number,
    std::string_view source_text);
std::string remove_spaces_and_tabs(std::string_view input);
CheatDocument parse_document(std::string_view input);

} // namespace gba::ezflash::parse_detail
