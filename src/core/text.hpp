#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::text {

std::string trim(std::string_view value);
std::string strip_utf8_bom(std::string_view value);

// Accepts Unix LF, classic Mac CR, Windows CRLF, and mixed input.
std::string normalize_newlines_lf(std::string_view value);
std::string normalize_newlines_crlf(std::string_view value);

// Canonicalizes code-only rows containing 12 hex digits to 8+4 and rows
// containing 16 hex digits to 8+8. A colon after the first 8 digits is
// accepted and replaced with a space. It also splits CodeTwink-style rows
// where the first valid code is physically attached to the end of a cheat
// name. It also reconstructs multiple 8+4 or 8+8 rows when a clipboard
// source has flattened all line breaks into spaces. Other non-code rows are
// preserved.
std::string format_compact_code_lines(std::string_view value);

// Converts GameHacking.org-style name/crypt blocks into inline metadata:
//   Name by Author
//   Device
// becomes:
//   Name , by Author , Crypt_Device
// Legacy underscore-only separator rows are removed.
std::string cleanup_gamehacking_org_blocks(std::string_view value);

// Promotes a plain cheat-name row to [Name] when its next non-empty row is
// an 8+4 or 8+8 code. This preserves user-friendly unbracketed names during
// conversion and native Save Output As reparsing. Existing headers, comments,
// metadata, and code rows are left unchanged.
std::string normalize_plain_cheat_headers(std::string_view value);

// Recognizes and preserves inline metadata headings in raw device output.
bool is_inline_metadata_name_line(std::string_view line);
std::string format_cheat_header(std::string_view name);

std::vector<std::string> split_lines(std::string_view value);
std::optional<std::uint32_t> parse_hex_u32(std::string_view value);
std::optional<std::uint16_t> parse_hex_u16(std::string_view value);
std::string hex(std::uint32_t value, unsigned width);
bool is_code_line_8x4(std::string_view line);
bool is_code_line_8x8(std::string_view line);

} // namespace gba::text
