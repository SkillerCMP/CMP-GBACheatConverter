#pragma once

#include "core/types.hpp"
#include "formats/codebreaker.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace gba::xploder {

// The uploaded Xploder Advance R1 ROM uses the same Future Console Design
// 8+4 dispatcher and seeded cipher as the CodeBreaker/GameShark SP family.
// A separate namespace keeps device labels and future version-specific
// differences isolated from the shared implementation.
using Seed = codebreaker::Seed;
using ParseOptions = codebreaker::ParseOptions;
using ExportOptions = codebreaker::ExportOptions;
using Result = codebreaker::Result;

CheatDocument parse(std::string_view input,
                    const ParseOptions& options = {});
Result export_document(const CheatDocument& document,
                       const ExportOptions& options = {});

} // namespace gba::xploder
