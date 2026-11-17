#include "core/rng.h"

#include <cmath>

namespace hc {

double Rng::next() {
    state_ += 0x6D2B79F5u;
    uint32_t t = state_;
    t = (t ^ (t >> 15)) * (1u | t);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
}

double hash2(int x, int y, int s) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + static_cast<uint32_t>(s) * 982451653u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<double>(h) / 4294967296.0;
}

double vnoise(double x, double y, int s) {
    const double xf0 = std::floor(x), yf0 = std::floor(y);
    const int xi = static_cast<int>(xf0), yi = static_cast<int>(yf0);
    const double xf = x - xf0, yf = y - yf0;
    const double u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
    const double a = hash2(xi, yi, s), b = hash2(xi + 1, yi, s), c = hash2(xi, yi + 1, s), d = hash2(xi + 1, yi + 1, s);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

double fbm(double x, double y, int s) { return vnoise(x, y, s) * 0.6 + vnoise(x * 2, y * 2, s + 7) * 0.3 + vnoise(x * 4, y * 4, s + 13) * 0.1; }

}   // namespace hc
