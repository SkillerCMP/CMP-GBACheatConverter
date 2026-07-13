#pragma once

#include "formats/armax_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gba::armax::detail {

using EncodedLine = std::pair<std::uint32_t, std::uint32_t>;
using EncodedLines = std::vector<EncodedLine>;

bool hook_fits_armax(const Operation& operation);
bool entry_has_invalid_hook(const CheatEntry& entry,
                            const Operation*& invalid_operation);
bool entry_has_unrepresentable_rom_patch(
    const CheatEntry& entry,
    const Operation*& invalid_operation);

class EntryEncoder {
public:
    EntryEncoder(const CheatEntry& entry, Result& result);

    bool encode_non_condition(const Operation& operation,
                              EncodedLines& destination);
    bool encode_range(std::size_t first,
                      std::size_t count,
                      EncodedLines& destination);
    EncodedLines encode_entry();

private:
    void unsupported(const Operation& operation, const std::string& reason);
    static std::uint32_t condition_code_bits(const Operation& operation);
    static bool button_write_fits(const Operation& operation);

    const CheatEntry& entry_;
    Result& result_;
};

Result export_encoded_document(const CheatDocument& document,
                               const ExportOptions& options);

} // namespace gba::armax::detail
