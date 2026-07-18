cmake_minimum_required(VERSION 3.20)

# The source ZIP may be extracted with timestamps ahead of the local clock.
# Suppress Visual Studio's automatic CMake regeneration so parallel targets
# cannot race while regenerating configured version resources.
set(CMAKE_SUPPRESS_REGENERATION ON)

project(GbaCheatConverter VERSION 2.15.0 LANGUAGES CXX)

set(GBA_DISPLAY_VERSION "2.15")
set(GBA_FILE_VERSION "2.15.0.0")
if(PROJECT_VERSION_TWEAK STREQUAL "")
    set(GBA_VERSION_TWEAK 0)
else()
    set(GBA_VERSION_TWEAK "${PROJECT_VERSION_TWEAK}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GBA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

function(gba_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus
        )
        if(GBA_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow -Wconversion -Wsign-conversion
        )
        if(GBA_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/core/version.hpp"
    @ONLY
)

add_library(gba_cheat_core
    src/core/text.cpp
    src/core/text_lines.cpp
    src/core/text_hex.cpp
    src/core/text_code_rows.cpp
    src/core/text_headers.cpp
    src/core/text_gamehacking.cpp
    src/core/cmp_input.cpp
    src/core/cmp_output.cpp
    src/core/detect.cpp
    src/core/detect_common.cpp
    src/core/detect_crypto.cpp
    src/core/detect_gameshark.cpp
    src/core/detect_armax.cpp
    src/core/detect_format.cpp
    src/core/inline_notes.cpp
    src/core/inline_notes_parse.cpp
    src/core/inline_notes_render.cpp
    src/crypto/tea.cpp
    src/formats/codebreaker.cpp
    src/formats/codebreaker_cipher.cpp
    src/formats/codebreaker_decode.cpp
    src/formats/codebreaker_encode.cpp
    src/formats/codebreaker_encode_common.cpp
    src/formats/codebreaker_encode_conditions.cpp
    src/formats/codebreaker_encode_operations.cpp
    src/formats/codebreaker_encode_document.cpp
    src/formats/codebreaker_parse.cpp
    src/formats/codebreaker_raw.cpp
    src/formats/codebreaker_seed.cpp
    src/formats/gameshark.cpp
    src/formats/gameshark_assignment.cpp
    src/formats/gameshark_common.cpp
    src/formats/gameshark_encode.cpp
    src/formats/gameshark_parse.cpp
    src/formats/armax.cpp
    src/formats/armax_common.cpp
    src/formats/armax_encode.cpp
    src/formats/armax_encode_common.cpp
    src/formats/armax_encode_operations.cpp
    src/formats/armax_encode_conditions.cpp
    src/formats/armax_encode_document.cpp
    src/formats/armax_parse.cpp
    src/formats/armax_transform.cpp
    src/formats/xploder.cpp
    src/formats/vba_clt_codec.cpp
    src/formats/retroarch_cht.cpp
    src/formats/mgba_cheats.cpp
    src/formats/mister_gg.cpp
    src/formats/mednafen_cht.cpp
    src/formats/ezflash.cpp
    src/formats/ezflash_groups.cpp
    src/formats/ezflash_tokens.cpp
    src/formats/ezflash_enhanced_plan.cpp
    src/formats/ezflash_optimize.cpp
    src/formats/ezflash_enhanced_emit.cpp
    src/formats/ezflash_parse.cpp
    src/formats/ezflash_parse_runs.cpp
    src/formats/ezflash_parse_runtime.cpp
    src/formats/ezflash_parse_conditions.cpp
    src/formats/ezflash_parse_document.cpp
    src/export/output_modes.cpp
    src/export/output_common.cpp
    src/export/output_text.cpp
    src/export/output_armax_dsc.cpp
    src/export/output_vba_clt.cpp
    src/export/output_mister.cpp
    src/export/zip_store.cpp
    src/import/native_input.cpp
    src/import/native_input_common.cpp
    src/import/native_input_binary.cpp
    src/import/native_input_text.cpp
    src/import/native_input_myboy.cpp
    src/import/native_input_mgba.cpp
    src/import/native_input_libretro.cpp
    src/import/native_input_mister.cpp
    src/import/native_input_mednafen.cpp
    src/cli/cli_app.cpp
    src/cli/cli_arguments.cpp
    src/cli/cli_io.cpp
    src/cli/cli_crypto.cpp
    src/cli/cli_conversion.cpp
)

target_include_directories(gba_cheat_core PUBLIC
    src
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
gba_enable_warnings(gba_cheat_core)

add_executable(gba-cheat-converter-cli src/cli/main.cpp)
target_link_libraries(gba-cheat-converter-cli PRIVATE gba_cheat_core)
set_target_properties(gba-cheat-converter-cli PROPERTIES
    OUTPUT_NAME "GbaCheatConverterCLI"
)
gba_enable_warnings(gba-cheat-converter-cli)

if(WIN32)
    enable_language(RC)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.manifest.in"
        "${CMAKE_CURRENT_BINARY_DIR}/app.manifest"
        @ONLY
    )
    file(TO_CMAKE_PATH
        "${CMAKE_CURRENT_BINARY_DIR}/app.manifest"
        GBA_MANIFEST_PATH
    )
    file(TO_CMAKE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/GbaCheatConverter.ico"
        GBA_ICON_PATH
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
        @ONLY
    )

    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR
            "The Win32 GUI release build is 64-bit only. Configure with -A x64."
        )
    endif()

    add_executable(GbaCheatConverter WIN32
        src/gui/main_win.cpp
        src/gui/gui_state.cpp
        src/gui/gui_text.cpp
        src/gui/gui_formats.cpp
        src/gui/gui_conversion.cpp
        src/gui/gui_files.cpp
        src/gui/gui_settings.cpp
        src/gui/gui_layout.cpp
        src/gui/gui_controls.cpp
        src/gui/gui_commands.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
    )
    target_link_libraries(GbaCheatConverter PRIVATE
        gba_cheat_core
        comdlg32
        shell32
        user32
        gdi32
        bcrypt
    )
    target_compile_definitions(GbaCheatConverter PRIVATE
        UNICODE
        _UNICODE
        WINVER=0x0601
        _WIN32_WINNT=0x0601
    )
    gba_enable_warnings(GbaCheatConverter)
    if(MSVC)
        # app.rc already embeds the configured manifest as RT_MANIFEST ID 1.
        # Disable MSVC's automatically generated manifest to avoid CVT1100.
        target_link_options(GbaCheatConverter PRIVATE /MANIFEST:NO)
    endif()
