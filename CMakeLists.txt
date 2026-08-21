cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands for tooling.")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

project(FadeStripper LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(FS_ENABLE_STATIC_MSVC_RUNTIME "Build with static MSVC runtime (/MT)." OFF)
option(FS_ENABLE_PDB_IN_RELEASE      "Emit PDB in Release builds (MSVC)."    ON )
option(FS_ENABLE_ASAN                "Build with AddressSanitizer."          OFF)

if(UNIX)
    find_package(fmt REQUIRED)
endif()

add_library(lzma_sdk STATIC
    third_party/lzma_sdk/LzmaDec.c
    third_party/lzma_sdk/LzmaEnc.c
    third_party/lzma_sdk/LzFind.c
    third_party/lzma_sdk/CpuArch.c
)
target_include_directories(lzma_sdk PUBLIC third_party/lzma_sdk)
target_compile_definitions(lzma_sdk PUBLIC _NO_EXCEPTIONS _LZMA_PROB32 Z7_ST)
target_compile_options(lzma_sdk PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W0> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>)

add_executable(FadeStripper
    src/LZMA.cpp
    src/BSP.cpp
    src/Entities/StaticProps.cpp
    src/Entities/Overlays.cpp
    src/main.cpp
)

target_include_directories(FadeStripper PRIVATE src third_party/lzma_sdk)

if(UNIX)
    target_link_libraries(FadeStripper PRIVATE fmt::fmt)
endif()
if(MSVC)
    target_link_libraries(FadeStripper PRIVATE dbghelp)
endif()
target_link_libraries(FadeStripper PRIVATE lzma_sdk)

if(MSVC)
    set(MSVC_RUNTIME_PREFIX "MultiThreaded")
    if(NOT FS_ENABLE_STATIC_MSVC_RUNTIME)
        set(MSVC_RUNTIME_PREFIX "MultiThreadedDLL")
    endif()
    set_target_properties(FadeStripper PROPERTIES
        MSVC_RUNTIME_LIBRARY "${MSVC_RUNTIME_PREFIX}$<$<CONFIG:Debug>:Debug>"
    )
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
if(MSVC)
    target_compile_options(FadeStripper PRIVATE /W4 /WX
        $<$<CONFIG:Release>:/O2 /Oi /Ot /GT>
        $<$<AND:$<CONFIG:Release>,$<BOOL:${FS_ENABLE_PDB_IN_RELEASE}>>:/Zi>
        $<$<BOOL:${FS_ENABLE_ASAN}>:/fsanitize=address>
    )
    target_link_options(FadeStripper PRIVATE  /SUBSYSTEM:CONSOLE
        $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:/MACHINE:X64>
        $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:/MACHINE:X86>
        $<$<AND:$<CONFIG:Release>,$<BOOL:${FS_ENABLE_PDB_IN_RELEASE}>>:/DEBUG>
    )
else()
    target_compile_options(FadeStripper PRIVATE 
        -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-parameter
        $<$<CONFIG:Release>:-O3> 
        $<$<BOOL:${FS_ENABLE_ASAN}>:-fsanitize=address>
    )
    target_link_options(FadeStripper PRIVATE
        $<$<BOOL:${FS_ENABLE_ASAN}>:-fsanitize=address>
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        target_compile_options(FadeStripper PRIVATE $<$<CONFIG:Release>:-march=x86-64-v2>)
    endif()
endif()

install(TARGETS FadeStripper RUNTIME DESTINATION .)
