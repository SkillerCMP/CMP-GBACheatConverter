#include "formats/codebreaker.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace gba::codebreaker {
namespace {

std::uint32_t rotate_right(std::uint32_t value, unsigned amount) {
    amount &= 31U;
    if (amount == 0) {
        return value;
    }
    return (value >> amount) | (value << (32U - amount));
}

std::array<std::uint8_t, 6> load_bytes(std::uint32_t op1, std::uint16_t op2) {
    return {
        static_cast<std::uint8_t>(op1 >> 24U),
        static_cast<std::uint8_t>(op1 >> 16U),
        static_cast<std::uint8_t>(op1 >> 8U),
        static_cast<std::uint8_t>(op1),
        static_cast<std::uint8_t>(op2 >> 8U),
        static_cast<std::uint8_t>(op2)
    };
}

std::pair<std::uint32_t, std::uint16_t>
store_bytes(const std::array<std::uint8_t, 6>& bytes) {
    const std::uint32_t op1 =
        (static_cast<std::uint32_t>(bytes[0]) << 24U) |
        (static_cast<std::uint32_t>(bytes[1]) << 16U) |
        (static_cast<std::uint32_t>(bytes[2]) << 8U) |
        static_cast<std::uint32_t>(bytes[3]);

    const std::uint16_t op2 =
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[4]) << 8U) |
            bytes[5]);

    return {op1, op2};
}

void swap_bits(std::array<std::uint8_t, 6>& bytes,
               std::size_t x,
               std::size_t y) {
    const std::size_t byte_x = x >> 3U;
    const std::size_t byte_y = y >> 3U;
    const unsigned bit_x = static_cast<unsigned>(x & 7U);
    const unsigned bit_y = static_cast<unsigned>(y & 7U);

    const auto word_x = static_cast<unsigned int>(bytes[byte_x]);
    const auto word_y = static_cast<unsigned int>(bytes[byte_y]);
    const bool value_x = ((word_x >> bit_x) & 1U) != 0U;
    const bool value_y = ((word_y >> bit_y) & 1U) != 0U;

    if (value_x == value_y) {
        return;
    }

    bytes[byte_x] ^= static_cast<std::uint8_t>(1U << bit_x);
    bytes[byte_y] ^= static_cast<std::uint8_t>(1U << bit_y);
}

} // namespace

std::uint32_t Cipher::random() {
    const std::uint32_t roll = rng_state_ * 0x41C64E6DU + 0x3039U;
    const std::uint32_t roll2 = roll * 0x41C64E6DU + 0x3039U;
    const std::uint32_t roll3 = roll2 * 0x41C64E6DU + 0x3039U;

    std::uint32_t mix = (roll << 14U) & 0xC0000000U;
    mix |= (roll2 >> 1U) & 0x3FFF8000U;
    mix |= (roll3 >> 16U) & 0x7FFFU;
    rng_state_ = roll3;
    return mix;
}

std::size_t Cipher::swap_index() {
    std::uint32_t roll = random();
    std::uint32_t count = static_cast<std::uint32_t>(table_.size());

    if (roll == count) {
        roll = 0;
    }
    if (roll < count) {
        return roll;
    }

    std::uint32_t bit = 1;
    while (count < 0x10000000U && count < roll) {
        count <<= 4U;
        bit <<= 4U;
    }
    while (count < 0x80000000U && count < roll) {
        count <<= 1U;
        bit <<= 1U;
    }

    std::uint32_t mask = 0;
    while (true) {
        mask = 0;
        if (roll >= count) {
            roll -= count;
        }
        if (roll >= (count >> 1U)) {
            roll -= count >> 1U;
            mask |= rotate_right(bit, 1);
        }
        if (roll >= (count >> 2U)) {
            roll -= count >> 2U;
            mask |= rotate_right(bit, 2);
        }
        if (roll >= (count >> 3U)) {
            roll -= count >> 3U;
            mask |= rotate_right(bit, 3);
        }
        if (roll == 0 || (bit >> 4U) == 0) {
            break;
        }
        bit >>= 4U;
        count >>= 4U;
    }

    mask &= 0xE0000000U;
    if (mask == 0 || (bit & 7U) == 0) {
        return roll;
    }
    if ((mask & rotate_right(bit, 3)) != 0) {
        roll += count >> 3U;
    }
    if ((mask & rotate_right(bit, 2)) != 0) {
        roll += count >> 2U;
    }
    if ((mask & rotate_right(bit, 1)) != 0) {
        roll += count >> 1U;
    }

    return roll;
}

