#pragma once

#include "formats/ezflash.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gba::ezflash::detail {

inline constexpr std::size_t kEnhancedRuntimeRecordLimit = 128U;
inline constexpr std::size_t kEnhancedRuntimeWriteWorkLimit = 4096U;
inline constexpr std::size_t kEnhancedSectionNameLimit = 49U;
inline constexpr std::size_t kEnhancedPhysicalLineLimit = 298U;

inline std::optional<std::uint32_t> canonical_rom_address(
    std::uint32_t address) {
    if (address <= 0x03FFFFFFU) {
        return 0x08000000U + address;
    }
    if (address >= 0x08000000U && address <= 0x0DFFFFFFU) {
        return 0x08000000U + ((address - 0x08000000U) & 0x01FFFFFFU);
    }
    return std::nullopt;
}

struct ByteWrite {
    std::uint32_t address = 0;
    std::uint8_t value = 0;
};

struct Group {
    std::vector<ByteWrite> conditions;
    std::vector<ByteWrite> writes;
};

struct RomGuardGroup {
    std::vector<ByteWrite> conditions;
    std::vector<ByteWrite> writes;
    std::vector<ByteWrite> rom_patches;
};

struct EntryGroups {
    std::vector<ByteWrite> direct_writes;
    std::vector<ByteWrite> direct_rom_patches;
    std::vector<Group> conditional_groups;
    std::vector<RomGuardGroup> rom_guard_groups;
    bool compatibility_error = false;
};

std::vector<ConditionTerm> condition_terms(const Operation& operation);
std::optional<std::uint32_t> compact_write_address(std::uint32_t address);
std::optional<std::uint32_t> compact_condition_address(std::uint32_t address);
bool is_rom_address(std::uint32_t address);
bool rom_patch_is_direct_image_write(const Operation& operation);
std::vector<ByteWrite> flatten(std::uint32_t address,
                               std::uint32_t value,
                               std::uint8_t width);
void append_expanded_write(std::vector<ByteWrite>& destination,
                           const Operation& operation);
bool equal_condition(const std::vector<ByteWrite>& left,
                     const std::vector<ByteWrite>& right);
bool is_condition(OperationKind kind);
EntryGroups build_groups(const CheatEntry& entry,
                         const Options& options,
                         std::vector<std::string>& warnings);
std::size_t runtime_records(const Group& group);
std::size_t runtime_records(const RomGuardGroup& group);
std::size_t runtime_records(const EntryGroups& groups);

class SectionNameAllocator {
public:
    explicit SectionNameAllocator(std::size_t maximum_length);
    std::string make(std::string_view preferred,
                     std::size_t reserved_suffix_bytes = 0U);

private:
    std::size_t maximum_length_;
    std::unordered_set<std::string> used_;
};

std::vector<std::string> emit_byte_run_tokens(
    const std::vector<ByteWrite>& input,
    bool condition,
    std::vector<std::string>& warnings);
std::vector<std::string> emit_rom_byte_run_tokens(
    const std::vector<ByteWrite>& input,
    std::vector<std::string>& warnings);
std::string join_tokens(const std::vector<std::string>& tokens);
bool emit_wrapped_key(std::ostringstream& output,
                      std::string_view prefix,
                      const std::vector<std::string>& tokens,
                      std::size_t maximum_line_length);

struct EncodedGroup {
    std::string prefix;
    std::vector<std::string> write_tokens;
    std::string full;
};

std::optional<EncodedGroup> encode_group(
    const Group& group,
    std::vector<std::string>& warnings,
    std::size_t maximum_line_length);
void emit_section_header(std::ostringstream& output,
                         SectionNameAllocator& names,
                         std::string_view preferred);
void emit_direct_section(std::ostringstream& output,
                         const CheatEntry& entry,
                         const std::vector<ByteWrite>& direct_writes,
                         bool also_has_conditions,
                         std::size_t maximum_line_length,
                         SectionNameAllocator& names,
                         Result& result);
void emit_rom_section(std::ostringstream& output,
                      const CheatEntry& entry,
                      const std::vector<ByteWrite>& rom_patches,
                      std::string_view suffix,
                      std::size_t maximum_line_length,
                      SectionNameAllocator& names,
                      Result& result);
void emit_direct_and_rom_section(
    std::ostringstream& output,
    const CheatEntry& entry,
    const std::vector<ByteWrite>& direct_writes,
    const std::vector<ByteWrite>& rom_patches,
    bool also_has_other_groups,
    std::size_t maximum_line_length,
    SectionNameAllocator& names,
    Result& result);
bool emit_conditional_groups_with_rom_tail(
    std::ostringstream& output,
    const std::vector<EncodedGroup>& groups,
    const std::vector<std::string>& rom_tokens,
    std::size_t maximum_line_length);
void emit_rom_guard_sections(std::ostringstream& output,
                             const CheatEntry& entry,
                             const std::vector<RomGuardGroup>& groups,
                             bool also_has_other_groups,
                             std::size_t maximum_line_length,
                             SectionNameAllocator& names,
                             Result& result);
void emit_conditional_sections(std::ostringstream& output,
                               const CheatEntry& entry,
                               const std::vector<Group>& source_groups,
                               bool also_has_direct_writes,
                               const Options& options,
                               std::size_t maximum_line_length,
                               SectionNameAllocator& names,
                               Result& result);

std::string_view condition_key_for_kind(OperationKind kind);
std::vector<std::uint8_t> operation_payload(const Operation& operation);
CheatEntry optimize_enhanced_v4_entry(const CheatEntry& entry);

struct EnhancedEncodedOption {
    std::vector<std::string> commands;
    std::size_t runtime_records = 0U;
    std::size_t runtime_write_work = 0U;
};

std::optional<EnhancedEncodedOption> encode_enhanced_v4_option(
    const CheatEntry& entry,
    std::vector<std::string>& warnings);
Result export_enhanced_v4(const CheatDocument& document,
                          const Options& options);

} // namespace gba::ezflash::detail
