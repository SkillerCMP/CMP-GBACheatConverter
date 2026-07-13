#include "formats/codebreaker_internal.hpp"

#include "core/text.hpp"

#include <sstream>
#include <stdexcept>

namespace gba::codebreaker {

using detail::parse_line;

std::string format_raw(std::string_view input,
                       bool input_encrypted,
                       std::optional<Seed> output_seed) {
    std::ostringstream output;
    Cipher input_cipher;
    Cipher output_cipher;

    if (output_seed) {
        output_cipher.reseed(*output_seed);
        output << text::hex(output_seed->op1, 8) << ' '
               << text::hex(output_seed->op2, 4) << '\n';
    }

    const std::string named_input =
        text::normalize_plain_cheat_headers(input);
    const auto lines = text::split_lines(named_input);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string line = text::trim(lines[index]);
        const auto parsed = parse_line(line, index + 1);

        if (!parsed) {
            if (!line.empty()) {
                output << line << '\n';
            }
            continue;
        }

        RawLine raw = *parsed;

        if (input_encrypted) {
            if (!input_cipher.active()) {
                if ((parsed->op1 >> 28U) != 0x9U) {
                    throw std::runtime_error(
                        "Encrypted CodeBreaker stream is missing its 9-type key");
                }
                input_cipher.reseed(Seed{parsed->op1, parsed->op2});
                if (!output_seed) {
                    output << parsed->source_text << '\n';
                }
                continue;
            }

            raw = input_cipher.decrypt(*parsed);
            if ((raw.op1 >> 28U) == 0x9U) {
                input_cipher.reseed(Seed{raw.op1, raw.op2});
            }
        } else if ((parsed->op1 >> 28U) == 0x9U) {
            // A raw seed configures CodeBreaker encryption but is not an
            // executable write. Preserve it for raw output, or replace it
            // with the selected output seed when encrypting.
            if (!output_seed) {
                output << parsed->source_text << '\n';
            }
            continue;
        }

        RawLine converted = raw;
        if (output_seed) {
            converted = output_cipher.encrypt(raw);
        }

        output << text::hex(converted.op1, 8) << ' '
               << text::hex(converted.op2, 4) << '\n';
    }

    return output.str();
}

} // namespace gba::codebreaker
