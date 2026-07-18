#include "import/native_input_internal.hpp"

#include "formats/mednafen_cht.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::native_input::detail {
namespace {

OperationKind condition_kind(mednafen_cht::ConditionOperator operation) {
    using Operator = mednafen_cht::ConditionOperator;
    switch (operation) {
    case Operator::GreaterOrEqual: return OperationKind::IfGreaterOrEqual;
    case Operator::LessOrEqual: return OperationKind::IfLessOrEqual;
    case Operator::Greater: return OperationKind::IfGreater;
    case Operator::Less: return OperationKind::IfLess;
    case Operator::Equal: return OperationKind::IfEqual;
    case Operator::NotEqual: return OperationKind::IfNotEqual;
    case Operator::AndNonzero: return OperationKind::IfAnd;
    case Operator::AndZero: return OperationKind::IfNand;
    case Operator::XorNonzero: return OperationKind::IfXor;
    case Operator::XorZero: return OperationKind::IfNotXor;
    case Operator::OrNonzero: return OperationKind::IfOr;
    case Operator::OrZero: return OperationKind::IfNotOr;
    }
    return OperationKind::Unsupported;
}

OperationKind record_kind(char type) {
    switch (type) {
    case 'R': return OperationKind::Write;
    case 'A': return OperationKind::Add;
    case 'T': return OperationKind::Transfer;
    case 'S': return OperationKind::ReadSubstitute;
    case 'C': return OperationKind::CompareReadSubstitute;
    default: return OperationKind::Unsupported;
    }
}

void set_wide_value(Operation& operation, std::uint64_t value) {
    operation.value = static_cast<std::uint32_t>(value & 0xFFFFFFFFULL);
    operation.wide_value = value;
    operation.has_wide_value = value >
        std::numeric_limits<std::uint32_t>::max() || operation.width > 4U;
}

CheatEntry convert_record(const mednafen_cht::Record& record) {
    CheatEntry entry;
    entry.name = record.name;
    entry.enabled = record.enabled;

    MednafenCheatMetadata metadata;
    metadata.rom_md5 = record.rom_md5;
    metadata.game_name = record.game_name;
    metadata.type = record.type;
    metadata.length = record.length;
    metadata.big_endian = record.big_endian;
    metadata.instance_count = record.instance_count;
    metadata.address = record.address;
    metadata.value = record.value;
    metadata.compare = record.compare;
    metadata.repeat_count = record.repeat_count;
    metadata.repeat_address_increment = record.repeat_address_increment;
    metadata.repeat_value_increment = record.repeat_value_increment;
    metadata.copy_source_address = record.copy_source_address;
    metadata.copy_source_increment = record.copy_source_increment;
    metadata.conditions = record.conditions_text.empty()
        ? mednafen_cht::serialize_conditions(record.conditions)
        : record.conditions_text;
    entry.mednafen = std::move(metadata);

    // Conditions affect only periodic R/A/T records in Mednafen. Chain them
    // as nested semantic conditions so a false test skips all remaining tests
    // and the final action. Native metadata remains authoritative.
    if (record.type == 'R' || record.type == 'A' || record.type == 'T') {
        const std::size_t condition_count = record.conditions.size();
        for (std::size_t index = 0U; index < condition_count; ++index) {
            const mednafen_cht::Condition& source = record.conditions[index];
            Operation condition;
            condition.kind = condition_kind(source.operation);
            condition.address = source.address;
            condition.width = source.length;
            condition.big_endian = source.big_endian;
            set_wide_value(condition, source.value);
            condition.condition_span = static_cast<std::uint32_t>(
                condition_count - index);
            entry.operations.push_back(std::move(condition));
        }
    }

    Operation operation;
    operation.kind = record_kind(record.type);
    operation.address = record.address;
    operation.width = record.length;
    operation.big_endian = record.big_endian;
    operation.repeat = record.repeat_count;
    set_wide_value(operation, record.value);
    operation.source_address = record.copy_source_address;
    operation.source_address_step = record.copy_source_increment;
    if (record.repeat_address_increment <=
        static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
        operation.address_step = static_cast<std::int32_t>(
            record.repeat_address_increment);
    }
    operation.wide_value_step = record.repeat_value_increment;
    operation.has_wide_value_step = record.repeat_value_increment >
        static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    if (!operation.has_wide_value_step) {
        operation.value_step = static_cast<std::int32_t>(
            record.repeat_value_increment);
    }
    if (record.type == 'C') {
        operation.wide_compare_value = record.compare;
        operation.has_wide_compare_value = record.compare >
            std::numeric_limits<std::uint32_t>::max() || record.length > 4U;
        operation.encoding_auxiliary = static_cast<std::uint32_t>(
            record.compare & 0xFFFFFFFFULL);
    }
    entry.operations.push_back(std::move(operation));
    return entry;
}

} // namespace

bool looks_like_mednafen(std::string_view text_value) {
    return mednafen_cht::looks_like(text_value);
}

Result import_mednafen(std::string_view, std::string_view text_value) {
    const std::string source_name = "Mednafen .cht";
    const mednafen_cht::ParseResult parsed = mednafen_cht::parse(text_value);
    if (!parsed.recognized) return {};
    if (!parsed.success) {
        return recognized_error(
            SourceFormat::MednafenCht, source_name,
            parsed.warnings.empty()
                ? "The Mednafen .cht file is malformed."
                : parsed.warnings.front());
    }

    CheatDocument document;
    document.warnings = parsed.warnings;
    for (const mednafen_cht::Record& record : parsed.records) {
        document.entries.push_back(convert_record(record));
    }

    Result result;
    result.recognized = true;
    result.success = true;
    result.source_format = SourceFormat::MednafenCht;
    result.source_name = source_name;
    result.document = document;
    result.has_document = true;
    result.warnings = parsed.warnings;

    // Provide editable semantic text when the entire document has an exact
    // representation. Otherwise retain the complete native document without
    // pretending that 64-bit, transfer, read-substitution, or condition data
    // can be represented by a device-code family.
    Result rendered = render_document(
        SourceFormat::MednafenCht, source_name, document,
        {InputFormat::FcdRaw, InputFormat::ActionReplayMaxRaw,
         InputFormat::EzFlash});
    if (rendered.success) {
        result.input_format = rendered.input_format;
        result.text = std::move(rendered.text);
    } else {
        result.warnings.push_back(
            "Mednafen-specific records were retained as a native-only "
            "document for lossless re-export.");
    }
    return result;
}

} // namespace gba::native_input::detail
