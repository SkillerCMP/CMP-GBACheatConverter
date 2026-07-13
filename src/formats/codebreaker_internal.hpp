#pragma once

#include "formats/codebreaker.hpp"

namespace gba::codebreaker::detail {

enum class PendingKind {
    None,
    Slide,
    PackedList
};

struct PendingOperation {
    PendingKind kind = PendingKind::None;
    std::size_t operation_index = 0;
    std::uint32_t remaining_values = 0;
    std::uint32_t next_address = 0;
    std::size_t header_line = 0;
    std::size_t first_operation_index = 0;
    std::optional<std::size_t> controlling_condition_index;
};

std::optional<RawLine> parse_line(std::string_view raw,
                                  std::size_t line_number);
std::uint16_t byte_swap_16(std::uint16_t value);
bool is_condition_kind(OperationKind kind);
void warn_incomplete_pending(CheatDocument& document,
                             const PendingOperation& pending);
void decode_line(CheatEntry& entry,
                 CheatDocument& document,
                 const RawLine& line,
                 PendingOperation& pending);

CheatDocument parse_document(std::string_view input,
                             const ParseOptions& options);
Result export_document_impl(const CheatDocument& document,
                            const ExportOptions& options);

} // namespace gba::codebreaker::detail
