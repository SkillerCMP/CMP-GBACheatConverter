#include "formats/armax_internal.hpp"

#include "core/text.hpp"
#include "crypto/tea.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gba::armax::detail {

CheatDocument parse_document(std::string_view input, const ParseOptions& options) {
    CheatDocument document;
    CheatEntry* current = nullptr;
    Pending pending;
    crypto::TeaKey decrypt_key = crypto::ProActionReplayV3Key;
    std::vector<BlockFrame> block_stack;

    const auto discard_open_blocks = [&](const std::string& reason) {
        if (!current || block_stack.empty()) {
            block_stack.clear();
            return;
        }
        const std::size_t first = block_stack.front().condition_index;
        const std::size_t source_line =
            first < current->operations.size()
                ? current->operations[first].source_line
                : 0U;
        if (first < current->operations.size()) {
            current->operations.erase(
                current->operations.begin() +
                    static_cast<std::ptrdiff_t>(first),
                current->operations.end());
        }
        document.warnings.push_back(
            "Action Replay MAX: " + reason +
            (source_line == 0U
                 ? std::string{}
                 : " at source line " + std::to_string(source_line)));
        block_stack.clear();
    };

    const std::string named_input =
        text::normalize_plain_cheat_headers(input);
    const auto lines = text::split_lines(named_input);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string line = text::trim(lines[index]);
        if (line.empty()) {
            continue;
        }

        const auto parsed = parse_line(line, index + 1U);
        if (!parsed) {
            const bool bracket_name =
                line.size() >= 2 && line.front() == '[' && line.back() == ']';
            const bool colon_name = !line.empty() && line.back() == ':';
            const bool metadata_name =
                text::is_inline_metadata_name_line(line);
            if (bracket_name || colon_name || metadata_name) {
                discard_open_blocks(
                    "unterminated block condition was discarded before a new cheat entry");
                document.entries.push_back(CheatEntry{clean_name(line), {}});
                current = &document.entries.back();
                pending = {};
                block_stack.clear();
            }
            continue;
        }

        RawLine decoded = *parsed;
        if (options.encrypted) {
            const auto raw = crypto::tea_decrypt(
                parsed->op1, parsed->op2, decrypt_key);
            decoded.op1 = raw.first;
            decoded.op2 = raw.second;
        }
        if (decoded.op1 == 0xDEADFACEU) {
            decrypt_key = crypto::pro_action_replay_v3_key_from_deadface(
                static_cast<std::uint16_t>(decoded.op2));
        }

        if (!current) {
            document.entries.push_back(
                CheatEntry{"Converted Action Replay MAX Code", {}});
            current = &document.entries.back();
        }

        if (pending.kind != PendingKind::None) {
            if (pending.kind == PendingKind::Fill) {
                Operation operation;
                operation.kind = OperationKind::Write;
                operation.address = pending.address;
                operation.width = pending.width;
                operation.value =
                    decoded.op1 & width_mask(pending.width);
                operation.value_step =
                    static_cast<std::int32_t>(
                        (decoded.op2 >> 24U) & 0xFFU);
                operation.repeat =
                    ((decoded.op2 >> 16U) & 0xFFU) + 1U;
                operation.address_step =
                    static_cast<std::int32_t>(
                        (decoded.op2 & 0xFFFFU) * pending.width);
                operation.source_line = pending.source_line;
                operation.source_text = pending.source_text;
                operation.note = "Action Replay MAX increment/fill";
                current->operations.push_back(operation);
            } else if (pending.kind == PendingKind::RomPatch) {
                RawLine combined = decoded;
                combined.source_line = pending.source_line;
                combined.source_text = pending.source_text;
                Operation patch = make_operation(
                    OperationKind::RomPatch,
                    combined,
                    pending.address,
                    decoded.op1,
                    2U,
                    "Action Replay MAX ROM interception patch");
                patch.encoding_hint = EncodingHint::ActionReplayMaxRomPatch;
                patch.encoding_parameter = pending.parameter;
                patch.encoding_auxiliary = decoded.op2;
                current->operations.push_back(std::move(patch));
            } else {
                RawLine combined = decoded;
                combined.source_line = pending.source_line;
                combined.source_text = pending.source_text;

                Operation button = make_operation(
                    OperationKind::IfDeviceButton, combined,
                    0, 0, 0,
                    "physical Action Replay MAX device button");
                button.condition_span = 1U;
                button.encoding_hint =
                    EncodingHint::ActionReplayMaxButtonWrite;
                current->operations.push_back(button);

                Operation write = make_operation(
                    OperationKind::Write, decoded,
                    pending.address,
                    decoded.op1 & width_mask(pending.width),
                    pending.width,
                    "Action Replay MAX device-button write");
                write.encoding_hint =
                    EncodingHint::ActionReplayMaxButtonWrite;
                write.encoding_parameter = decoded.op2;
                current->operations.push_back(write);
            }
            pending = {};
            continue;
        }

        if (decoded.op1 == 0xDEADFACEU) {
            Operation seed = make_operation(
                OperationKind::EncryptionSeed, decoded,
                decoded.op1, decoded.op2, 0,
                "Action Replay MAX dynamic encryption seed");
            seed.encoding_hint = EncodingHint::ActionReplayMaxDeadface;
            current->operations.push_back(seed);
            continue;
        }

        if (decoded.op2 == 0x001DC0DEU) {
            current->operations.push_back(
                make_operation(OperationKind::GameId, decoded,
                               decoded.op1, decoded.op2, 0,
                               "Action Replay MAX game ID"));
            continue;
        }

        if (decoded.op1 == 0x00000000U) {
            const std::uint32_t special = decoded.op2 & kSpecialMask;
            switch (special) {
            case kSpecialEnd:
                break;

            case kSpecialSlowdown: {
                Operation slowdown = make_operation(
                    OperationKind::DeviceSlowdown,
                    decoded,
                    0U,
                    decoded.op2 & 0x00FFFFFFU,
                    0U,
                    "Action Replay MAX slowdown operation");
                slowdown.encoding_hint =
                    EncodingHint::ActionReplayMaxSlowdown;
                slowdown.encoding_parameter = decoded.op2;
                current->operations.push_back(std::move(slowdown));
                break;
            }

            case kSpecialButton8:
            case kSpecialButton16:
            case kSpecialButton32:
                pending.kind = PendingKind::ButtonWrite;
                pending.address = decode_address(decoded.op2);
                pending.width = special == kSpecialButton8
                    ? 1
                    : (special == kSpecialButton16 ? 2 : 4);
                pending.source_line = decoded.source_line;
                pending.source_text = decoded.source_text;
                break;

            case kSpecialPatch1:
            case kSpecialPatch2:
            case kSpecialPatch3:
            case kSpecialPatch4:
                pending.kind = PendingKind::RomPatch;
                pending.address = 0x08000000U |
                    ((decoded.op2 & 0x00FFFFFFU) << 1U);
                pending.width = 2;
                pending.parameter = special;
                pending.source_line = decoded.source_line;
                pending.source_text = decoded.source_text;
                break;

            case kSpecialEndIf:
                if (block_stack.empty()) {
                    add_warning(document, decoded,
                                "ENDIF does not have a matching block condition");
                    break;
                }
                {
                    const BlockFrame frame = block_stack.back();
                    block_stack.pop_back();
                    Operation& condition =
                        current->operations[frame.condition_index];
                    const std::size_t branch_size =
                        current->operations.size() -
                        frame.condition_index - 1U;
                    if (frame.else_seen) {
                        condition.condition_else_span =
                            static_cast<std::uint32_t>(
                                branch_size - condition.condition_span);
                    } else {
                        condition.condition_span =
                            static_cast<std::uint32_t>(branch_size);
                    }
                }
                break;

            case kSpecialElse:
                if (block_stack.empty()) {
                    add_warning(document, decoded,
                                "ELSE does not have a matching block condition");
                    break;
                }
                {
                    BlockFrame& frame = block_stack.back();
                    if (frame.else_seen) {
                        add_warning(document, decoded,
                                    "block condition contains more than one ELSE");
                        break;
                    }
                    Operation& condition =
                        current->operations[frame.condition_index];
                    condition.condition_span =
                        static_cast<std::uint32_t>(
                            current->operations.size() -
                            frame.condition_index - 1U);
                    condition.condition_has_else = true;
                    frame.else_seen = true;
                }
                break;

            case kSpecialFill8:
            case kSpecialFill16:
            case kSpecialFill32:
                pending.kind = PendingKind::Fill;
                pending.address = decode_address(decoded.op2);
                pending.width = special == kSpecialFill8
                    ? 1
                    : (special == kSpecialFill16 ? 2 : 4);
                pending.source_line = decoded.source_line;
                pending.source_text = decoded.source_text;
                break;

            default:
                add_unsupported(
                    *current, document, decoded, 0, decoded.op2, 0,
                    "unknown Action Replay MAX special operation");
                break;
            }
            continue;
        }

        if ((decoded.op1 >> 24U) == 0xC4U) {
            current->operations.push_back(
                make_operation(
                    OperationKind::Hook, decoded,
                    0x08000000U | (decoded.op1 & 0x01FFFFFEU),
                    decoded.op2, 0,
                    "Action Replay MAX hook/master line"));
            continue;
        }

        const std::uint8_t width = decode_width(decoded.op1);
        if (width == 0) {
            add_unsupported(
                *current, document, decoded,
                decode_address(decoded.op1), decoded.op2, 0,
                "invalid Action Replay MAX width");
            continue;
        }

        if ((decoded.op1 & kConditionMask) != 0) {
            const std::uint32_t action = decoded.op1 & kActionMask;
            if (action == kActionDisable) {
                add_unsupported(
                    *current, document, decoded,
                    decode_address(decoded.op1),
                    decoded.op2 & width_mask(width), width,
                    "disable/while condition is not cross-format compatible");
                continue;
            }
            Operation condition = make_operation(
                decode_condition_kind(decoded.op1),
                decoded,
                decode_address(decoded.op1),
                decoded.op2 & width_mask(width),
                width,
                ((decoded.op1 & kConditionMask) == kCondLessUnsigned ||
                 (decoded.op1 & kConditionMask) == kCondGreaterUnsigned)
                    ? "unsigned Action Replay MAX comparison"
                    : "Action Replay MAX comparison");
            condition.encoding_parameter =
                decoded.op1 & kConditionMask;
            if (action == kActionBlock) {
                condition.condition_span = 0U;
                condition.condition_else_span = 0U;
                condition.encoding_hint =
                    EncodingHint::ActionReplayMaxBlock;
                current->operations.push_back(condition);
                block_stack.push_back(
                    BlockFrame{current->operations.size() - 1U, false});
            } else {
                condition.condition_span =
                    action == kActionNextTwo ? 2U : 1U;
                current->operations.push_back(condition);
            }
            continue;
        }

        if ((decoded.op1 & kSpecialBit) != 0 &&
            (decoded.op1 & 0xFE000000U) != 0xC6000000U) {
            add_unsupported(
                *current, document, decoded,
                decode_address(decoded.op1), decoded.op2, width,
                "reserved Action Replay MAX special bit is set");
            continue;
        }

        const std::uint32_t base = decoded.op1 & kBaseMask;
        if (base == kBaseOther) {
            const std::uint8_t io_width =
                ((decoded.op1 >> 24U) & 1U) == 0 ? 2 : 4;
            current->operations.push_back(
                make_operation(
                    OperationKind::Write, decoded,
                    0x04000000U | (decoded.op1 & 0x00FFFFFFU),
                    decoded.op2 & width_mask(io_width),
                    io_width,
                    "Action Replay MAX I/O write"));
            continue;
        }

        if (base == kBaseIndirect) {
            Operation operation = make_operation(
                OperationKind::PointerWrite, decoded,
                decode_address(decoded.op1),
                decoded.op2 & width_mask(width), width,
                "Action Replay MAX indirect/pointer write");
            if (width == 1U) {
                operation.pointer_offset = decoded.op2 >> 8U;
            } else if (width == 2U) {
                operation.pointer_offset = (decoded.op2 >> 16U) * 2U;
            }
            current->operations.push_back(operation);
            continue;
        }

        Operation operation = make_operation(
            base == kBaseAdd ? OperationKind::Add : OperationKind::Write,
            decoded,
            decode_address(decoded.op1),
            decoded.op2 & width_mask(width),
            width);

        if (base == kBaseAssign && width < 4) {
            operation.repeat =
                (decoded.op2 >> (width * 8U)) + 1U;
            operation.address_step = width;
        }

        current->operations.push_back(operation);
    }

    if (pending.kind != PendingKind::None) {
        document.warnings.push_back(
            "Action Replay MAX input ended during a multi-line operation");
    }
    discard_open_blocks(
        "unterminated block condition was discarded at end of input");

    return document;
}

} // namespace gba::armax::detail