void Cipher::reseed(Seed seed) {
    rng_state_ = (seed.op2 & 0xFFU) ^ 0x1111U;

    for (std::size_t index = 0; index < table_.size(); ++index) {
        table_[index] = static_cast<std::uint8_t>(index);
    }

    for (int index = 0; index < 0x50; ++index) {
        const std::size_t x = swap_index();
        const std::size_t y = swap_index();
        std::swap(table_.at(x), table_.at(y));
    }

    rng_state_ = 0x4EFAD1C3U;
    for (std::uint32_t index = 0; index < ((seed.op1 >> 24U) & 0xFU); ++index) {
        rng_state_ = random();
    }

    seeds_[2] = random();
    seeds_[3] = random();

    const std::uint32_t seed_iterations =
        static_cast<std::uint32_t>(seed.op2 >> 8U);
    rng_state_ = seed_iterations ^ 0xF254U;
    for (std::uint32_t index = 0; index < seed_iterations; ++index) {
        rng_state_ = random();
    }

    seeds_[0] = random();
    seeds_[1] = random();
    master_ = seed.op1;
}

bool Cipher::active() const noexcept {
    return master_ != 0;
}

RawLine Cipher::decrypt(const RawLine& encrypted) const {
    if (!active()) {
        throw std::runtime_error("CodeBreaker cipher has not been seeded");
    }

    auto bytes = load_bytes(encrypted.op1, encrypted.op2);
    for (std::size_t reverse = TableSize; reverse > 0; --reverse) {
        const std::size_t index = reverse - 1;
        swap_bits(bytes, index, table_[index]);
    }

    auto words = store_bytes(bytes);
    words.first ^= seeds_[0];
    words.second ^= static_cast<std::uint16_t>(seeds_[1]);

    bytes = load_bytes(words.first, words.second);
    const std::uint8_t high =
        static_cast<std::uint8_t>(master_ >> 8U);
    const std::uint8_t low =
        static_cast<std::uint8_t>(master_);

    for (std::size_t index = 0; index < 5; ++index) {
        bytes[index] ^= static_cast<std::uint8_t>(high ^ bytes[index + 1]);
    }
    bytes[5] ^= high;

    for (std::size_t index = 5; index > 0; --index) {
        bytes[index] ^= static_cast<std::uint8_t>(low ^ bytes[index - 1]);
    }
    bytes[0] ^= low;

    words = store_bytes(bytes);
    words.first ^= seeds_[2];
    words.second ^= static_cast<std::uint16_t>(seeds_[3]);

    RawLine result = encrypted;
    result.op1 = words.first;
    result.op2 = words.second;
    return result;
}

RawLine Cipher::encrypt(const RawLine& raw) const {
    if (!active()) {
        throw std::runtime_error("CodeBreaker cipher has not been seeded");
    }

    std::uint32_t op1 = raw.op1 ^ seeds_[2];
    std::uint16_t op2 =
        static_cast<std::uint16_t>(raw.op2 ^ seeds_[3]);

    auto bytes = load_bytes(op1, op2);
    const std::uint8_t high =
        static_cast<std::uint8_t>(master_ >> 8U);
    const std::uint8_t low =
        static_cast<std::uint8_t>(master_);

    bytes[0] ^= low;
    for (std::size_t index = 1; index < 6; ++index) {
        bytes[index] ^= static_cast<std::uint8_t>(low ^ bytes[index - 1]);
    }

    bytes[5] ^= high;
    for (std::size_t reverse = 5; reverse > 0; --reverse) {
        const std::size_t index = reverse - 1;
        bytes[index] ^=
            static_cast<std::uint8_t>(high ^ bytes[index + 1]);
    }

    auto words = store_bytes(bytes);
    words.first ^= seeds_[0];
    words.second ^= static_cast<std::uint16_t>(seeds_[1]);

    bytes = load_bytes(words.first, words.second);
    for (std::size_t index = 0; index < TableSize; ++index) {
        swap_bits(bytes, index, table_[index]);
    }

    words = store_bytes(bytes);
    RawLine result = raw;
    result.op1 = words.first;
    result.op2 = words.second;
    return result;
}

} // namespace gba::codebreaker
