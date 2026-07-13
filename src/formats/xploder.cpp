#include "formats/xploder.hpp"

namespace gba::xploder {

CheatDocument parse(std::string_view input, const ParseOptions& options) {
    return codebreaker::parse(input, options);
}

Result export_document(const CheatDocument& document,
                       const ExportOptions& options) {
    return codebreaker::export_document(document, options);
}

} // namespace gba::xploder
