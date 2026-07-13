#include "test_support.hpp"

namespace gba::tests {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ByteMap direct_write_bytes(const gba::CheatDocument& document) {
    ByteMap bytes;
    for (const gba::CheatEntry& entry : document.entries) {
        for (const gba::Operation& operation : entry.operations) {
            if (operation.kind != gba::OperationKind::Write ||
                (operation.width != 1U &&
                 operation.width != 2U &&
                 operation.width != 4U)) {
                continue;
            }
            for (std::uint32_t offset = 0U;
                 offset < operation.width;
                 ++offset) {
                bytes[operation.address + offset] =
                    static_cast<std::uint8_t>(
                        (operation.value >> (offset * 8U)) & 0xFFU);
            }
        }
    }
    return bytes;
}

} // namespace gba::tests
