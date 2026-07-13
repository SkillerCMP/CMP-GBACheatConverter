#pragma once

#include <array>
#include <cstdint>
#include <utility>

namespace gba::crypto {

using TeaKey = std::array<std::uint32_t, 4>;

constexpr TeaKey GameSharkV1Key{
    0x09F4FBBDU, 0x9681884AU, 0x352027E9U, 0xF3DEE5A7U
};

constexpr TeaKey ProActionReplayV3Key{
    0x7AA9648FU, 0x7FAE6994U, 0xC0EFAAD5U, 0x42712C57U
};

std::pair<std::uint32_t, std::uint32_t>
tea_encrypt(std::uint32_t left, std::uint32_t right, const TeaKey& key);

std::pair<std::uint32_t, std::uint32_t>
tea_decrypt(std::uint32_t left, std::uint32_t right, const TeaKey& key);

// Derives the rolling GameShark/Action Replay GBX v1/v2 TEA key selected
// by a DEADFACE xxxxxxxx row. Only the low 16 bits select the tables.
TeaKey game_shark_v1_key_from_deadface(std::uint16_t value);

// Derives the rolling Pro Action Replay v3/v4 TEA key selected by a
// DEADFACE xxxxxxxx row. Only the low 16 bits select the tables.
TeaKey pro_action_replay_v3_key_from_deadface(std::uint16_t value);

} // namespace gba::crypto
