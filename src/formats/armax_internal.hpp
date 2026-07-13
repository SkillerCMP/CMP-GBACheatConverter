#pragma once

#include "formats/armax.hpp"
#include "crypto/tea.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace gba::armax::detail {

inline constexpr std::uint32_t kWidthMask = 0x06000000U;
inline constexpr unsigned kWidthShift = 25U;
inline constexpr std::uint32_t kConditionMask = 0x38000000U;
inline constexpr std::uint32_t kActionMask = 0xC0000000U;
inline constexpr std::uint32_t kBaseMask = 0xC0000000U;
inline constexpr std::uint32_t kSpecialBit = 0x01000000U;

inline constexpr std::uint32_t kActionNext = 0x00000000U;
inline constexpr std::uint32_t kActionNextTwo = 0x40000000U;
inline constexpr std::uint32_t kActionBlock = 0x80000000U;
inline constexpr std::uint32_t kActionDisable = 0xC0000000U;

inline constexpr std::uint32_t kBaseAssign = 0x00000000U;
inline constexpr std::uint32_t kBaseIndirect = 0x40000000U;
inline constexpr std::uint32_t kBaseAdd = 0x80000000U;
inline constexpr std::uint32_t kBaseOther = 0xC0000000U;

inline constexpr std::uint32_t kCondEqual = 0x08000000U;
inline constexpr std::uint32_t kCondNotEqual = 0x10000000U;
inline constexpr std::uint32_t kCondLessSigned = 0x18000000U;
inline constexpr std::uint32_t kCondGreaterSigned = 0x20000000U;
inline constexpr std::uint32_t kCondLessUnsigned = 0x28000000U;
inline constexpr std::uint32_t kCondGreaterUnsigned = 0x30000000U;
inline constexpr std::uint32_t kCondAnd = 0x38000000U;

inline constexpr std::uint32_t kSpecialMask = 0xFF000000U;
inline constexpr std::uint32_t kSpecialEnd = 0x00000000U;
inline constexpr std::uint32_t kSpecialSlowdown = 0x08000000U;
inline constexpr std::uint32_t kSpecialButton8 = 0x10000000U;
inline constexpr std::uint32_t kSpecialButton16 = 0x12000000U;
inline constexpr std::uint32_t kSpecialButton32 = 0x14000000U;
inline constexpr std::uint32_t kSpecialPatch1 = 0x18000000U;
inline constexpr std::uint32_t kSpecialPatch2 = 0x1A000000U;
inline constexpr std::uint32_t kSpecialPatch3 = 0x1C000000U;
inline constexpr std::uint32_t kSpecialPatch4 = 0x1E000000U;
inline constexpr std::uint32_t kSpecialEndIf = 0x40000000U;
inline constexpr std::uint32_t kSpecialElse = 0x60000000U;
inline constexpr std::uint32_t kSpecialFill8 = 0x80000000U;
inline constexpr std::uint32_t kSpecialFill16 = 0x82000000U;
inline constexpr std::uint32_t kSpecialFill32 = 0x84000000U;

enum class PendingKind {
    None,
    Fill,
    RomPatch,
    ButtonWrite
};

struct Pending {
    PendingKind kind = PendingKind::None;
    std::uint32_t address = 0;
    std::uint8_t width = 0;
    std::uint32_t parameter = 0;
    std::size_t source_line = 0;
    std::string source_text;
};

struct BlockFrame {
    std::size_t condition_index = 0;
    bool else_seen = false;
};

std::optional<RawLine> parse_line(std::string_view raw,
                                  std::size_t line_number);
std::string clean_name(std::string line);
std::uint8_t decode_width(std::uint32_t op1);
std::uint32_t decode_address(std::uint32_t encoded);
std::optional<std::uint32_t> encode_address(std::uint32_t address);
std::uint32_t width_mask(std::uint8_t width);
std::uint32_t width_bits(std::uint8_t width);
Operation make_operation(OperationKind kind,
                         const RawLine& line,
                         std::uint32_t address,
                         std::uint32_t value,
                         std::uint8_t width,
                         std::string note = {});
bool is_condition_kind(OperationKind kind);
OperationKind decode_condition_kind(std::uint32_t op1);
std::uint32_t condition_bits(OperationKind kind);
std::string format_line(
    std::uint32_t op1,
    std::uint32_t op2,
    bool encrypted,
    const crypto::TeaKey& key = crypto::ProActionReplayV3Key);
void add_warning(CheatDocument& document,
                 const RawLine& line,
                 const std::string& message);
void add_unsupported(CheatEntry& entry,
                     CheatDocument& document,
                     const RawLine& line,
                     std::uint32_t address,
                     std::uint32_t value,
                     std::uint8_t width,
                     const std::string& reason);

Result transform_text_impl(std::string_view input,
                           bool input_encrypted,
                           bool output_encrypted);
CheatDocument parse_document(std::string_view input,
                             const ParseOptions& options);
Result export_document_impl(const CheatDocument& document,
                            const ExportOptions& options);

} // namespace gba::armax::detail
