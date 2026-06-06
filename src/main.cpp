#include "BSP.h"
#include "Formatting.h"
#include "StaticProps.h"
#include <chrono>
#include <cstdio>
#include <filesystem>

namespace fs  = std::filesystem;
namespace chr = std::chrono;

static fs::path buildOutputPath (const fs::path& in) {
    return in.parent_path () /
    fs::path{ in.stem ().string () + "_no_fade" + in.extension ().string () };
}

int main (int argc, char* argv[]) {
    if (argc < 2) {
        std::fputs ("Pass me the map :) \n", stdout);
        return 1;
    }

    const fs::path inputPath  = argv[1];
    const fs::path outputPath = buildOutputPath (inputPath);

    BSP bsp{ inputPath.string () };
    if (!bsp) {
        std::fprintf (stderr, "Error: could not load %s (this BSP file doesnt exist)\n",
        inputPath.string ().c_str ());
        return 1;
    }
    std::fprintf (stdout, "%s loaded (BSP v%d)\n",
    inputPath.filename ().string ().c_str (), bsp.version ());

    const LogFn log       = [] (const char*) {};
    const PatchResult res = patchStaticPropFades (bsp, log);

    if (!res.ok) {
        std::fprintf (stderr, "Error: %s\n", res.error.c_str ());
        return 1;
    }

    std::fprintf (
    stdout, "Found %d static props, %d with fade\n", res.total, res.hasFade);
    std::fputs ("Deleting fade on static props...\n", stdout);

    const auto t0 = chr::high_resolution_clock::now ();

    if (!bsp.bake (outputPath.string ())) {
        std::fprintf (
        stderr, "Error: could not write %s\n", outputPath.string ().c_str ());
        return 1;
    }

    const double elapsed =
    chr::duration<double> (chr::high_resolution_clock::now () - t0).count ();

    std::fprintf (stdout, "Done in %.3fs → %s\n", elapsed,
    outputPath.filename ().string ().c_str ());
    return 0;
}