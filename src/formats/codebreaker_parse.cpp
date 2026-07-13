#include "formats/codebreaker_internal.hpp"

#include "core/text.hpp"

#include <string>

namespace gba::codebreaker::detail {

std::optional<RawLine> parse_line(std::string_view raw, std::size_t line_number) {
    const std::string line = text::trim(raw);
    if (!text::is_code_line_8x4(line)) {
        return std::nullopt;
    }

    const auto op1 = text::parse_hex_u32(std::string_view(line).substr(0, 8));
    const auto op2 = text::parse_hex_u16(std::string_view(line).substr(9, 4));
    if (!op1 || !op2) {
        return std::nullopt;
    }

    return RawLine{*op1, *op2, line_number, line};
}

namespace {

std::string clean_name(std::string line) {
    line = text::trim(line);
    if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
        return line.substr(1, line.size() - 2);
    }
    if (!line.empty() && line.back() == ':') {
        line.pop_back();
        return text::trim(line);
    }
    return line;
}

} // namespace

CheatDocument parse_document(std::string_view input, const ParseOptions& options) {
    CheatDocument document;
    CheatEntry* current = nullptr;
    Cipher cipher;
    PendingOperation pending;

    if (options.encrypted && options.force_seed && options.seed) {
        cipher.reseed(*options.seed);
    }

    const std::string named_input =
        text::normalize_plain_cheat_headers(input);
    const auto lines = text::split_lines(named_input);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string line = text::trim(lines[index]);
        if (line.empty()) {
            continue;
        }

        const auto parsed = parse_line(line, index + 1);
        if (!parsed) {
            const bool bracket_name =
                line.size() >= 2 && line.front() == '[' && line.back() == ']';
            const bool colon_name = !line.empty() && line.back() == ':';
            const bool metadata_name =
                text::is_inline_metadata_name_line(line);

            if (bracket_name || colon_name || metadata_name) {
                warn_incomplete_pending(document, pending);
                pending = {};
                document.entries.push_back(CheatEntry{clean_name(line), {}});
                current = &document.entries.back();
            }
            continue;
        }

        RawLine decoded = *parsed;

        if (options.encrypted) {
            if (!cipher.active()) {
                // The first plaintext 9-type row is the normal stream seed.
                // A supplied input seed is only a fallback for a keyless list.
                if ((parsed->op1 >> 28U) == 0x9U) {
                    const Seed embedded{parsed->op1, parsed->op2};
                    if (options.seed &&
                        (options.seed->op1 != embedded.op1 ||
                         options.seed->op2 != embedded.op2)) {
                        document.warnings.push_back(
                            "Embedded CodeBreaker key " +
                            format_seed(embedded, ' ') +
                            " overrides supplied input key " +
                            format_seed(*options.seed, ' ') +
                            " at source line " +
                            std::to_string(parsed->source_line));
                    }
                    cipher.reseed(embedded);
                    continue;
                }

                if (!options.seed) {
                    document.warnings.push_back(
                        "Encrypted CodeBreaker line before a 9-type key at source line " +
                        std::to_string(parsed->source_line));
                    continue;
                }
                cipher.reseed(*options.seed);
            }

            decoded = cipher.decrypt(*parsed);
            if (pending.kind == PendingKind::None &&
                (decoded.op1 >> 28U) == 0x9U) {
                cipher.reseed(Seed{decoded.op1, decoded.op2});
                continue;
            }
        } else if (pending.kind == PendingKind::None &&
                   (parsed->op1 >> 28U) == 0x9U) {
            cipher.reseed(Seed{parsed->op1, parsed->op2});
            continue;
        }

        if (!current) {
            document.entries.push_back(CheatEntry{"Converted Code", {}});
            current = &document.entries.back();
        }

        decode_line(*current, document, decoded, pending);
    }

    warn_incomplete_pending(document, pending);

    return document;
}

} // namespace gba::codebreaker::detail
