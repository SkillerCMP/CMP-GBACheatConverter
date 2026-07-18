#pragma once

#include <cstdint>
#include <string>
#include <cstddef>
#include <optional>
#include <utility>
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
    IfXor,
    IfNotXor,
    IfOr,
    IfNotOr,
    // Physical button on the GameShark/Action Replay GBX device. This is
    // distinct from the GBA KEYINPUT register used by normal button codes.
    IfDeviceButton,
    // GameShark/Action Replay GBX 16-bit cartridge ROM interception patch.
    RomPatch,
    // Physical GameShark device-button slowdown loop (80F00000).
    DeviceSlowdown,
    Transfer,
    ReadSubstitute,
    CompareReadSubstitute,
    Hook,
    GameId,
    EncryptionSeed,
    Unsupported
};

enum class EzFlashGroupMode {
    None,
    MultiSelect,
    ZeroOrOne
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
    // Optional comparison mask used by EZ-Flash IF*M commands. When enabled,
    // the comparison is performed against (memory & condition_mask).
    std::uint32_t condition_mask = 0;
    bool condition_has_mask = false;
    // Tracks ELSE marker presence separately so an empty ELSE branch can
    // round-trip without being confused with a block that has no ELSE.
    bool condition_has_else = false;
    // Optional additional exact-equality terms that are ANDed together.
    // A condition may retain additional discontiguous comparison terms.
    // Other formats must reject these compound conditions unless they have
    // an exact representation.
    std::vector<ConditionTerm> condition_terms;
    std::int32_t address_step = 0;
    std::int32_t value_step = 0;
    // Byte offset added to the pointer value for indirect writes.
    std::uint32_t pointer_offset = 0;
    // Native formats may carry values wider than the 32-bit device-code
    // model. These fields preserve exact data without changing existing
    // exporters that intentionally operate on value/address/width.
    std::uint64_t wide_value = 0;
    std::uint64_t wide_compare_value = 0;
    std::uint64_t wide_value_step = 0;
    bool has_wide_value = false;
    bool has_wide_compare_value = false;
    bool has_wide_value_step = false;
    bool big_endian = false;
    std::uint32_t source_address = 0;
    std::uint32_t source_address_step = 0;
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



enum class MgbaCodeFamily {
    AutoDetect,
    GameSharkV1Encrypted,
    GameSharkV1Raw,
    ProActionReplayV3Encrypted,
    ProActionReplayV3Raw
};

struct MgbaCheatMetadata {
    MgbaCodeFamily family = MgbaCodeFamily::AutoDetect;
    std::vector<std::string> code_lines;
};

struct MednafenCheatMetadata {
    std::string rom_md5;
    std::string game_name;
    char type = 'R';
    std::uint8_t length = 1U;
    bool big_endian = false;
    std::uint32_t instance_count = 0U;
    std::uint32_t address = 0U;
    std::uint64_t value = 0U;
    std::uint64_t compare = 0U;
    std::uint32_t repeat_count = 1U;
    std::uint32_t repeat_address_increment = 0U;
    std::uint64_t repeat_value_increment = 0U;
    std::uint32_t copy_source_address = 0U;
    std::uint32_t copy_source_increment = 0U;
    std::string conditions;
};

struct RetroArchCheatMetadata {
    std::string code;
    std::uint32_t handler = 0U;
    std::uint32_t memory_search_size = 3U;
    std::uint32_t cheat_type = 1U;
    std::uint32_t value = 0U;
    std::uint32_t address = 0U;
    std::uint32_t address_mask = 0U;
    std::uint32_t rumble_type = 0U;
    std::uint32_t rumble_value = 0U;
    std::uint32_t rumble_port = 0U;
    std::uint32_t rumble_primary_strength = 0U;
    std::uint32_t rumble_primary_duration = 0U;
    std::uint32_t rumble_secondary_strength = 0U;
    std::uint32_t rumble_secondary_duration = 0U;
    std::uint32_t repeat_count = 1U;
    std::uint32_t repeat_add_to_value = 0U;
    std::uint32_t repeat_add_to_address = 1U;
    bool big_endian = false;
};

struct CheatEntry {
    CheatEntry() = default;
    CheatEntry(std::string entry_name, std::vector<Operation> entry_operations)
        : name(std::move(entry_name)),
          operations(std::move(entry_operations)) {}

    std::string name;
    std::vector<Operation> operations;
    // Native cheat-list toggle state. Device text formats without a stored
    // state leave entries disabled by default.
    bool enabled = false;
    // Exact RetroArch fields are retained so handler-0 core codes and
    // handler-1 native-memory records can be re-exported without guessing.
    std::optional<RetroArchCheatMetadata> retroarch;
    // Exact mGBA directive family and source rows for lossless .cheats
    // round trips, including mixed/native-only sets.
    std::optional<MgbaCheatMetadata> mgba;
    // Exact Mednafen section and record fields, including 64-bit values,
    // transfer metadata, extended repeats, and the raw conditions line.
    std::optional<MednafenCheatMetadata> mednafen;

    // EZ-Flash database grouping metadata. Enhanced revision 6 permits
    // standalone name=commands rows, normal multi-select [Group] sections,
    // and optional zero-or-one [Group|ONE] sections.
    std::string ezflash_group_name;
    std::string ezflash_option_name;
    EzFlashGroupMode ezflash_group_mode = EzFlashGroupMode::None;

    // CMP database metadata is independent from the device format. A path
    // contains each enclosing !Group: name from outermost to innermost.
    std::vector<std::string> cmp_group_path;
    std::string credits;
    std::size_t cmp_order = 0U;
};

struct CmpGroup {
    std::vector<std::string> path;
    std::vector<Operation> header_operations;
    std::size_t order = 0U;
};

struct CheatDocument {
    CheatDocument() = default;
    CheatDocument(std::vector<CheatEntry> document_entries,
                  std::vector<std::string> document_warnings)
        : entries(std::move(document_entries)),
          warnings(std::move(document_warnings)) {}

    std::vector<CheatEntry> entries;
    std::vector<std::string> warnings;
    std::vector<CmpGroup> cmp_groups;
};

} // namespace gba
