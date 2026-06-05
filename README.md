# FadeStripper

This tool automatically sets `FadeMinDist = -1` and `FadeMaxDist = 0` on every `prop_static` in a BSP map (currently v20-v21), preventing fade and having to do it manually in Hammer (this was designed for maps used in SFM since animators don't like fade)


## Repo structure

```
CMakeLists.txt
src/
   lzma/               — the cloned LZMA C lib

  BSPTypes.h           — all format structs (Lump, BSPHeader, ValveLZMAHeader, etc)
  LZMA.h / .cpp        — Valve format LZMA compress/decompress via liblzma
  BSP.h  / .cpp        — BSP loader/writer (parseGameLumps, bake)
  StaticProps.h /.cpp  — sprp patcher (v7* detection)
  main.cpp             — the EXE entry point
```

## dependencies

Only the C library from **liblzma** cloned from [here](https://github.com/theaperturecat/csgo-branch-fixed/tree/main/src/utils/lzma/C)


## Build

in order to build this tool follow this steps for each (supported) platform

just open the terminal on the root path of the repo (FadeStripper folder) and paste the following

```bat
::windows 
cmake -B build -G "Visual Studio 17 2022" -A x64 &&
cmake --build build --config Release
```

```sh
# GCC / Clang
cmake -B build -DCMAKE_BUILD_TYPE=Release &&
cmake --build build
```
the file will be located on `build/bin` and then you could place it anywhere on your disk

## Usage

It's as easy as grabbing and dropping the desired map onto the executable
OR you can pass the arguments through any terminal (mainly for linux/clang users)

Example:

```
FadeStripper.exe path/to/my_awesome_map.bsp 
```

Output: `mapname_no_fade.bsp` on the same path

The LZMA compressed game lumps are automatically detected, decompressed, patched, and recompressed

## TODO

- Add support to remove fade on decals and detailed props