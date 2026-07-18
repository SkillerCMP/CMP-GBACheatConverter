#include "test_support.hpp"

#include <filesystem>
#include <fstream>

namespace gba::tests {

void test_release_version() {
    require(std::string(GBA_CHEAT_VERSION) == "2.15",
            "Configured release version is not v2.15");
}

void test_embedded_cli_runner() {
    {
        std::istringstream input;
        std::ostringstream output;
        std::ostringstream error;
        const int result = gba::cli::run(
            {"--version"}, input, output, error,
            "GbaCheatConverter.exe");
        require(result == 0 &&
                    output.str() == "GBA Cheat Converter v2.15\n" &&
                    error.str().empty(),
                "Embedded GUI CLI version mode failed");
    }

    {
        std::istringstream input(
            "Infinite Health\n"
            "82001000 1234\n");
        std::ostringstream output;
        std::ostringstream error;
        const int result = gba::cli::run(
            {"--from", "cb-raw", "--to", "ez",
             "--ez-mode", "original", "-"},
            input, output, error, "GbaCheatConverter.exe");
        require(result == 0 &&
                    output.str().find("[Infinite Health]") !=
                        std::string::npos &&
                    output.str().find("ON=1000,34,12;") !=
                        std::string::npos,
                "Embedded GUI CLI semantic conversion failed");
    }
}

void test_cli_output_file_and_binary_console_guard() {
    require(gba::cli::would_write_binary_to_stdout(
                {"--from", "auto", "--to", "vba-clt", "input.cht"}),
            "VBA CLT stdout was not recognized as binary");
    require(!gba::cli::would_write_binary_to_stdout(
                {"--from", "auto", "--to", "vba-clt", "--output",
                 "output.clt", "input.cht"}),
            "Explicit binary output path still requested binary stdout");
    require(!gba::cli::would_write_binary_to_stdout(
                {"--from", "auto", "--to", "retroarch-cht",
                 "input.cht"}),
            "Text RetroArch output was incorrectly marked binary");

    const std::filesystem::path output_path =
        std::filesystem::temp_directory_path() /
        "gba-cheat-converter-v213-output-test.clt";
    std::error_code ignored;
    std::filesystem::remove(output_path, ignored);
    std::filesystem::remove(output_path.string() + ".tmp", ignored);
    std::filesystem::remove(output_path.string() + ".bak.tmp", ignored);

    std::istringstream input(
        "Infinite Health\n"
        "82001000 1234\n");
    std::ostringstream output;
    std::ostringstream error;
    const int result = gba::cli::run(
        {"--from", "cb-raw", "--to", "vba-clt", "--output",
         output_path.string(), "-"},
        input, output, error, "GbaCheatConverterCLI");

    require(result == 0, "CLI --output VBA CLT conversion failed");
    require(output.str().empty(),
            "CLI --output also wrote binary data to stdout");
    require(std::filesystem::exists(output_path),
            "CLI --output did not create the requested file");
    require(std::filesystem::file_size(output_path) == 96U,
            "CLI --output created an unexpected VBA CLT size");
    require(error.str().find("wrote output:") != std::string::npos,
            "CLI --output did not report a readable completion message");

    std::ifstream file(output_path, std::ios::binary);
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()};
    const native_input::Result imported = native_input::import_file(
        output_path.filename().string(), bytes);
    require(imported.recognized && imported.success && imported.has_document,
            "CLI --output VBA CLT did not re-import successfully");

    {
        std::ofstream existing(output_path, std::ios::binary | std::ios::trunc);
        existing << "KEEP";
    }
    std::istringstream empty_input;
    std::ostringstream failed_stdout;
    std::ostringstream failed_stderr;
    const int failed_result = gba::cli::run(
        {"--from", "cb-raw", "--to", "vba-clt", "--output",
         output_path.string(), "-"},
        empty_input, failed_stdout, failed_stderr,
        "GbaCheatConverterCLI");
    require(failed_result == 1,
            "Empty conversion unexpectedly succeeded with --output");
    std::ifstream preserved(output_path, std::ios::binary);
    const std::string preserved_text{
        std::istreambuf_iterator<char>(preserved),
        std::istreambuf_iterator<char>()};
    require(preserved_text == "KEEP",
            "Failed conversion replaced an existing output file");

    std::filesystem::remove(output_path, ignored);

    const std::filesystem::path short_output_path =
        std::filesystem::temp_directory_path() /
        "gba-cheat-converter-v213-short-output-test.cht";
    std::filesystem::remove(short_output_path, ignored);
    std::istringstream short_input(
        "Infinite Health\n"
        "32001000 0063\n");
    std::ostringstream short_stdout;
    std::ostringstream short_stderr;
    const int short_result = gba::cli::run(
        {"--from", "cb-raw", "--to", "ez", "--ez-mode", "original",
         "-o", short_output_path.string(), "-"},
        short_input, short_stdout, short_stderr,
        "GbaCheatConverterCLI");
    require(short_result == 0 && short_stdout.str().empty() &&
                std::filesystem::exists(short_output_path),
            "CLI -o alias failed");
    std::filesystem::remove(short_output_path, ignored);
}

} // namespace gba::tests
