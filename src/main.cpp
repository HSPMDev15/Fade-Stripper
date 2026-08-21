#include "BSP.h"
#include "log.h"
#include "Entities/StaticProps.h"
#include <chrono>
#include <filesystem>

PatchResult patchOverlayFades(BSP& bsp); // foward declaration

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
                Error("output requires a path argument");
                return std::nullopt;
            }
            args_t.outputDir = fs::path{argv[++i]};
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            Error("unknown argument '{}'", argv[i]);
            return std::nullopt;
        }
        else
        {
            if (!args_t.input.empty()) {
                Error("multiple input files specified"); //TODO: allow multiple BSP to be patched???
                return std::nullopt;
            }
            args_t.input = fs::path{argv[i]};
        }
    }

    if (args_t.input.empty()) {
        Error("no input file specified");
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
    
    Info("FadeStripper ({})", __DATE__);

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
            Error("cannot create output directory '{}': {}", args_t->outputDir->string(), ec.message());
            return 1;
        }
    }

    BSP bsp{args_t->input.string()};
    if (!bsp){
        Error("could not load '{}' (invalid or unsupported BSP)", args_t->input.string());
        return 1;
    }

    Info("{} loaded (BSP v{})",  args_t->input.filename().string(), bsp.version());

    const PatchResult sprpRes = patchStaticPropFades(bsp);
    if (!sprpRes.ok)  {
        return 1;
    }
    Info("Found {} static props, {} are with fade", sprpRes.total, sprpRes.hasFade);

    const PatchResult overlayRes = patchOverlayFades(bsp);
    if (!overlayRes.ok) {
        return 1;
    }
    Info("Found {} texture overlays, {} are with fade", overlayRes.total, overlayRes.hasFade);

    if (sprpRes.patched == 0 && overlayRes.patched == 0) {
        Warning("There isn't any entity with fade data on the map\nCancelling...");
        return 0;
    }
 
    const auto t0 = chr::high_resolution_clock::now();

    if (sprpRes.patched > 0)
        Info("Deleting fade on static props...");
    if (overlayRes.patched > 0)
        Info("Deleting fade on overlays...");

    const std::string  oldStem = args_t->input.stem().string();
    const RenameResult ren     = bsp.renameMapReferences(oldStem);
    if (!ren.ok) {
        Error("failed to rename map references!!!");
        return 1;
    }

    if (!bsp.bake(outputPath.string())){
        Error("could not write {}", outputPath.string());
        return 1;
    }
 
    const double elapsed = chr::duration<double>(chr::high_resolution_clock::now() - t0).count();
    Info("Completed in {:.3f}s -> {}", elapsed, outputPath.string());  
    return 0;
}