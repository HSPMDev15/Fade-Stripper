#pragma once

//-----------------------------------------------------------------------------
// Prupose:
// Console log output with semantic levels
// Error/Warning go to stderr with the respective color
// Info use stdout (the normal output of the program)
//-----------------------------------------------------------------------------

#include "Formatting.h"
#include <cstdio>
#include <string_view>

namespace log_detail {

inline constexpr std::string_view kRed    = "\x1b[31m";
inline constexpr std::string_view kYellow = "\x1b[33m";
inline constexpr std::string_view kReset  = "\x1b[0m";

inline void printColored(std::string_view color, std::string_view prefix,
                         std::string_view message, std::FILE* stream) {
    std::fprintf(stream, "%.*s%.*s%.*s%.*s\n",
                 static_cast<int>(color.size()),   color.data(),
                 static_cast<int>(prefix.size()),  prefix.data(),
                 static_cast<int>(message.size()), message.data(),
                 static_cast<int>(kReset.size()),  kReset.data());
}

} // namespace log_detail

template<typename... Args>
void Error(formatting::format_string<Args...> fmt, Args&&... args) {
    log_detail::printColored(log_detail::kRed, "Error: ",
                             formatting::format(fmt, std::forward<Args>(args)...), stderr);
}

template<typename... Args>
void Warning(formatting::format_string<Args...> fmt, Args&&... args) {
    log_detail::printColored(log_detail::kYellow, "Warning: ",
                             formatting::format(fmt, std::forward<Args>(args)...), stderr);
}

template<typename... Args>
void Info(formatting::format_string<Args...> fmt, Args&&... args) {
    const std::string msg = formatting::format(fmt, std::forward<Args>(args)...);
    std::fprintf(stdout, "%.*s\n", static_cast<int>(msg.size()), msg.data());
}