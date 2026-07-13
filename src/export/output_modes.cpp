#include "export/output_modes.hpp"

#include "export/output_modes_internal.hpp"

namespace gba::output_modes {

Result export_document(const CheatDocument& document,
                       Format format,
                       const Options& options) {
    switch (format) {
    case Format::ArmaxDsc:
        return detail::export_armax_dsc(document, options);
    case Format::VisualBoyAdvanceClt:
        return detail::export_vba_clt(document);
    case Format::MyBoyCht:
        return detail::export_myboy(document);
    case Format::MisterZip:
        return detail::export_mister(document);
    case Format::MednafenCht:
        return detail::export_mednafen(document, options);
    case Format::MgbaCheats:
        return detail::export_mgba(document);
    case Format::LibretroCht:
        return detail::export_libretro(document);
    case Format::EzFlashCht:
        return detail::export_ezflash(document, options);
    }
    Result result;
    result.success = false;
    result.warnings.push_back("Unknown native output format.");
    return result;
}

} // namespace gba::output_modes
