#include "formats/armax.hpp"

#include "formats/armax_internal.hpp"

namespace gba::armax {

CheatDocument parse(std::string_view input, const ParseOptions& options) {
    return detail::parse_document(input, options);
}

Result export_document(const CheatDocument& document,
                       const ExportOptions& options) {
    return detail::export_document_impl(document, options);
}

Result transform_text(std::string_view input,
                      bool input_encrypted,
                      bool output_encrypted) {
    return detail::transform_text_impl(
        input, input_encrypted, output_encrypted);
}

} // namespace gba::armax
