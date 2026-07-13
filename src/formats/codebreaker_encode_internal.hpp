#pragma once

#include "formats/codebreaker_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace gba::codebreaker::detail {

struct EncodeState {
    explicit EncodeState(const ExportOptions& export_options)
        : options(export_options) {}

    ExportOptions options;
    Result result;
    std::ostringstream output;
    Cipher cipher;
};

std::size_t packed_write_run_length(const CheatEntry& entry,
                                    std::size_t first_index,
                                    std::size_t maximum_count,
                                    std::size_t minimum_count,
                                    bool allow_preceding_condition);

std::optional<std::pair<std::uint32_t, std::uint16_t>>
encode_condition_words(const Operation& operation);

bool hook_fits_fcd(const Operation& operation);
bool can_encode_fcd_action(const Operation& operation);

void emit_line(EncodeState& state,
               std::uint32_t op1,
               std::uint16_t op2,
               std::ostringstream& destination);

void warn_unsupported(EncodeState& state,
                      const CheatEntry& entry,
                      const Operation& operation,
                      const std::string& reason);

void emit_packed_list(EncodeState& state,
                      const CheatEntry& entry,
                      std::size_t first_index,
                      std::size_t packed_count,
                      std::ostringstream& destination);

void encode_condition_operation(EncodeState& state,
                                const CheatEntry& entry,
                                std::size_t& operation_index,
                                std::ostringstream& entry_output,
                                bool& has_output);

void encode_regular_operation(EncodeState& state,
                              const CheatEntry& entry,
                              std::size_t& operation_index,
                              std::ostringstream& entry_output,
                              bool& has_output);

Result encode_document(const CheatDocument& document,
                       const ExportOptions& options);

} // namespace gba::codebreaker::detail
