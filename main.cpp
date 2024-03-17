#include "Magick++.h"
#include <filesystem>
#include <iostream>

using namespace std;
using namespace Magick;

#define ERROR(a) \
    std::cerr << "fix-skin: " << a << std::endl; \
    std::cout << a << std::endl; \
    break;

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

enum InvocationError {
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

void print_help(char *argv[], InvocationError error = OK)
{
    switch (error) {
    case OK:
        [[fallthrough]];
    default:
        break;
    case ARGS_LT:
        ERROR("Not enough arguments.")
    case ARGS_GT:
        ERROR("Too many arguments.")
    case INFILE_NE:
        ERROR("Input file " + std::filesystem::path(argv[1]).string()
              + " is inaccessible or does not exist.")
    case OUTPATH_NE:
        ERROR("Output file directory " + std::filesystem::path(argv[2]).parent_path().string()
              + " is unreadable or does not exist.")
    case OUTFILE_CONFLICT:
        ERROR("Output file already exists.")
    case UNKNOWN:
        ERROR("Unknown error.")
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

InvocationError sane(int argc, char *argv[])
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
    InvocationError error = sane(argc, argv);
    if (error || help_requested) {
        if (help_requested)
            error = OK;
        print_help(argv, error);
        return -error;
    }
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
            std::cout << std::to_string(geometry.width()) + "x" + std::to_string(geometry.height())
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
    } catch (Exception &error_) {
        cout << "Caught exception: " << error_.what() << endl;
        error = UNKNOWN;
    }
    return -error;
}
