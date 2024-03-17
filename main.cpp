#include "Magick++.h"
#include <filesystem>
#include <iostream>

using namespace std;
using namespace Magick;

#define ERROR(a) \
    std::cout << a << std::endl; \
    break;
static constexpr const char g_help_text[] =
    R"GORP(fix-skin: primitively adjust DDNet skins so that they will no longer error in the client
Usage:
    fix-skin file:input file:output

Return codes:
   -*: Incorrect invocation
    0: Success
    1: Process error
)GORP";

enum InvocationError {
    SANE = 0,
    ARGS_LT,
    ARGS_GT,
    INFILE_NE,
    OUTPATH_NE,
    OUTFILE_CONFLICT,
    ARGS_UNKNOWN
};
using Argument = std::tuple<const std::string, const std::string>;
constexpr const Argument s_help_arg_strs = std::make_tuple("-h", "--help");
constexpr const Argument s_verbose_arg_strs = std::make_tuple("-v", "--verbose");
constexpr const std::array<Argument, 2> s_args = {s_help_arg_strs, s_verbose_arg_strs};

bool verbose = false;
std::vector<std::string> unknown_args;

void print_help(char *argv[], InvocationError error = SANE)
{
    switch (error) {
    case SANE:
        [[fallthrough]];
    default:
        break;
    case ARGS_LT:
        ERROR("Not enough arguments.")
    case ARGS_GT:
        ERROR("Too many arguments.")
    case ARGS_UNKNOWN: {
        std::string str;
        for (int i = unknown_args.size() - 1; i >= 0; i--) {
            str += unknown_args.at(i) + (i == 0 ? "" : " ");
        }
        ERROR("Unknown argument[s]: " + str);
    }
    case INFILE_NE:
        ERROR("Input file " + std::filesystem::path(argv[1]).string()
              + " is inaccessible or does not exist.")
    case OUTPATH_NE:
        ERROR("Output file directory " + std::filesystem::path(argv[2]).parent_path().string()
              + " is unreadable or does not exist.")
    case OUTFILE_CONFLICT:
        ERROR("Output file already exists.")
    }
    std::cout << g_help_text << std::endl;
}

bool is_arg(const char *str, const Argument &arg)
{
    std::string lhs = std::get<0>(arg);
    std::string rhs = std::get<1>(arg);
    return lhs == str || rhs == str;
}

bool is_arg(const char *str)
{
    for (auto i = 0; i < s_args.size(); i++) {
        if (is_arg(str, s_args[i]))
            return true;
    };
    return false;
}

InvocationError sane(int argc, char *argv[])
{
    switch (argc) {
    case 3:
        if (std::string(argv[2]).starts_with('-') && !is_arg(argv[2])) {
            unknown_args.push_back(argv[2]);
        } else {
            if (!std::filesystem::exists(argv[1]))
                return INFILE_NE;
            if (!std::filesystem::exists(std::filesystem::path(argv[2]).parent_path()))
                return OUTPATH_NE;
            if (std::filesystem::exists(argv[2]))
                return OUTFILE_CONFLICT;
        }
        [[fallthrough]];
    case 2:
        if (std::string(argv[1]).starts_with('-') && !is_arg(argv[1])) {
            unknown_args.push_back(argv[1]);
        }
        break;
    case 1:
        [[fallthrough]];
    case 0:
        return ARGS_LT;
    default:
        return ARGS_GT;
    }

    if (!unknown_args.empty())
        return ARGS_UNKNOWN;
    if (argc == 3)
        return SANE;
    return ARGS_LT;
}

int main(int argc, char *argv[])
{
    InvocationError error = sane(argc, argv);
    bool help_requested = argc >= 2 && is_arg(argv[1], s_help_arg_strs);
    if (error || help_requested) {
        if (help_requested)
            error = SANE;
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

        auto w = geometry.width();
        if (w % 8 != 0)
            w = ((w + 8 - 1) / 8) * 8;

        auto h = geometry.height();
        if (h % 8 != 0)
            h = ((h + 8 - 1) / 8) * 8;

        geometry.xOff(0);
        geometry.yOff(0);
        geometry.width(w);
        geometry.height(h);

        if (verbose) {
            std::cout << std::to_string(geometry.width()) + "x" + std ::to_string(geometry.height())
                      << " -> " << std::to_string(w) + "x" + std ::to_string(h) << std::endl;
        }

        image.resize(geometry);
        image.colorSpace(ColorspaceType::sRGBColorspace);

        image.depth(8);

        // Write the image to a file
        if (verbose)
            std::cout << "Writing to " << output_file << std::endl;
        image.write(output_file);
    } catch (Exception &error_) {
        cout << "Caught exception: " << error_.what() << endl;
        return 1;
    }
    return 0;
}

// todo add verbose
