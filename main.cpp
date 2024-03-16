#include "Magick++.h"
#include <filesystem>
#include <iostream>

using namespace std;
using namespace Magick;

#define ERROR(a) \
    std::cout << a << std::endl; \
    break;
static constexpr const char g_help_text[] =
    R"GORP(
fix-skin: primitively adjust DDNet skins so that they will no longer error in the client
Usage:
    fix-skin file:input file:output

Return codes:
   -*: Incorrect invocation
    0: Success
    1: Process error
)GORP";

enum WrongInvocation { SANE = 0, ARGS_LT, ARGS_GT, INFILE_NE, OUTPATH_NE, OUTFILE_CONFLICT };

void print_help(int malformed = 0)
{
    switch (malformed) {
    case SANE:
        [[fallthrough]];
    default:
        break;
    case ARGS_LT:
        ERROR("Not enough arguments.")
    case ARGS_GT:
        ERROR("Too many arguments.")
    case INFILE_NE:
        ERROR("Input file is inaccessible or does not exist.")
    case OUTPATH_NE:
        ERROR("Output file directory is unreadable or does not exist.")
    case OUTFILE_CONFLICT:
        ERROR("Output file already exists.")
    }
    std::cout << g_help_text << std::endl;
}

WrongInvocation sane(int argc, char *argv[])
{
    if (argc <= 2)
        return ARGS_LT;
    if (argc >= 4)
        return ARGS_GT;
    if (!std::filesystem::exists(argv[1]))
        return INFILE_NE;
    if (!std::filesystem::exists(std::filesystem::path(argv[2]).parent_path()))
        return OUTPATH_NE;
    if (std::filesystem::exists(argv[2]))
        return OUTFILE_CONFLICT;

    return SANE;
}

int main(int argc, char *argv[])
{
    WrongInvocation error = sane(argc, argv);
    if (error) {
        print_help(error);
        return -error;
    }
    std::string input_file = std::filesystem::absolute(argv[1]).string(),
                output_file = std::filesystem::absolute(argv[2]).string();

    InitializeMagick(*argv);
    try {
        Image image;
        // Read a file into image object
        std::cout << "Reading from " << input_file << std::endl;
        image.read(input_file);
        auto geometry = image.size();

        auto w = geometry.width();
        if (w % 8 != 0)
            w = ((w + 8 - 1) / 8) * 8;

        auto h = geometry.height();
        if (h % 8 != 0)
            h = ((h + 8 - 1) / 8) * 8;

        std::cout << std::to_string(geometry.width()) + "x" + std ::to_string(geometry.height())
                  << std::endl;
        std::cout << std::to_string(w) + "x" + std ::to_string(h) + "," << std::endl;

        geometry.xOff(0);
        geometry.yOff(0);
        geometry.width(w);
        geometry.height(h);

        image.resize(geometry);
        image.colorSpace(ColorspaceType::sRGBColorspace);

        image.depth(8);

        // Write the image to a file
        std::cout << "Writing to " << output_file << std::endl;
        image.write(output_file);
    } catch (Exception &error_) {
        cout << "Caught exception: " << error_.what() << endl;
        return 1;
    }
    return 0;
}
