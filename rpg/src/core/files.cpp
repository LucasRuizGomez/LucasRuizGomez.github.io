#include "core/files.h"

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace hc {

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

json loadJson(const std::string& path) {
    try {
        return json::parse(readFile(path), nullptr, true, /*ignore_comments=*/true);
    } catch (const json::exception& e) { throw std::runtime_error("Bad JSON in " + path + ": " + e.what()); }
}

static void vlog(const char* tag, const char* fmt, va_list args) {
    std::fprintf(stderr, "[%s] ", tag);
    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);
}

void logInfo(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog("info", fmt, args);
    va_end(args);
}

void logWarn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vlog("warn", fmt, args);
    va_end(args);
}

}   // namespace hc
