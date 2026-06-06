#pragma once

//------------------------------------------------
// Pruporse: string formatting utils
//------------------------------------------------


// in Windows std::format is available natively in C++20
// on Linux/macOS  is used libfmt (fmt::format)

// NOTE: _APPLE_  macro covers all Apple platforms (macOS, iOS,
// watchOS, tvOS) for strictly desktop macOS, include <TargetConditionals.h>
// and check TARGET_OS_OSX for this tool (desktop only) __APPLE__ is fine

#if defined(_WIN32)
#include <format>
namespace formatting = std;
#elif defined(__linux__) || defined(__APPLE__)
#include <fmt/format.h>
namespace formatting = fmt;
#else
#error "platform doesn`t support fmt"
#endif
