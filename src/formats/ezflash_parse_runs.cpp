#include "formats/ezflash_parse_internal.hpp"

#include "formats/ezflash_internal.hpp"

#include "core/text.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::ezflash::parse_detail {

using detail::canonical_rom_address;

std::optional<std::uint32_t> expand_compact_address(std::uint32_t compact,
                                                    bool condition) {
    if (compact <= 0x3FFFFU) {
        return 0x02000000U + compact;
    }
    if (compact >= 0x40000U && compact <= 0x47FFFU) {
        return 0x03000000U + (compact - 0x40000U);
    }
    if (condition && compact >= 0x80000U && compact <= 0x803FFU) {
        return 0x04000000U + (compact - 0x80000U);
    }
    return std::nullopt;
}

std::vector<std::string> split_semicolon_runs(std::string_view payload) {
    std::vector<std::string> runs;
    std::size_t start = 0;
    while (start < payload.size()) {
        const std::size_t end = payload.find(';', start);
        const std::size_t count = end == std::string_view::npos
            ? payload.size() - start
            : end - start;
        const std::string run = text::trim(payload.substr(start, count));
        if (!run.empty()) {
            runs.push_back(run);
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
    }
    return runs;
}

std::optional<std::vector<ParsedRun>> parse_run(
    std::string_view raw,
    bool condition,
    std::vector<std::string>& warnings,
    std::size_t line_number) {
    const std::string run = text::trim(raw);
    const std::size_t first_comma = run.find(',');
    if (first_comma == std::string::npos) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": missing byte list after compact address");
        return std::nullopt;
    }

    const auto compact = text::parse_hex_u32(
        std::string_view(run).substr(0, first_comma));
    if (!compact) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": invalid compact address");
        return std::nullopt;
    }

    std::vector<std::uint8_t> bytes;
    std::size_t start = first_comma + 1U;
    while (start <= run.size()) {
        const std::size_t end = run.find(',', start);
        const std::size_t count = end == std::string::npos
            ? run.size() - start
            : end - start;
        const std::string token = text::trim(
            std::string_view(run).substr(start, count));
        const auto byte = text::parse_hex_u32(token);
        if (!byte || *byte > 0xFFU) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": invalid byte value '" + token + "'");
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>(*byte));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1U;
    }

    if (bytes.empty()) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": empty byte list");
        return std::nullopt;
    }

    std::vector<ParsedRun> segments;
    for (std::size_t offset = 0U; offset < bytes.size(); ++offset) {
        const std::uint64_t compact_address =
            static_cast<std::uint64_t>(*compact) + offset;
        if (compact_address > 0xFFFFFFFFULL) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": compact address overflow");
            return std::nullopt;
        }

        const auto full = expand_compact_address(
            static_cast<std::uint32_t>(compact_address), condition);
        if (!full) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": compact byte address " +
                text::hex(static_cast<std::uint32_t>(compact_address), 1) +
                " is outside the supported " +
                std::string(condition ? "condition" : "write") + " ranges");
            return std::nullopt;
        }

        if (segments.empty() ||
            *full != segments.back().address +
                         static_cast<std::uint32_t>(
                             segments.back().bytes.size())) {
            segments.push_back(ParsedRun{*full, {}});
        }
        segments.back().bytes.push_back(bytes[offset]);
    }

    return segments;
}


std::optional<std::vector<ParsedRun>> parse_rom_run(
    std::string_view raw,
    std::vector<std::string>& warnings,
    std::size_t line_number) {
    const std::string run = text::trim(raw);
    const std::size_t first_comma = run.find(',');
    if (first_comma == std::string::npos) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": missing ROM byte list after address");
        return std::nullopt;
    }

    const auto address = text::parse_hex_u32(
        std::string_view(run).substr(0U, first_comma));
    if (!address) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": invalid ROM address");
        return std::nullopt;
    }
    const auto canonical = canonical_rom_address(*address);
    if (!canonical) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": ROM address is outside the Enhanced v3 image ranges");
        return std::nullopt;
    }

    ParsedRun parsed;
    parsed.address = *canonical;
    std::size_t start = first_comma + 1U;
    while (start <= run.size()) {
        const std::size_t end = run.find(',', start);
        const std::size_t count = end == std::string::npos
            ? run.size() - start
            : end - start;
        const std::string token = text::trim(
            std::string_view(run).substr(start, count));
        const auto byte = text::parse_hex_u32(token);
        if (!byte || *byte > 0xFFU) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": invalid ROM byte value '" + token + "'");
            return std::nullopt;
        }
        const std::uint64_t current =
            static_cast<std::uint64_t>(parsed.address) + parsed.bytes.size();
        if (current > 0x0BFFFFFFULL) {
            warnings.push_back(
                "EZ-Flash line " + std::to_string(line_number) +
                ": ROM byte-list address overflow");
            return std::nullopt;
        }
        parsed.bytes.push_back(static_cast<std::uint8_t>(*byte));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1U;
    }

    if (parsed.bytes.empty()) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": empty ROM byte list");
        return std::nullopt;
    }
    return std::vector<ParsedRun>{std::move(parsed)};
}

