cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands for tooling.")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

project(FadeStripper
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(FS_ENABLE_STATIC_MSVC_RUNTIME "Build with static MSVC runtime (/MT)." OFF)
option(FS_ENABLE_PDB_IN_RELEASE      "Emit PDB in Release builds (MSVC)."    ON)
option(FS_ENABLE_ASAN                "Build with AddressSanitizer."           OFF)

find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE FS_GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE FS_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    message(STATUS "[FadeStripper] Git: branch=${FS_GIT_BRANCH} commit=${FS_GIT_COMMIT}")
endif()

if(UNIX)
    find_package(fmt REQUIRED)
endif()

add_library(lzma_sdk STATIC
    src/lzma/LzmaDec.c
    src/lzma/LzmaEnc.c
    src/lzma/LzFind.c
)
target_include_directories(lzma_sdk PUBLIC src/lzma)
target_compile_definitions(lzma_sdk PUBLIC _NO_EXCEPTIONS _LZMA_PROB32 _7ZIP_ST)

target_compile_options(lzma_sdk PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
)

add_executable(FadeStripper
    src/LZMA.cpp
    src/BSP.cpp
    src/StaticProps.cpp
    src/main.cpp
)

target_include_directories(FadeStripper PRIVATE src src/lzma)
target_link_libraries(FadeStripper PRIVATE lzma_sdk)

if(MSVC AND FS_ENABLE_STATIC_MSVC_RUNTIME)
    set_target_properties(FadeStripper PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

if(UNIX)
    target_link_libraries(FadeStripper PRIVATE fmt::fmt)
endif()
if(MSVC)
    target_link_libraries(FadeStripper PRIVATE dbghelp)
endif()

target_compile_options(FadeStripper PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /Zc:rvalueCast /EHa /MP /Oi /utf-8 /wd4127 /wd4201 /wd4324>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /GL>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>,$<BOOL:${FS_ENABLE_PDB_IN_RELEASE}>>:/Zi>
    
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter -Wno-array-bounds>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3 -flto>
    $<$<CXX_COMPILER_ID:Clang>:-Wno-gnu-null-pointer-arithmetic>
    
    $<$<BOOL:${FS_ENABLE_ASAN}>:$<IF:$<CXX_COMPILER_ID:MSVC>,/fsanitize=address,-fsanitize=address>>
)

target_link_options(FadeStripper PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/LTCG /OPT:REF /OPT:ICF>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>,$<BOOL:${FS_ENABLE_PDB_IN_RELEASE}>>:/DEBUG>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-flto>
    $<$<BOOL:${FS_ENABLE_ASAN}>:$<IF:$<CXX_COMPILER_ID:MSVC>,,-fsanitize=address>>
)

target_compile_definitions(FadeStripper PRIVATE
    $<$<PLATFORM_ID:Linux>:LINUX=1 _LINUX=1 POSIX=1>
    $<$<PLATFORM_ID:Darwin>:OSX=1 _OSX=1 POSIX=1>
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN>
    
    $<$<CONFIG:Debug>:_DEBUG DEBUG>
    
    $<$<CXX_COMPILER_ID:MSVC>:COMPILER_MSVC=1>
    $<$<CXX_COMPILER_ID:GNU>:COMPILER_GCC=1>
    $<$<CXX_COMPILER_ID:Clang>:COMPILER_CLANG=1>
    
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:PLATFORM_64BITS=1 X64BITS=1>
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:PLATFORM_32BITS=1 X86BITS=1>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>>:_WIN64 COMPILER_MSVC64=1>
)

if(MSVC)
    set_property(TARGET FadeStripper APPEND_STRING PROPERTY 
        LINK_FLAGS " /SUBSYSTEM:CONSOLE")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set_property(TARGET FadeStripper APPEND_STRING PROPERTY LINK_FLAGS " /MACHINE:X64")
    else()
        set_property(TARGET FadeStripper APPEND_STRING PROPERTY LINK_FLAGS " /MACHINE:X86")
    endif()
endif()

install(TARGETS FadeStripper RUNTIME DESTINATION .)
