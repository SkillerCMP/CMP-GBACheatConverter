#pragma once

#include "core/detect.hpp"
#include "crypto/tea.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::detect::internal {

struct Line88 {
    std::uint32_t left = 0;
    std::uint32_t right = 0;
};

struct Candidate {
    Format format = Format::Unknown;
    int score = 0;
    std::vector<std::string> reasons;
};

bool starts_with(std::string_view value, std::string_view prefix);
std::optional<Line88> parse_8x8(std::string_view raw);
bool plausible_gba_address(std::uint32_t address);
int address_score(std::uint32_t address);

std::vector<Line88> decrypt_rows(
    const std::vector<Line88>& rows,
    const crypto::TeaKey& key);
std::vector<Line88> decrypt_armax_rows(
    const std::vector<Line88>& rows);

Candidate score_gameshark(
    const std::vector<Line88>& rows,
    Format format);

std::uint32_t armax_address(std::uint32_t encoded);
bool known_armax_special(std::uint32_t right);
Candidate score_armax(
    const std::vector<Line88>& rows,
    Format format);

} // namespace gba::detect::internal
