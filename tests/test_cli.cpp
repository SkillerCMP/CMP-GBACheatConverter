#include "test_support.hpp"

namespace gba::tests {

void test_release_version() {
    require(std::string(GBA_CHEAT_VERSION) == "2.00",
            "Configured release version is not v2.00");
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
                    output.str() == "GBA Cheat Converter v2.00\n" &&
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

} // namespace gba::tests