std::optional<std::vector<ParsedRun>> parse_rom_payload_runs(
    std::string_view payload,
    std::vector<std::string>& warnings,
    std::size_t line_number) {
    std::vector<ParsedRun> parsed;
    for (const std::string& run : split_semicolon_runs(payload)) {
        const auto value = parse_rom_run(run, warnings, line_number);
        if (!value) {
            return std::nullopt;
        }
        parsed.insert(parsed.end(), value->begin(), value->end());
    }
    if (parsed.empty()) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": empty ROM byte-list payload");
        return std::nullopt;
    }
    return parsed;
}

std::uint32_t little_endian_value(const std::vector<std::uint8_t>& bytes,
                                  std::size_t first,
                                  std::size_t count) {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < count; ++index) {
        value |= static_cast<std::uint32_t>(bytes[first + index])
                 << (index * 8U);
    }
    return value;
}

void append_write_run(CheatEntry& entry,
                      const ParsedRun& run,
                      std::size_t source_line,
                      std::string_view source_text) {
    std::size_t offset = 0;
    while (offset < run.bytes.size()) {
        const std::size_t remaining = run.bytes.size() - offset;
        const std::uint8_t width = remaining >= 4U
            ? 4U
            : (remaining >= 2U ? 2U : 1U);

        Operation operation;
        operation.kind = OperationKind::Write;
        operation.address = run.address + static_cast<std::uint32_t>(offset);
        operation.value = little_endian_value(run.bytes, offset, width);
        operation.width = width;
        operation.source_line = source_line;
        operation.source_text = std::string(source_text);
        operation.note = "EZ-Flash byte write";
        entry.operations.push_back(operation);
        offset += width;
    }
}

std::vector<Operation> make_write_operations(
    const std::vector<ParsedRun>& runs,
    std::size_t source_line,
    std::string_view source_text) {
    CheatEntry temporary;
    for (const ParsedRun& run : runs) {
        append_write_run(temporary, run, source_line, source_text);
    }
    return temporary.operations;
}


void append_rom_patch_run(CheatEntry& entry,
                          const ParsedRun& run,
                          std::size_t source_line,
                          std::string_view source_text) {
    std::size_t offset = 0U;
    while (offset < run.bytes.size()) {
        const std::size_t remaining = run.bytes.size() - offset;
        const std::uint8_t width = remaining >= 4U
            ? 4U
            : (remaining >= 2U ? 2U : 1U);

        Operation operation;
        operation.kind = OperationKind::RomPatch;
        operation.address = run.address + static_cast<std::uint32_t>(offset);
        operation.value = little_endian_value(run.bytes, offset, width);
        operation.width = width;
        operation.source_line = source_line;
        operation.source_text = std::string(source_text);
        operation.note = "EZ-Flash Enhanced ROM image patch";
        operation.encoding_hint = EncodingHint::EzFlashEnhancedRomPatch;
        entry.operations.push_back(operation);
        offset += width;
    }
}

std::vector<Operation> make_rom_patch_operations(
    const std::vector<ParsedRun>& runs,
    std::size_t source_line,
    std::string_view source_text) {
    CheatEntry temporary;
    for (const ParsedRun& run : runs) {
        append_rom_patch_run(temporary, run, source_line, source_text);
    }
    return temporary.operations;
}

std::optional<std::vector<ParsedRun>> parse_payload_runs(
    std::string_view payload,
    bool condition,
    std::vector<std::string>& warnings,
    std::size_t line_number) {
    std::vector<ParsedRun> parsed;
    const std::vector<std::string> runs = split_semicolon_runs(payload);
    for (const std::string& run : runs) {
        const auto value = parse_run(run, condition, warnings, line_number);
        if (!value) {
            return std::nullopt;
        }
        parsed.insert(parsed.end(), value->begin(), value->end());
    }
    if (parsed.empty()) {
        warnings.push_back(
            "EZ-Flash line " + std::to_string(line_number) +
            ": empty byte-list payload");
        return std::nullopt;
    }
    return parsed;
}

} // namespace gba::ezflash::parse_detail
