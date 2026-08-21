# FadeStripper

This tool will automatically delete all the fade distance data on every `prop_static` and `info_overlay` entities in a map (current support for v20/v21 BSP formats), preventing to manually edit their configs in Hammer or any lump editor program (this tool was mainly designed for use on maps used in SFM since artist DONT like the fade)

The LZMA compressed lumps are automatically detected, decompressed, patched, and recompressed by the program 

## Dependencies

This tool only needs the (already included) C library from **LZMA SDK** downloaded from [this page](https://www.7-zip.org/sdk.html) for its functionality

## Build

To build mannually this tool just download the source code zip and uncompress the folder  

Then open a terminal on `Fade-Stripper-main` directory and paste the following for your platform/compiler

**I will assume you arleady have `Cmake` installed on your machine**

```bat
::windows 
cmake -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release
```

```sh
# GCC / Clang
cmake -B build -DCMAKE_BUILD_TYPE=Release &&
cmake --build build
```
The executable file will be located on `build/bin` and then you could place it anywhere on your disk

## Usage

Go to the [latest release](https://github.com/HSPMDev15/Fade-Stripper/releases/), download your platform zip artifact and uncompress it.

Then just drag n drop the map onto the executable OR you can pass the arguments through a terminal window 

CLI use example:

```
FadeStripper.exe path/to/my_awesome_map.bsp 
```

Output: `<mapname>_no_fade.bsp` on same directory as input

### Custom Output Directory
You can also optionally specify where to save the patched map using the `-output` argument

For example:

```FadeStripper.exe my_map.bsp -output ../patched_maps```

**Note:** this supports relative paths (like `../`) and absolute paths
