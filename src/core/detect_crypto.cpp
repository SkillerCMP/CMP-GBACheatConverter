#include "core/detect_internal.hpp"

#include "crypto/tea.hpp"

#include <cstdint>
#include <vector>

namespace gba::detect::internal {

std::vector<Line88> decrypt_rows(
    const std::vector<Line88>& rows,
    const crypto::TeaKey& key) {
    std::vector<Line88> result;
    result.reserve(rows.size());
    for (const Line88& row : rows) {
        const auto decoded = crypto::tea_decrypt(row.left, row.right, key);
        result.push_back(Line88{decoded.first, decoded.second});
    }
    return result;
}

std::vector<Line88> decrypt_armax_rows(
    const std::vector<Line88>& rows) {
    std::vector<Line88> result;
    result.reserve(rows.size());
    crypto::TeaKey key = crypto::ProActionReplayV3Key;
    for (const Line88& row : rows) {
        const auto decoded = crypto::tea_decrypt(row.left, row.right, key);
        result.push_back(Line88{decoded.first, decoded.second});
        if (decoded.first == 0xDEADFACEU) {
            key = crypto::pro_action_replay_v3_key_from_deadface(
                static_cast<std::uint16_t>(decoded.second));
        }
    }
    return result;
}

} // namespace gba::detect::internal
