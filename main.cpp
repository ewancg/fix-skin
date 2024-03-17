#include "Magick++.h"
#include <filesystem>
#include <iostream>

using namespace std;

std::time_t g_t = std::time(nullptr);
std::tm g_tm{};

enum LogLevel { INFO, WARN, FATAL };
template<typename... Ts>
static void log(LogLevel level, const Ts... msg) noexcept
{
    auto &stream = (level == INFO) ? std::cout : std::cerr;
    ([&] { stream << msg; }(), ...);
    std::endl(stream);
}

template<typename... Ts>
static inline void error(const Ts... msg) noexcept
{
    g_tm = *std::localtime(&g_t);
    std::cerr << "Error [" << std::put_time(&g_tm, "%X") << "]: ";
    log(LogLevel::FATAL, msg...);
}

template<typename... Ts>
static inline void warn(const Ts... msg) noexcept
{
    g_tm = *std::localtime(&g_t);
    std::cerr << "Warning [" << std::put_time(&g_tm, "%X") << "]: ";
    log(LogLevel::WARN, msg...);
}

template<typename... Ts>
static inline void info(const Ts... msg) noexcept
{
    log(LogLevel::INFO, msg...);
}

static constexpr const char g_help_text[] =
    R"GORP(fix-skin: primitively adjust a DDNet skin so that it will no longer error in the client
Usage:
   fix-skin file:input file:output

   -h,  --help       Show this message
   -v,  --verbose    Output progress

Return codes:
   -*   Bad invocation
    0   Success
    1   Unknown error
)GORP";

using Argument = std::tuple<const std::string, const std::string>;
constexpr const Argument g_help_arg_strs = std::make_tuple("-h", "--help");
constexpr const Argument g_verbose_arg_strs = std::make_tuple("-v", "--verbose");
constexpr const std::array<Argument, 2> s_args = {g_help_arg_strs, g_verbose_arg_strs};

bool g_verbose = false;
bool g_help_requested = false;
std::vector<std::string> g_unknown_args;

enum ErrorStatus {
    OK = 0,
    ERROR_UNKNOWN = -1,
    ERROR_ARGS_LT = 1,
    ERROR_ARGS_GT,
    ERROR_INFILE_NE,
    ERROR_OUTPATH_NE,
    ERROR_OUTFILE_CONFLICT,
};

void print_help(char *argv[], ErrorStatus status = OK)
{
    switch (status) {
    case OK:
        [[fallthrough]];
    default:
        break;
    case ERROR_ARGS_LT:
        error("Not enough arguments.");
        break;
    case ERROR_ARGS_GT:
        error("Too many arguments.");
        break;
    case ERROR_INFILE_NE:
        error("Input file ",
              std::filesystem::path(argv[1]).string(),
              " is inaccessible or does not exist.");
        break;
    case ERROR_OUTPATH_NE:
        error("Output file directory ",
              std::filesystem::path(argv[2]).parent_path().string(),
              " is unreadable or does not exist.");
        break;
    case ERROR_OUTFILE_CONFLICT:
        error("Output file already exists.");
        break;
    case ERROR_UNKNOWN:
        error("Unknown error.");
        return;
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

const std::integral auto next_multiple(const std::integral auto x, const int multiple)
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
                if (arg == &g_help_arg_strs) {
                    g_help_requested = true;
                    return OK;
                } else if (arg == &g_verbose_arg_strs) {
                    g_verbose = true;
                }
            } else {
                g_unknown_args.push_back(argv[i]);
            }
        }
    }

    if (!g_unknown_args.empty()) {
        std::string str;
        for (const auto &it : g_unknown_args) {
            str += (str.empty() ? "" : ", ") + it;
        }
        warn("Ignoring unknown argument(s) ", str);
    }

    switch (argc) {
    case 3:
        if (!std::filesystem::exists(argv[1]))
            return ERROR_INFILE_NE;
        if (!std::filesystem::exists(std::filesystem::path(argv[2]).parent_path()))
            return ERROR_OUTPATH_NE;
        if (std::filesystem::exists(argv[2]))
            return ERROR_OUTFILE_CONFLICT;
        return OK;
    case 2:
        [[fallthrough]];
    case 1:
        [[fallthrough]];
    case 0: // Impossible
        return ERROR_ARGS_LT;
    default:
        return ERROR_ARGS_GT;
    }
}

int main(int argc, char *argv[])
{
    ErrorStatus status = sane(argc, argv);
    if (status || g_help_requested) {
        if (g_help_requested)
            status = OK;
        print_help(argv, status);
    } else {
        std::string input_file = std::filesystem::absolute(argv[1]).string(),
                    output_file = std::filesystem::absolute(argv[2]).string();

        using namespace Magick;
        InitializeMagick(*argv);
        try {
            Image image;
            // Read a file into image object
            if (g_verbose)
                std::cout << "Reading from " << input_file << std::endl;
            image.read(input_file);
            auto geometry = image.size();

            auto w = next_multiple(geometry.width(), 8);
            auto h = next_multiple(geometry.height(), 4);

            if (g_verbose) {
                std::cout << std::to_string(geometry.width()) + "x"
                                 + std::to_string(geometry.height())
                          << " -> " << std::to_string(w) + "x" + std ::to_string(h) << std::endl;
            }

            geometry.xOff(0);
            geometry.yOff(0);
            geometry.width(w);
            geometry.height(h);
            geometry.aspect(true);

            image.depth(8);
            image.colorSpace(ColorspaceType::sRGBColorspace);
            image.resize(geometry);

            // Write the image to a file
            if (g_verbose)
                std::cout << "Writing to " << output_file << std::endl;
            image.write(output_file);
        } catch (Exception &e) {
            status = ERROR_UNKNOWN;
            error(e.what());
        }
    }
    return -status;
}
