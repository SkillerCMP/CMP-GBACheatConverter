#include "formats/gameshark.hpp"

#include "formats/gameshark_internal.hpp"

namespace gba::gameshark {

CheatDocument parse(std::string_view input, const ParseOptions& options) {
    return detail::parse_document(input, options);
}

Result export_document(const CheatDocument& document,
                       const ExportOptions& options) {
    return detail::encode_document(document, options);
}

} // namespace gba::gameshark
