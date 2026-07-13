#include "cli/cli_internal.hpp"

#include "core/text.hpp"
#include "crypto/tea.hpp"

#include <ostream>
#include <sstream>
#include <stdexcept>

namespace gba::cli::detail {
namespace {

std::string crypt_8x8(std::string_view input,
                      const gba::crypto::TeaKey& key,
                      bool encrypt) {
    std::ostringstream output;
    for (const std::string& raw_line : gba::text::split_lines(input)) {
        const std::string line = gba::text::trim(raw_line);
        if (!gba::text::is_code_line_8x8(line)) {
            if (!line.empty()) {
                output << line << '\n';
            }
            continue;
        }

        const auto left =
            gba::text::parse_hex_u32(std::string_view(line).substr(0, 8));
        const auto right =
            gba::text::parse_hex_u32(std::string_view(line).substr(9, 8));
        if (!left || !right) {
            throw std::runtime_error("Invalid 8+8 code line");
        }

        const auto converted = encrypt
            ? gba::crypto::tea_encrypt(*left, *right, key)
            : gba::crypto::tea_decrypt(*left, *right, key);

        output << gba::text::hex(converted.first, 8)
               << ' '
               << gba::text::hex(converted.second, 8)
               << '\n';
    }
    return output.str();
}

} // namespace

int run_crypto(const Options& options,
               std::string_view input,
               std::ostream& output_stream) {
    if (options.encrypt == options.decrypt) {
        throw std::runtime_error(
            "Select exactly one of --encrypt or --decrypt");
    }

    const auto* key = &gba::crypto::GameSharkV1Key;
    if (options.crypt_format == "par-v3") {
        key = &gba::crypto::ProActionReplayV3Key;
    } else if (options.crypt_format != "gsa-v1") {
        throw std::runtime_error(
            "Unknown crypto format: " + options.crypt_format);
    }

    output_stream << crypt_8x8(input, *key, options.encrypt);
    return 0;
}

} // namespace gba::cli::detail
