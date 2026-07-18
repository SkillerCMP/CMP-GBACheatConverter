#include "cli/cli_app.hpp"

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::vector<std::string> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

#ifdef _WIN32
    if (_isatty(_fileno(stdout)) &&
        gba::cli::would_write_binary_to_stdout(arguments)) {
        std::cerr
            << "error: Binary output cannot be printed to the console. "
               "Use --output FILE or redirect stdout with > FILE.\n";
        return 2;
    }
#endif

    return gba::cli::run(arguments, std::cin, std::cout, std::cerr,
                         "GbaCheatConverterCLI");
}
