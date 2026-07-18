#pragma once

//-----------------------------------------------------------------------------
// Purpose: String formatting utilities
//-----------------------------------------------------------------------------

// In Windows, std::format is available natively in C++20.
// On Linux/macOS, libfmt (fmt::format) is used instead.
//
// NOTE: __APPLE__ macro covers all Apple platforms (macOS, iOS, watchOS, tvOS).
// For strictly desktop macOS, include <TargetConditionals.h> and check 
// TARGET_OS_OSX. For this library, __APPLE__ is perfectly fine

#if defined(_WIN32)
    #include <format>
    namespace formatting = std;
#elif defined(__linux__) || defined(__APPLE__)
    #include <fmt/format.h>
    namespace formatting = fmt;
#else
    #error "Platform does not support formatting library"
#endif