#pragma once

#include "export/output_modes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {

using Lines8x4 =
    std::vector<std::pair<std::uint32_t, std::uint16_t>>;
using Lines8x8 =
    std::vector<std::pair<std::uint32_t, std::uint32_t>>;

struct ZipItem {
    std::string name;
    std::string data;
};

std::string single_line_name(std::string_view raw);
std::string xml_escape(std::string_view raw);
std::string quote_escape(std::string_view raw);

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value);
void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value);
void patch_u32(std::vector<std::uint8_t>& out,
               std::size_t offset,
               std::uint32_t value);
void append_bytes(std::vector<std::uint8_t>& out, std::string_view value);
std::vector<std::uint8_t> bytes_from_text(std::string_view value);

CheatDocument one_entry_document(const CheatEntry& entry);
std::vector<std::string> code_lines(std::string_view output, bool want8x8);
std::optional<Lines8x4> exact_fcd(const CheatEntry& entry);
std::optional<Lines8x8> exact_armax(const CheatEntry& entry, bool encrypted);
std::string format_8x4(const Lines8x4& lines);
std::string format_8x8(const Lines8x8& lines);
void warn_omitted(Result& result,
                  const CheatEntry& entry,
                  std::string_view target);
bool direct_writes_only(const CheatEntry& entry);
std::string lower_hex(std::uint32_t value);

std::vector<std::uint8_t> store_zip(const std::vector<ZipItem>& items);

Result export_ezflash(const CheatDocument& document, const Options& options);
Result export_myboy(const CheatDocument& document);
Result export_mgba(const CheatDocument& document);
Result export_libretro(const CheatDocument& document);
Result export_mednafen(const CheatDocument& document, const Options& options);
Result export_armax_dsc(const CheatDocument& document, const Options& options);
Result export_vba_clt(const CheatDocument& document);
Result export_mister(const CheatDocument& document);

} // namespace gba::output_modes::detail
