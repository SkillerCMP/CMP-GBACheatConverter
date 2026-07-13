#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace gba::ezflash {

enum class Mode {
    Original,
    Enhanced
};

struct Options {
    std::size_t maximum_runtime_records = 128;
    std::size_t maximum_section_name_length = 49;
    std::size_t maximum_physical_line_length = 298;
    Mode mode = Mode::Enhanced;
    bool combine_multiple_if_groups = true;
};

struct Result {
    std::string text;
    std::vector<std::string> warnings;
    bool success = true;
};

CheatDocument parse(std::string_view input);

Result export_document(const CheatDocument& document,
                       const Options& options = {});

} // namespace gba::ezflash
