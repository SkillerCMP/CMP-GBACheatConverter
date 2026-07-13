#pragma once

#include "core/types.hpp"
#include "import/native_input.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::native_input::detail {

struct NativeEntry {
    std::string name;
    InputFormat format = InputFormat::FcdRaw;
    std::vector<std::string> code_lines;
};

std::optional<std::uint16_t> read_u16(
    const std::vector<std::uint8_t>& data, std::size_t offset);
std::optional<std::uint32_t> read_u32(
    const std::vector<std::uint8_t>& data, std::size_t offset);
std::string fixed_text(const std::vector<std::uint8_t>& data,
                       std::size_t offset,
                       std::size_t size,
                       bool trim_spaces = true);
std::string filename_extension(std::string_view filename);
std::string bytes_as_text(const std::vector<std::uint8_t>& data);
bool has_nul_byte(const std::vector<std::uint8_t>& data);
bool equals_ascii(const std::vector<std::uint8_t>& data,
                  std::size_t offset,
                  std::string_view value);
std::string xml_unescape(std::string_view value);
std::string quoted_unescape(std::string_view value);
std::string normalized_entry_text(const NativeEntry& entry);

CheatDocument parse_entry(const NativeEntry& entry);
Result finish_entries(SourceFormat source_format,
                      std::string source_name,
                      const std::vector<NativeEntry>& entries,
                      const std::vector<InputFormat>& mixed_preferences);
Result render_document(SourceFormat source_format,
                       std::string source_name,
                       const CheatDocument& document,
                       const std::vector<InputFormat>& preferences);
Result recognized_error(SourceFormat source_format,
                        std::string source_name,
                        std::string warning);

Result import_armax_dsc(std::string_view filename,
                        const std::vector<std::uint8_t>& data);
Result import_vba_clt(std::string_view filename,
                      const std::vector<std::uint8_t>& data);
Result import_myboy(std::string_view filename, std::string_view text);
Result import_mgba(std::string_view filename, std::string_view text);
Result import_libretro(std::string_view filename, std::string_view text);
Result import_mednafen(std::string_view filename, std::string_view text);
Result import_ezflash(std::string_view filename, std::string_view text);
Result import_mister(std::string_view filename,
                     const std::vector<std::uint8_t>& data);

bool looks_like_myboy(std::string_view text);
bool looks_like_mgba(std::string_view text);
bool looks_like_libretro(std::string_view text);
bool looks_like_mednafen(std::string_view text);
bool looks_like_ezflash(std::string_view text);

} // namespace gba::native_input::detail
