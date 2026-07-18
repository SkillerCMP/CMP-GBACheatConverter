#pragma once

#include "cli/cli_app.hpp"
#include "core/cmp.hpp"
#include "core/detect.hpp"
#include "core/inline_notes.hpp"
#include "core/text.hpp"
#include "core/version.hpp"
#include "crypto/tea.hpp"
#include "export/output_modes.hpp"
#include "import/native_input.hpp"
#include "formats/armax.hpp"
#include "formats/codebreaker.hpp"
#include "formats/ezflash.hpp"
#include "formats/gameshark.hpp"
#include "formats/xploder.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace gba::tests {

using ByteMap = std::map<std::uint32_t, std::uint8_t>;
using TestFunction = void (*)();

struct TestCase {
    const char* name;
    TestFunction function;
};

void require(bool condition, const std::string& message);
ByteMap direct_write_bytes(const gba::CheatDocument& document);

} // namespace gba::tests
