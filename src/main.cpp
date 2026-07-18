#include "BSP.h"
#include "Formatting.h"
#include "StaticProps.h"
#include <chrono>
#include <cstdio>
#include <filesystem>

namespace fs  = std::filesystem;
namespace chr = std::chrono;

struct args_t {
    fs::path input;
    std::optional<fs::path> outputDir;
};

static void printUsage() {
    std::fputs(
        "Usage for FadeStripper: \n"
        "\n"
        "  <map.bsp>        your BSP filename\n"
        "  -output <dir>    use it to specify your output path\n"
        "                   by default the outpout is same directory as input\n"
        "\n"
        "Output file: <map>_no_fade.bsp\n",
        stdout);
}

static std::optional<args_t> parseargs(int argc, char* argv[])
{
    if (argc < 2)
        return std::nullopt;

    args_t args_t;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == "-output" || arg == "--output") //handle both forms
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: output requires a path argument\n");
                return std::nullopt;
            }
            args_t.outputDir = fs::path{argv[++i]};
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            std::fprintf(stderr, "Error: unknown argument '%s'\n", argv[i]);
            return std::nullopt;
        }
        else
        {
            if (!args_t.input.empty()) {
                std::fprintf(stderr, "Error: multiple input files specified\n");
                return std::nullopt;
            }
            args_t.input = fs::path{argv[i]};
        }
    }

    if (args_t.input.empty()) {
        std::fprintf(stderr, "Error: no input file specified\n");
        return std::nullopt;
    }

    return args_t;
}

static fs::path buildOutputPath(const fs::path& input, const std::optional<fs::path>& outputDir)  {
    const fs::path filename = 
                  input.stem().string() + "_no_fade" + input.extension().string();
    return outputDir.has_value() ? (*outputDir / filename) : (input.parent_path() / filename);
}

int main(int argc, char* argv[])  {
    
    std::printf("FadeStripper (%s)\n", __DATE__);

    const auto args_t = parseargs(argc, argv);
    if (!args_t) {
        printUsage();
        return 1;
    }

    const fs::path outputPath = buildOutputPath(args_t->input, args_t->outputDir);

    if (args_t->outputDir.has_value()) {
        std::error_code ec;
        fs::create_directories(*args_t->outputDir, ec);
        if (ec){
            std::fprintf(stderr, "Error: cannot create output directory '%s': %s\n",
                         args_t->outputDir->string().c_str(), ec.message().c_str());
            return 1;
        }
    }

    BSP bsp{args_t->input.string()};
    if (!bsp){
        std::fprintf(stderr, "Error: could not load '%s' (invalid or unsupported BSP)\n",
                     args_t->input.string().c_str());
        return 1;
    }
    std::fprintf(stdout, "%s loaded (BSP v%d)\n",
                 args_t->input.filename().string().c_str(), bsp.version());

    const LogFn log = [](const char*) {};
    const PatchResult res = patchStaticPropFades(bsp, log);

    if (!res.ok)  {
        std::fprintf(stderr, "Error: %s\n", res.error.c_str());
        return 1;
    }

    std::fprintf(stdout, "Found %d static props, %d are with fade\n", res.total, res.hasFade);

    if (res.patched == 0) {
        std::fputs("All static props are already without fade\nCancelling...\n", stdout);
        return 0;
    }

    const auto t0 = chr::high_resolution_clock::now();

    std::fputs("Deleting fade on static props...\n", stdout);

    const std::string  oldStem = args_t->input.stem().string();
    const RenameResult ren     = bsp.renameMapReferences(oldStem);
    if (!ren.ok) {
        std::fprintf(stderr, "Error: failed to rename map references!!!\n");
        return 1;
    }

    if (!bsp.bake(outputPath.string())){
        std::fprintf(stderr, "Error: could not write %s\n", outputPath.string().c_str());
        return 1;
    }

    const double elapsed = chr::duration<double>(chr::high_resolution_clock::now() - t0).count();
    std::fprintf(stdout, "Completed in %.3fs -> %s\n", elapsed, outputPath.string().c_str());  
    return 0;
}