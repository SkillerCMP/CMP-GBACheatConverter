#include "formats/codebreaker.hpp"

#include "formats/codebreaker_internal.hpp"

namespace gba::codebreaker {

CheatDocument parse(std::string_view input, const ParseOptions& options) {
    return detail::parse_document(input, options);
}

Result export_document(const CheatDocument& document,
                       const ExportOptions& options) {
    return detail::export_document_impl(document, options);
}

} // namespace gba::codebreaker
