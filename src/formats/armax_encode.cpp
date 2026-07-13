#include "formats/armax_encode_internal.hpp"

namespace gba::armax::detail {

Result export_document_impl(const CheatDocument& document,
                            const ExportOptions& options) {
    return export_encoded_document(document, options);
}

} // namespace gba::armax::detail
