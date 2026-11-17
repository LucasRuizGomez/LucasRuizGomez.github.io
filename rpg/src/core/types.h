// Small shared value types and math helpers. No dependencies beyond the standard library.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace hc {

constexpr double PI = 3.14159265358979323846;

struct Color {
    uint8_t r = 255, g = 255, b = 255, a = 255;
    constexpr Color() = default;
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    constexpr Color alpha(double k) const { return {r, g, b, static_cast<uint8_t>(std::clamp(k, 0.0, 1.0) * 255.0 + 0.5)}; }
};

// "#rrggbb" or "#rrggbbaa". Anything else gives opaque white.
inline Color hex(const std::string& s) {
    auto nib = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0; };
    if (s.size() < 7 || s[0] != '#') return {};
    auto byte = [&](size_t i) { return static_cast<uint8_t>(nib(s[i]) * 16 + nib(s[i + 1])); };
    return {byte(1), byte(3), byte(5), s.size() >= 9 ? byte(7) : static_cast<uint8_t>(255)};
}

struct Vec2 {
    double x = 0, y = 0;
};

struct TilePos {
    int c = 0, r = 0;
    bool operator==(const TilePos&) const = default;
};

inline int ifloor(double v) { return static_cast<int>(std::floor(v)); }
inline int iround(double v) { return static_cast<int>(std::floor(v + 0.5)); }
inline double hypot2(double x, double y) { return std::sqrt(x * x + y * y); }
template <typename T> inline int sign(T v) { return (T(0) < v) - (v < T(0)); }

}   // namespace hc
