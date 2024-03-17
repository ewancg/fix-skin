#include "Magick++.h"
#include <filesystem>
#include <iostream>

using namespace std;
using namespace Magick;

template<typename... Ts>
static inline void error(const Ts... msg) noexcept
{
    std::cerr << "fix-skin: ";
    (
        [&] {
            std::cerr << msg;
            std::cout << msg;
        }(),
        ...);
    std::cerr << std::endl;
    std::cout << std::endl;
}

static constexpr const char g_help_text[] =
    R"GORP(fix-skin: primitively adjust a DDNet skin so that it will no longer error in the client
Usage:
   fix-skin file:input file:output

   -h,  --help       Show this message
   -v,  --verbose    Output progress

Return codes:
   -*   Incorrect invocation
    0   Success
    1   Process error
)GORP";

enum ErrorStatus {
    UNKNOWN = -1,
    OK = 0,
    ARGS_LT,
    ARGS_GT,
    INFILE_NE,
    OUTPATH_NE,
    OUTFILE_CONFLICT,
};

using Argument = std::tuple<const std::string, const std::string>;
constexpr const Argument s_help_arg_strs = std::make_tuple("-h", "--help");
constexpr const Argument s_verbose_arg_strs = std::make_tuple("-v", "--verbose");
constexpr const std::array<Argument, 2> s_args = {s_help_arg_strs, s_verbose_arg_strs};

bool verbose = false;
bool help_requested = false;
std::vector<std::string> unknown_args;

void print_help(char *argv[], ErrorStatus status = OK)
{
    switch (status) {
    case OK:
        [[fallthrough]];
    default:
        break;
    case ARGS_LT:
        error("Not enough arguments.");
        break;
    case ARGS_GT:
        error("Too many arguments.");
        break;
    case INFILE_NE:
        error("Input file ",
              std::filesystem::path(argv[1]).string(),
              " is inaccessible or does not exist.");
        break;
    case OUTPATH_NE:
        error("Output file directory ",
              std::filesystem::path(argv[2]).parent_path().string(),
              " is unreadable or does not exist.");
        break;
    case OUTFILE_CONFLICT:
        error("Output file already exists.");
        break;
    case UNKNOWN:
        error("Unknown error.");
        break;
    }
    std::cout << g_help_text << std::endl;
}

bool is_arg(const char *str, const Argument &arg)
{
    return str == std::get<0>(arg) || str == std::get<1>(arg);
}

const Argument *is_arg(const char *str)
{
    for (const auto &it : s_args) {
        if (is_arg(str, it))
            return &it;
    }
    return nullptr;
}

const std::integral auto roundDimension(const std::integral auto x, const int multiple)
{
    const std::integral auto remainder = x % multiple;
    return remainder ? x + multiple - remainder : x;
}

ErrorStatus sane(int argc, char *argv[])
{
    for (auto i = argc - 1; i > 0; i--) {
        if (std::string_view(argv[i]).starts_with('-')) {
            auto arg = is_arg(argv[i]);
            if (arg) {
                if (arg == &s_help_arg_strs) {
                    help_requested = true;
                    return OK;
                } else if (arg == &s_verbose_arg_strs) {
                    verbose = true;
                }
            } else {
                unknown_args.push_back(argv[i]);
            }
        }
    }

    if (!unknown_args.empty()) {
        std::string str;
        for (const auto &it : unknown_args) {
            str += (str.empty() ? "" : ", ") + it;
            std::cout << "Ignoring unknown argument(s) " << str << std::endl;
        }
    }

    switch (argc) {
    case 3:
        if (!std::filesystem::exists(argv[1]))
            return INFILE_NE;
        if (!std::filesystem::exists(std::filesystem::path(argv[2]).parent_path()))
            return OUTPATH_NE;
        if (std::filesystem::exists(argv[2]))
            return OUTFILE_CONFLICT;
        [[fallthrough]];
    case 2:
        break;
    case 1:
        [[fallthrough]];
    case 0: // Impossible
        return ARGS_LT;
    default:
        return ARGS_GT;
    }

    if (argc == 3)
        return OK;
    return ARGS_LT;
}

int main(int argc, char *argv[])
{
    ErrorStatus status = sane(argc, argv);
    if (status || help_requested) {
        if (help_requested)
            status = OK;
        print_help(argv, status);
    } else {
        std::string input_file = std::filesystem::absolute(argv[1]).string(),
                    output_file = std::filesystem::absolute(argv[2]).string();

        InitializeMagick(*argv);
        try {
            Image image;
            // Read a file into image object
            if (verbose)
                std::cout << "Reading from " << input_file << std::endl;
            image.read(input_file);
            auto geometry = image.size();

            auto w = roundDimension(geometry.width(), 8);
            auto h = roundDimension(geometry.height(), 4);

            if (verbose) {
                std::cout << std::to_string(geometry.width()) + "x"
                                 + std::to_string(geometry.height())
                          << " -> " << std::to_string(w) + "x" + std ::to_string(h) << std::endl;
            }

            geometry.xOff(0);
            geometry.yOff(0);
            geometry.width(w);
            geometry.height(h);
            geometry.aspect(true);

            image.alpha(true);
            image.depth(8);
            image.colorSpace(ColorspaceType::sRGBColorspace);
            image.resize(geometry);

            // Write the image to a file
            if (verbose)
                std::cout << "Writing to " << output_file << std::endl;
            image.write(output_file);
        } catch (Exception &e) {
            status = UNKNOWN;
            error("Caught exception: ", e.what());
        }
    }
    return -status;
}
