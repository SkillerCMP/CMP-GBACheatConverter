#pragma once

#include "core/types.hpp"
#include "formats/codebreaker.hpp"
#include "formats/ezflash.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::cli::detail {

enum class Action {
    Convert,
    Version,
    Help,
    ListFormats,
    DetectOnly
};

struct Options {
    Action action = Action::Convert;
    std::string from;
    std::string to;
    std::string crypt_format;
    std::string input_path;
    std::string output_path;
    std::string game_name;
    std::string rom_md5;
    bool encrypt = false;
    bool decrypt = false;
    bool show_warnings = true;
    gba::ezflash::Mode ez_mode = gba::ezflash::Mode::Enhanced;
    std::optional<gba::codebreaker::Seed> cb_input_seed;
    std::optional<gba::codebreaker::Seed> cb_output_seed;
};

Options parse_arguments(const std::vector<std::string>& arguments);
void usage(std::ostream& error_stream, std::string_view program_name);
void list_formats(std::ostream& output_stream);

std::string read_input(const std::string& path, std::istream& input_stream);
void write_output_file(const std::string& path, std::string_view data);
void print_warnings(const std::vector<std::string>& warnings,
                    std::ostream& error_stream);

int run_crypto(const Options& options,
               std::string_view input,
               std::ostream& output_stream);

void resolve_auto_format(Options& options,
                         std::string_view input,
                         std::ostream& error_stream);

bool is_direct_armax_transform(const Options& options);
int run_direct_armax_transform(const Options& options,
                               std::string_view input,
                               std::ostream& output_stream,
                               std::ostream& error_stream);

CheatDocument parse_document(const Options& options, std::string_view input);
int export_document(const Options& options,
                    const CheatDocument& document,
                    std::ostream& output_stream,
                    std::ostream& error_stream);

} // namespace gba::cli::detail
