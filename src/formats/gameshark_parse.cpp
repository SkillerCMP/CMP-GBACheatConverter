#include "formats/gameshark_internal.hpp"

#include "core/text.hpp"

#include <utility>

namespace gba::gameshark::detail {

CheatDocument parse_document(std::string_view input,
                             const ParseOptions& options) {
    CheatDocument document;
    CheatEntry* current = nullptr;
    std::uint32_t next_assignment_group = 1U;
    crypto::TeaKey decrypt_key = crypto::GameSharkV1Key;

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
                document.entries.push_back(CheatEntry{clean_name(line), {}});
                current = &document.entries.back();
            }
            continue;
        }

        RawLine decoded = decode_line(*parsed, options.encrypted, decrypt_key);

        if (decoded.op1 == 0xDEADFACEU) {
            if (!current) {
                document.entries.push_back(
                    CheatEntry{"Converted GameShark Code", {}});
                current = &document.entries.back();
            }

            Operation seed = make_operation(
                OperationKind::EncryptionSeed,
                decoded,
                decoded.op1,
                decoded.op2,
                0U,
                "GameShark/Action Replay GBX dynamic encryption key");
            seed.encoding_hint = EncodingHint::GameSharkDeadface;
            current->operations.push_back(std::move(seed));
            decrypt_key = crypto::game_shark_v1_key_from_deadface(
                static_cast<std::uint16_t>(decoded.op2));
            continue;
        }

        if (!current) {
            document.entries.push_back(CheatEntry{"Converted Code", {}});
            current = &document.entries.back();
        }

        if (decoded.op2 == 0x001DC0DEU) {
            current->operations.push_back(
                make_operation(OperationKind::GameId, decoded,
                               decoded.op1, decoded.op2, 0,
                               "GameShark/AR GBX game ID"));
            continue;
        }

        const std::uint8_t type =
            static_cast<std::uint8_t>(decoded.op1 >> 28U);
        const std::uint32_t address = decoded.op1 & 0x0FFFFFFFU;

        switch (type) {
        case 0x0:
            current->operations.push_back(
                make_operation(OperationKind::Write, decoded,
                               address, decoded.op2 & 0xFFU, 1));
            break;

        case 0x1:
            current->operations.push_back(
                make_operation(OperationKind::Write, decoded,
                               address, decoded.op2 & 0xFFFFU, 2));
            break;

        case 0x2:
            current->operations.push_back(
                make_operation(OperationKind::Write, decoded,
                               address, decoded.op2, 4));
            break;

        case 0x3: {
            const std::uint32_t subtype =
                (decoded.op1 >> 16U) & 0xFFU;

            if (subtype == 0U) {
                const std::uint32_t count = decoded.op1 & 0xFFFFU;
                if (count == 0U) {
                    current->operations.push_back(
                        make_operation(OperationKind::Unsupported, decoded,
                                       0U, decoded.op2, 4,
                                       "GameShark assignment list has zero writes"));
                    document.warnings.push_back(
                        "GameShark/AR GBX assignment list has zero writes at "
                        "source line " +
                        std::to_string(decoded.source_line));
                    break;
                }

                std::vector<RawLine> continuation;
                const std::uint32_t remaining_addresses = count - 1U;
                const std::size_t continuation_rows =
                    (remaining_addresses + 1U) / 2U;
                std::size_t next_line = index + 1U;
                bool complete = true;

                while (continuation.size() < continuation_rows) {
                    while (next_line < lines.size() &&
                           text::trim(lines[next_line]).empty()) {
                        ++next_line;
                    }
                    if (next_line >= lines.size()) {
                        complete = false;
                        break;
                    }

                    const auto parameter =
                        parse_line(lines[next_line], next_line + 1U);
                    if (!parameter) {
                        complete = false;
                        break;
                    }

                    continuation.push_back(
                        decode_line(*parameter, options.encrypted, decrypt_key));
                    ++next_line;
                }

                if (!complete) {
                    current->operations.push_back(
                        make_operation(OperationKind::Unsupported, decoded,
                                       0U, decoded.op2, 4,
                                       "truncated GameShark assignment list"));
                    document.warnings.push_back(
                        "GameShark/AR GBX assignment list is missing continuation "
                        "rows at source line " +
                        std::to_string(decoded.source_line));
                    break;
                }

                const std::uint32_t group = next_assignment_group++;
                Operation first = make_operation(
                    OperationKind::Write, decoded,
                    decoded.op2, decoded.op2, 4,
                    "GameShark assignment-list implicit value-as-address write");
                set_assignment_hint(first, group, 0U, count);
                current->operations.push_back(std::move(first));

                std::uint32_t emitted = 1U;
                for (const RawLine& parameter : continuation) {
                    if (emitted < count) {
                        Operation operation = make_operation(
                            OperationKind::Write, parameter,
                            parameter.op1, decoded.op2, 4,
                            "GameShark assignment-list address");
                        set_assignment_hint(
                            operation, group, emitted, count);
                        current->operations.push_back(std::move(operation));
                        ++emitted;
                    }
                    if (emitted < count) {
                        Operation operation = make_operation(
                            OperationKind::Write, parameter,
                            parameter.op2, decoded.op2, 4,
                            "GameShark assignment-list address");
                        set_assignment_hint(
                            operation, group, emitted, count);
                        current->operations.push_back(std::move(operation));
                        ++emitted;
                    }
                }

                index = next_line - 1U;
                break;
            }

            const bool add =
                subtype == 0x10U || subtype == 0x30U || subtype == 0x50U;
            const bool subtract =
                subtype == 0x20U || subtype == 0x40U || subtype == 0x60U;
            if (!add && !subtract) {
                current->operations.push_back(
                    make_operation(
                        OperationKind::Unsupported, decoded,
                        decoded.op2, decoded.op1 & 0xFFFFU, 4U,
                        "unknown GameShark type-3 arithmetic subtype"));
                document.warnings.push_back(
                    "Unknown GameShark/AR GBX type-3 arithmetic subtype at "
                    "source line " +
                    std::to_string(decoded.source_line));
                break;
            }

            std::uint32_t immediate = 0U;
            std::uint32_t auxiliary = 0U;
            if (subtype == 0x10U || subtype == 0x20U) {
                immediate = decoded.op1 & 0xFFU;
            } else if (subtype == 0x30U || subtype == 0x40U) {
                immediate = decoded.op1 & 0xFFFFU;
            } else {
                std::size_t next_line = index + 1U;
                while (next_line < lines.size() &&
                       text::trim(lines[next_line]).empty()) {
                    ++next_line;
                }
                if (next_line >= lines.size()) {
                    current->operations.push_back(
                        make_operation(
                            OperationKind::Unsupported, decoded,
                            decoded.op2, 0U, 4U,
                            "truncated GameShark full-width arithmetic operation"));
                    document.warnings.push_back(
                        "GameShark/AR GBX full-width arithmetic row is missing "
                        "its continuation at source line " +
                        std::to_string(decoded.source_line));
                    break;
                }

                const auto parameter =
                    parse_line(lines[next_line], next_line + 1U);
                if (!parameter) {
                    current->operations.push_back(
                        make_operation(
                            OperationKind::Unsupported, decoded,
                            decoded.op2, 0U, 4U,
                            "invalid GameShark full-width arithmetic continuation"));
                    document.warnings.push_back(
                        "GameShark/AR GBX full-width arithmetic continuation is "
                        "not a code row at source line " +
                        std::to_string(decoded.source_line));
                    break;
                }

                const RawLine continuation =
                    decode_line(*parameter, options.encrypted, decrypt_key);
                immediate = continuation.op1;
                auxiliary = continuation.op2;
                index = next_line;
            }

            Operation arithmetic = make_operation(
                add ? OperationKind::Add : OperationKind::Subtract,
                decoded,
                decoded.op2,
                immediate,
                4U,
                add ? "GameShark 32-bit add" : "GameShark 32-bit subtract");
            arithmetic.encoding_hint = EncodingHint::GameSharkArithmetic;
            arithmetic.encoding_parameter = decoded.op1 & 0x00FFFFFFU;
            arithmetic.encoding_auxiliary = auxiliary;
            current->operations.push_back(std::move(arithmetic));
            break;
        }

        case 0x6: {
            if ((decoded.op1 & 0x0F000000U) != 0U) {
                current->operations.push_back(
                    make_operation(
                        OperationKind::Unsupported, decoded,
                        0U, decoded.op2, 0U,
                        "noncanonical GameShark ROM patch address bits"));
                document.warnings.push_back(
                    "GameShark/AR GBX ROM patch requires a 60aaaaaa first "
                    "word at source line " +
                    std::to_string(decoded.source_line));
                break;
            }

            Operation patch = make_operation(
                OperationKind::RomPatch, decoded,
                0x08000000U |
                    ((decoded.op1 & 0x00FFFFFFU) << 1U),
                decoded.op2 & 0xFFFFU, 2U,
                "GameShark 16-bit ROM interception patch");
            patch.encoding_parameter = decoded.op2 & 0xFFFF0000U;
            current->operations.push_back(std::move(patch));

            const std::uint32_t mode = decoded.op2 >> 28U;
            if ((decoded.op2 & 0x0FFF0000U) != 0U || mode > 2U) {
                document.warnings.push_back(
                    "GameShark/AR GBX ROM patch uses nonstandard mode/flag "
                    "bits at source line " +
                    std::to_string(decoded.source_line) +
                    "; the row will still be preserved exactly");
            }
            break;
        }

        case 0x8: {
            if (decoded.op1 == 0x80F00000U) {
                Operation slowdown = make_operation(
                    OperationKind::DeviceSlowdown, decoded,
                    0U, decoded.op2 & 0xFFFFU, 0U,
                    "GameShark physical-button slowdown loop");
                slowdown.encoding_parameter =
                    decoded.op2 & 0xFFFF0000U;
                current->operations.push_back(std::move(slowdown));
                if ((decoded.op2 & 0xFFFF0000U) != 0U) {
                    document.warnings.push_back(
                        "GameShark/AR GBX slowdown row has nonzero reserved "
                        "bits at source line " +
                        std::to_string(decoded.source_line) +
                        "; the row will be preserved exactly");
                }
                break;
            }

            const std::uint32_t subtype =
                decoded.op1 & 0x00F00000U;
            std::uint8_t width = 0U;
            if (subtype == 0x00100000U) {
                width = 1U;
            } else if (subtype == 0x00200000U) {
                width = 2U;
            }

            if (width == 0U) {
                current->operations.push_back(
                    make_operation(
                        OperationKind::Unsupported, decoded,
                        address, decoded.op2, 0,
                        "unknown GameShark device-button subtype"));
                document.warnings.push_back(
                    "GameShark/AR GBX device-button subtype is not a supported "
                    "8-bit/16-bit write at source line " +
                    std::to_string(decoded.source_line));
                break;
            }

            Operation condition = make_operation(
                OperationKind::IfDeviceButton, decoded,
                0U, 0U, 0U,
                "physical GameShark/Action Replay GBX button condition");
            condition.condition_span = 1U;
            current->operations.push_back(std::move(condition));

            Operation write = make_operation(
                OperationKind::Write, decoded,
                decoded.op1 & 0x0F0FFFFFU,
                decoded.op2 & (width == 1U ? 0xFFU : 0xFFFFU),
                width,
                "GameShark device-button combined write");
            write.encoding_hint = EncodingHint::GameSharkButtonWrite;
            current->operations.push_back(std::move(write));
            break;
        }

        case 0xD: {
            const std::uint32_t condition = decoded.op2 >> 20U;
            OperationKind kind = OperationKind::Unsupported;
            switch (condition) {
            case 0:
                kind = OperationKind::IfEqual;
                break;
            case 1:
                kind = OperationKind::IfNotEqual;
                break;
            case 2:
                kind = OperationKind::IfLessOrEqual;
                break;
            case 3:
                kind = OperationKind::IfGreaterOrEqual;
                break;
            default:
                break;
            }

            current->operations.push_back(
                make_operation(kind, decoded, address,
                               decoded.op2 & 0xFFFFU, 2,
                               condition == 2
                                   ? "GameShark less-or-equal condition"
                                   : condition == 3
                                         ? "GameShark greater-or-equal condition"
                                         : ""));
            if (kind == OperationKind::Unsupported) {
                document.warnings.push_back(
                    "Unknown GameShark/AR GBX condition subtype at source line " +
                    std::to_string(decoded.source_line));
            }
            break;
        }

        case 0xE: {
            const std::uint32_t span =
                (decoded.op1 >> 16U) & 0xFFU;
            const std::uint32_t subtype = decoded.op2 >> 28U;
            OperationKind kind = OperationKind::Unsupported;
            switch (subtype) {
            case 0U:
                kind = OperationKind::IfEqual;
                break;
            case 1U:
                kind = OperationKind::IfNotEqual;
                break;
            case 2U:
                kind = OperationKind::IfLessOrEqual;
                break;
            case 3U:
                kind = OperationKind::IfGreaterOrEqual;
                break;
            default:
                break;
            }
            if (span == 0U) {
                kind = OperationKind::Unsupported;
            }

            Operation condition = make_operation(
                kind,
                decoded,
                decoded.op2 & 0x0FFFFFFFU,
                decoded.op1 & 0xFFFFU,
                2U,
                subtype == 0U
                    ? "GameShark multiline equality condition"
                    : subtype == 1U
                          ? "GameShark multiline inequality condition"
                          : subtype == 2U
                                ? "GameShark multiline less-or-equal condition"
                                : subtype == 3U
                                      ? "GameShark multiline greater-or-equal condition"
                                      : "unknown GameShark multiline condition subtype");
            condition.condition_span = span;
            condition.encoding_hint =
                EncodingHint::GameSharkMultilineCondition;
            condition.encoding_parameter = subtype;
            current->operations.push_back(std::move(condition));

            if (span == 0U) {
                document.warnings.push_back(
                    "GameShark/AR GBX multiline condition controls zero "
                    "operations at source line " +
                    std::to_string(decoded.source_line));
            } else if (subtype > 3U) {
                document.warnings.push_back(
                    "Unknown GameShark/AR GBX multiline condition subtype at "
                    "source line " +
                    std::to_string(decoded.source_line));
            }
            break;
        }

        case 0xF:
            current->operations.push_back(
                make_operation(OperationKind::Hook, decoded,
                               0x08000000U | address,
                               decoded.op2, 0,
                               "GameShark hook/master row"));
            break;

        default:
            current->operations.push_back(
                make_operation(OperationKind::Unsupported, decoded,
                               address, decoded.op2, 0,
                               "unknown GameShark/AR GBX type"));
            document.warnings.push_back(
                "Unknown GameShark/AR GBX code type at source line " +
                std::to_string(decoded.source_line));
            break;
        }
    }

    return document;
}

} // namespace gba::gameshark::detail
