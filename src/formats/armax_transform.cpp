#include "formats/armax_internal.hpp"

#include "core/text.hpp"
#include "crypto/tea.hpp"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace gba::armax::detail {

Result transform_text_impl(std::string_view input,
                      bool input_encrypted,
                      bool output_encrypted) {
    Result result;
    std::ostringstream output;
    crypto::TeaKey input_key = crypto::ProActionReplayV3Key;
    crypto::TeaKey output_key = crypto::ProActionReplayV3Key;

    const std::string named_input =
        text::normalize_plain_cheat_headers(input);
    const auto lines = text::split_lines(named_input);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string line = text::trim(lines[index]);
        const auto parsed = parse_line(line, index + 1U);

        if (!parsed) {
            output << lines[index];
            if (index + 1U < lines.size()) {
                output << '\n';
            }
            continue;
        }

        std::uint32_t op1 = parsed->op1;
        std::uint32_t op2 = parsed->op2;
        if (input_encrypted) {
            const auto converted = crypto::tea_decrypt(op1, op2, input_key);
            op1 = converted.first;
            op2 = converted.second;
        }

        if (!output_encrypted) {
            op2 = canonicalize_raw_operand(op1, op2);
        }

        output << format_line(op1, op2, output_encrypted, output_key);
        if (index + 1U < lines.size()) {
            output << '\n';
        }

        if (op1 == 0xDEADFACEU) {
            const auto next = crypto::pro_action_replay_v3_key_from_deadface(
                static_cast<std::uint16_t>(op2));
            input_key = next;
            output_key = next;
        }
    }

    result.text = output.str();
    return result;
}

} // namespace gba::armax::detail
