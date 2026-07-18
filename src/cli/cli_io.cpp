#include "cli/cli_internal.hpp"

#include <filesystem>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace gba::cli::detail {

std::string read_input(const std::string& path, std::istream& input_stream) {
    std::string raw_input;
    if (path == "-") {
        std::ostringstream input;
        input << input_stream.rdbuf();
        raw_input = input.str();
    } else {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Unable to open input file: " + path);
        }

        std::ostringstream data;
        data << input.rdbuf();
        raw_input = data.str();
    }

    return raw_input;
}

void write_output_file(const std::string& path, std::string_view data) {
    if (path.empty() || path == "-") {
        throw std::runtime_error("A real output path is required");
    }
    if (data.empty()) {
        throw std::runtime_error(
            "Conversion produced no output; no file was written");
    }

    const std::filesystem::path target(path);
    std::filesystem::path temporary = target;
    temporary += ".tmp";

    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);

    {
        std::ofstream output(
            temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error(
                "Unable to create output file: " + path);
        }
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
        output.flush();
        if (!output) {
            output.close();
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error(
                "Unable to write complete output file: " + path);
        }
    }

    const auto written_size = std::filesystem::file_size(temporary, ignored);
    if (ignored || written_size != data.size()) {
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error(
            "Output verification failed; no file was written: " + path);
    }

    std::filesystem::path backup = target;
    backup += ".bak.tmp";
    std::filesystem::remove(backup, ignored);

    const bool target_existed = std::filesystem::exists(target, ignored);
    if (target_existed) {
        std::error_code backup_error;
        std::filesystem::rename(target, backup, backup_error);
        if (backup_error) {
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error(
                "Unable to prepare existing output file for replacement: " +
                path);
        }
    }

    std::error_code rename_error;
    std::filesystem::rename(temporary, target, rename_error);
    if (rename_error) {
        if (target_existed) {
            std::error_code restore_error;
            std::filesystem::rename(backup, target, restore_error);
        }
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error(
            "Unable to replace output file: " + path);
    }

    if (target_existed) {
        std::filesystem::remove(backup, ignored);
    }
}

void print_warnings(const std::vector<std::string>& warnings,
                    std::ostream& error_stream) {
    for (const std::string& warning : warnings) {
        error_stream << "warning: " << warning << '\n';
    }
}

} // namespace gba::cli::detail
