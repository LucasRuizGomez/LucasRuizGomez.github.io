// Deterministic random numbers and coherent noise. World generation depends on these giving
// the same sequence on every platform, so they use fixed-width integer arithmetic only.
#pragma once
#include <cstdint>

namespace hc {

// mulberry32: small, fast, good enough for games, and trivially seedable.
class Rng {
  public:
    explicit Rng(uint32_t seed = 1) : state_(seed) {}
    void seed(uint32_t s) { state_ = s; }
    uint32_t state() const { return state_; }
    double next();                                        // [0, 1)
    int range(int n) { return static_cast<int>(next() * n); }   // 0 .. n-1
    int between(int lo, int hi) { return lo + range(hi - lo + 1); }
    double uniform(double lo, double hi) { return lo + next() * (hi - lo); }
    bool chance(double p) { return next() < p; }

  private:
    uint32_t state_;
};

double hash2(int x, int y, int s);                 // position hash in [0, 1)
double vnoise(double x, double y, int s);          // value noise in [0, 1)
double fbm(double x, double y, int s);             // three octaves of value noise

}   // namespace hc
