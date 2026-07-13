#pragma once

#include "formats/gameshark.hpp"
#include "crypto/tea.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::gameshark::detail {

std::optional<RawLine> parse_line(std::string_view raw,
                                  std::size_t line_number);
std::string clean_name(std::string line);
Operation make_operation(OperationKind kind,
                         const RawLine& line,
                         std::uint32_t address,
                         std::uint32_t value,
                         std::uint8_t width,
                         std::string note = {});
void set_assignment_hint(Operation& operation,
                         std::uint32_t group,
                         std::uint32_t index,
                         std::uint32_t count);
RawLine decode_line(RawLine line,
                    bool encrypted,
                    const crypto::TeaKey& key);

std::size_t assignment_group_length(const CheatEntry& entry,
                                    std::size_t first,
                                    std::size_t maximum);
std::vector<RawLine> encode_assignment_group(const CheatEntry& entry,
                                             std::size_t first,
                                             std::size_t count);

bool is_condition_kind(OperationKind kind);
std::string format_line(const RawLine& line,
                        bool encrypted,
                        const crypto::TeaKey& key);

CheatDocument parse_document(std::string_view input,
                             const ParseOptions& options);
Result encode_document(const CheatDocument& document,
                       const ExportOptions& options);

} // namespace gba::gameshark::detail
