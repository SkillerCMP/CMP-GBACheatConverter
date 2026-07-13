#include "formats/ezflash.hpp"

#include "formats/ezflash_parse_internal.hpp"

#include <string_view>

namespace gba::ezflash {

CheatDocument parse(std::string_view input) {
    return parse_detail::parse_document(input);
}

} // namespace gba::ezflash
