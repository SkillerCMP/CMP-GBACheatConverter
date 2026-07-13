#include "formats/armax_encode_internal.hpp"

#include <cstdint>
#include <string>

namespace gba::armax::detail {

bool EntryEncoder::encode_non_condition(
    const Operation& operation,
    EncodedLines& destination) {
    const std::size_t original_size = destination.size();
    const auto address = encode_address(operation.address);
    const std::uint32_t wb = width_bits(operation.width);

    const auto fail = [&](const std::string& reason) {
        destination.resize(original_size);
        unsupported(operation, reason);
        return false;
    };

    switch (operation.kind) {
    case OperationKind::Write: {
        if (operation.address >= 0x04000000U &&
            operation.address <= 0x04FFFFFFU) {
            if (operation.repeat != 1U ||
                operation.value_step != 0 ||
                operation.address_step != 0) {
                return fail("repeated I/O writes are not encoded");
            }
            if (operation.width == 2U) {
                destination.emplace_back(
                    0xC6000000U |
                        (operation.address & 0x00FFFFFFU),
                    operation.value & 0xFFFFU);
                return true;
            }
            if (operation.width == 4U) {
                destination.emplace_back(
                    0xC7000000U |
                        (operation.address & 0x00FFFFFFU),
                    operation.value);
                return true;
            }
            return fail("AR MAX has no normal 8-bit I/O write");
        }

        if (!address || wb == 0xFFFFFFFFU) {
            return fail("write address/width cannot be represented");
        }

        const std::uint32_t repeat =
            operation.repeat == 0U ? 1U : operation.repeat;
        const std::uint32_t natural_step = operation.width;
        const bool compact_fill =
            operation.width < 4U &&
            operation.value_step == 0 &&
            (operation.address_step == 0 ||
             operation.address_step ==
                 static_cast<std::int32_t>(natural_step)) &&
            ((operation.width == 1U && repeat <= 0x1000000U) ||
             (operation.width == 2U && repeat <= 0x10000U));

        if (compact_fill) {
            const unsigned shift = operation.width * 8U;
            destination.emplace_back(
                kBaseAssign | wb | *address,
                ((repeat - 1U) << shift) |
                    (operation.value &
                     width_mask(operation.width)));
            return true;
        }

        for (std::uint32_t repeat_index = 0;
             repeat_index < repeat;
             ++repeat_index) {
            const std::uint32_t step =
                operation.address_step == 0
                    ? operation.width
                    : static_cast<std::uint32_t>(
                          operation.address_step);
            const std::uint32_t current_address =
                operation.address + repeat_index * step;
            const std::uint32_t current_value =
                operation.value +
                repeat_index *
                    static_cast<std::uint32_t>(
                        operation.value_step);
            const auto current_encoded =
                encode_address(current_address);
            if (!current_encoded) {
                return fail(
                    "expanded write address cannot be represented");
            }
            destination.emplace_back(
                kBaseAssign | wb | *current_encoded,
                current_value & width_mask(operation.width));
        }
        return true;
    }

    case OperationKind::PointerWrite: {
        if (!address || wb == 0xFFFFFFFFU) {
            return fail(
                "pointer address/width cannot be represented");
        }
        std::uint32_t parameter = 0;
        if (operation.width == 1U) {
            if (operation.pointer_offset > 0x00FFFFFFU) {
                return fail(
                    "8-bit pointer offset exceeds 24 bits");
            }
            parameter = (operation.pointer_offset << 8U) |
                        (operation.value & 0xFFU);
        } else if (operation.width == 2U) {
            if ((operation.pointer_offset & 1U) != 0U ||
                operation.pointer_offset > 0x0001FFFEU) {
                return fail(
                    "16-bit pointer offset must be even and fit 16 units");
            }
            parameter =
                ((operation.pointer_offset / 2U) << 16U) |
                (operation.value & 0xFFFFU);
        } else if (operation.width == 4U) {
            if (operation.pointer_offset != 0U) {
                return fail(
                    "32-bit pointer writes do not encode an offset");
            }
            parameter = operation.value;
        } else {
            return fail("unsupported pointer write width");
        }
        destination.emplace_back(
            kBaseIndirect | wb | *address, parameter);
        return true;
    }

    case OperationKind::Add:
    case OperationKind::Subtract:
        if (!address || wb == 0xFFFFFFFFU) {
            return fail("arithmetic address/width cannot be represented");
        }
        destination.emplace_back(
            kBaseAdd | wb | *address,
            operation.kind == OperationKind::Subtract
                ? ((~operation.value + 1U) &
                   width_mask(operation.width))
                : (operation.value &
                   width_mask(operation.width)));
        return true;

    case OperationKind::GameId:
        if (operation.value != 0x001DC0DEU) {
            return fail(
                "FCD game-ID metadata has no exact Action Replay MAX mapping");
        }
        destination.emplace_back(
            operation.address, operation.value);
        return true;

    case OperationKind::EncryptionSeed:
        if (operation.encoding_hint ==
            EncodingHint::ActionReplayMaxDeadface) {
            destination.emplace_back(
                0xDEADFACEU, operation.value);
        }
        return true;

    case OperationKind::Hook:
        if (!hook_fits_armax(operation)) {
            return fail(
                "hook/master code cannot be represented exactly");
        }
        destination.emplace_back(
            0xC4000000U |
                (operation.address & 0x01FFFFFEU),
            operation.value);
        return true;

    case OperationKind::Or:
    case OperationKind::And:
        return fail(
            "logical write operation has no AR MAX mapping");

    case OperationKind::RomPatch: {
        if (operation.address < 0x08000000U ||
            operation.address > 0x09FFFFFEU ||
            (operation.address & 1U) != 0U) {
            return fail("AR MAX ROM patch address is invalid");
        }

        if (operation.encoding_hint ==
            EncodingHint::ActionReplayMaxRomPatch) {
            if ((operation.encoding_parameter & 0xFF000000U) <
                    kSpecialPatch1 ||
                (operation.encoding_parameter & 0xFF000000U) >
                    kSpecialPatch4 ||
                ((operation.encoding_parameter >> 24U) & 1U) !=
                    0U) {
                return fail("AR MAX ROM patch metadata is invalid");
            }
            destination.emplace_back(
                0U,
                (operation.encoding_parameter & 0xFF000000U) |
                    (((operation.address - 0x08000000U) >> 1U) &
                     0x00FFFFFFU));
            destination.emplace_back(
                operation.value,
                operation.encoding_auxiliary);
            return true;
        }

        if (operation.encoding_hint !=
                EncodingHint::EzFlashEnhancedRomPatch ||
            operation.encoding_parameter != 0U ||
            (operation.width != 2U && operation.width != 4U)) {
            return fail(
                "ROM patch has no exact Action Replay MAX mapping");
        }

        const auto emit_direct_patch =
            [&](std::uint32_t address_value,
                std::uint16_t patch_value) {
                destination.emplace_back(
                    0U,
                    kSpecialPatch1 |
                        (((address_value - 0x08000000U) >> 1U) &
                         0x00FFFFFFU));
                destination.emplace_back(
                    static_cast<std::uint32_t>(patch_value), 0U);
            };

        emit_direct_patch(
            operation.address,
            static_cast<std::uint16_t>(operation.value & 0xFFFFU));
        if (operation.width == 4U) {
            if (operation.address > 0x09FFFFFCU) {
                return fail(
                    "32-bit ROM patch exceeds the AR MAX ROM range");
            }
            emit_direct_patch(
                operation.address + 2U,
                static_cast<std::uint16_t>(
                    (operation.value >> 16U) & 0xFFFFU));
        }
        return true;
    }

    case OperationKind::DeviceSlowdown:
        if (operation.encoding_hint !=
            EncodingHint::ActionReplayMaxSlowdown) {
            return fail(
                "foreign physical-device slowdown control has no Action Replay MAX mapping");
        }
        destination.emplace_back(
            0U, operation.encoding_parameter);
        return true;

    case OperationKind::Unsupported:
        return fail("unsupported source operation");

    default:
        return fail("unexpected condition operation");
    }
}

} // namespace gba::armax::detail