endif()

enable_testing()
add_executable(gba-cheat-tests
    tests/test_main.cpp
    tests/test_support.cpp
    tests/test_cli.cpp
    tests/test_core.cpp
    tests/test_text_cleanup.cpp
    tests/test_detection.cpp
    tests/test_codebreaker.cpp
    tests/test_gameshark.cpp
    tests/test_armax.cpp
    tests/test_xploder.cpp
    tests/test_ezflash_original.cpp
    tests/test_ezflash_grouped.cpp
    tests/test_ezflash_enhanced.cpp
    tests/test_native_outputs.cpp
    tests/test_retroarch.cpp
    tests/test_mgba.cpp
    tests/test_mister.cpp
    tests/test_mednafen.cpp
    tests/test_integration.cpp
    tests/test_native_inputs.cpp
    tests/test_vba_clt.cpp
    tests/test_cmp.cpp
)
target_link_libraries(gba-cheat-tests PRIVATE gba_cheat_core)
gba_enable_warnings(gba-cheat-tests)
add_test(NAME gba-cheat-tests COMMAND gba-cheat-tests)
add_test(NAME gba-cheat-cli-version
         COMMAND gba-cheat-converter-cli --version)
set_tests_properties(gba-cheat-cli-version PROPERTIES
    PASS_REGULAR_EXPRESSION "GBA Cheat Converter v2.15"
)
if(WIN32)
    add_test(NAME gba-cheat-unified-cli-version
             COMMAND GbaCheatConverter --version)
    set_tests_properties(gba-cheat-unified-cli-version PROPERTIES
        PASS_REGULAR_EXPRESSION "GBA Cheat Converter v2.15"
    )
endif()
