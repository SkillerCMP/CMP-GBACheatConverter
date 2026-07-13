#include "formats/codebreaker_encode_internal.hpp"

namespace gba::codebreaker::detail {

Result export_document_impl(const CheatDocument& document,
                            const ExportOptions& options) {
    return encode_document(document, options);
}

} // namespace gba::codebreaker::detail
