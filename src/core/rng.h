// PCG32 (O'Neill 2014). Small, fast, statistically strong, and -- critically for
// this game -- trivially serialisable so a seed can be displayed, typed in, and
// replayed.
//
// Lunar Trail uses an explicit, player-visible seed rather than hidden entropy.
// The original 1971/78 Oregon Trail was fully deterministic (verified: the
// source contains no RANDOMIZE statement), and the 1985 remake's determinism
// is undocumented. The design goal here is the same property that made the
// original feel *fair*: a death must be attributable to a decision, and the
// route must be learnable. See docs/DESIGN.md section 7.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace lt {

class Rng {
public:
    Rng() { seed(0x853c49e6748fea9bULL); }

    void seed(uint64_t s) {
        state_ = 0;
        inc_ = 0x14057b7ef767814fULL;  // stream selector, arbitrary but fixed
        nextU32();
        state_ += s;
        nextU32();
    }

    uint32_t nextU32() {
        const uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        const uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
    }

    // Uniform in [0, n). Rejection-sampled so the distribution is exactly
    // uniform -- a modulo fold would bias low indices, which matters because
    // this drives hazard selection.
    uint32_t below(uint32_t n) {
        if (n == 0) return 0;
        const uint32_t threshold = (0u - n) % n;
        for (;;) {
            const uint32_t r = nextU32();
            if (r >= threshold) return r % n;
        }
    }

    // Uniform in [0,1).
    double unit() {
        return static_cast<double>(nextU32()) / 4294967296.0;
    }

    // Uniform in [lo,hi).
    double range(double lo, double hi) { return lo + unit() * (hi - lo); }

    int rangeInt(int lo, int hi) {  // inclusive lo, exclusive hi
        if (hi <= lo) return lo;
        return lo + static_cast<int>(below(static_cast<uint32_t>(hi - lo)));
    }

    bool chance(double p) { return unit() < p; }

    // Fisher-Yates, in place.
    template <typename T>
    void shuffle(std::vector<T>& v) {
        for (size_t i = v.size(); i > 1; --i) {
            const size_t j = below(static_cast<uint32_t>(i));
            std::swap(v[i - 1], v[j]);
        }
    }

    uint64_t state() const { return state_; }
    void restore(uint64_t s) {
        seed(s);
    }

private:
    uint64_t state_ = 0;
    uint64_t inc_ = 0x14057b7ef767814fULL;
};

// Formats a seed as the 8 hex digits a player would type in.
std::string formatSeed(uint64_t s);

// Accepts hex with an optional 0x prefix. Returns false if unparseable.
bool parseSeed(const std::string& text, uint64_t* out);

// Derives a child seed from a parent and a purpose tag, so subsystems can have
// independent streams without disturbing one another.
uint64_t deriveSeed(uint64_t parent, uint64_t salt);

}  // namespace lt
