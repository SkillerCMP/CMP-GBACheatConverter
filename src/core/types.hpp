#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gba {

enum class OperationKind {
    Write,
    PointerWrite,
    Or,
    And,
    Add,
    Subtract,
    IfEqual,
    IfNotEqual,
    IfGreater,
    IfLess,
    IfGreaterOrEqual,
    IfLessOrEqual,
    IfAnd,
    IfNand,
    // Physical button on the GameShark/Action Replay GBX device. This is
    // distinct from the GBA KEYINPUT register used by normal button codes.
    IfDeviceButton,
    // GameShark/Action Replay GBX 16-bit cartridge ROM interception patch.
    RomPatch,
    // Physical GameShark device-button slowdown loop (80F00000).
    DeviceSlowdown,
    Hook,
    GameId,
    EncryptionSeed,
    Unsupported
};

enum class EncodingHint {
    None,
    FcdUnsignedComparison,
    GameSharkAssignmentList,
    GameSharkArithmetic,
    GameSharkMultilineCondition,
    GameSharkButtonWrite,
    GameSharkDeadface,
    ActionReplayMaxDeadface,
    ActionReplayMaxRomPatch,
    ActionReplayMaxSlowdown,
    ActionReplayMaxBlock,
    ActionReplayMaxButtonWrite,
    EzFlashEnhancedRomPatch
};

struct ConditionTerm {
    std::uint32_t address = 0;
    std::uint32_t value = 0;
    std::uint8_t width = 0;
};

struct Operation {
    OperationKind kind = OperationKind::Unsupported;
    std::uint32_t address = 0;
    std::uint32_t value = 0;
    std::uint8_t width = 0;
    std::uint32_t repeat = 1;
    // For condition operations, this is the number of following semantic
    // operations in the true/THEN branch. Most older formats use 1;
    // Action Replay MAX/PAR v3 can use a full block.
    std::uint32_t condition_span = 1;
    // Number of semantic operations in an optional ELSE branch. The true
    // branch is followed immediately by this false branch in the flat list.
    std::uint32_t condition_else_span = 0;
    // Tracks ELSE marker presence separately so an empty ELSE branch can
    // round-trip without being confused with a block that has no ELSE.
    bool condition_has_else = false;
    // Optional additional exact-equality terms that are ANDed together.
    // Fix 8 can compare several discontiguous byte runs in one IF= group.
    // Other formats must reject these compound conditions unless they have
    // an exact representation.
    std::vector<ConditionTerm> condition_terms;
    std::int32_t address_step = 0;
    std::int32_t value_step = 0;
    // Byte offset added to the pointer value for indirect writes.
    std::uint32_t pointer_offset = 0;
    // Optional exact byte payload for Enhanced operations whose data can be
    // longer than 32 bits (ADD/SUB/PTR/FILL/SLIDE). Bytes are stored in the
    // same little-endian/list order used by the .cht text format.
    std::vector<std::uint8_t> byte_payload;
    std::size_t source_line = 0;
    std::string source_text;
    std::string note;

    // Optional source-format grouping metadata. GameShark type-3 assignment
    // lists decode into individual semantic writes so conditions can count
    // them correctly, while this hint allows exact compact re-export.
    EncodingHint encoding_hint = EncodingHint::None;
    std::uint32_t encoding_group = 0;
    std::uint32_t encoding_index = 0;
    std::uint32_t encoding_count = 0;
    // Source-format-specific flags that are required for exact re-export.
    // GameShark ROM patches use this for the high-word patch-mode flags;
    // device slowdown rows preserve any original high-word bits here too.
    std::uint32_t encoding_parameter = 0;
    // Additional source-format payload needed for exact two-line re-export.
    // GameShark full-width arithmetic and AR MAX patch rows use this for
    // the otherwise ignored second word of their continuation line.
    std::uint32_t encoding_auxiliary = 0;
};

struct CheatEntry {
    std::string name;
    std::vector<Operation> operations;
};

struct CheatDocument {
    std::vector<CheatEntry> entries;
    std::vector<std::string> warnings;
};

} // namespace gba
