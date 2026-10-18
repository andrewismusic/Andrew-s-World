#pragma once

#include <cstdint>

namespace aw {

/// xorshift32; fast, good enough for audio noise and probability rolls.
struct RNG {
    uint32_t s = 0x9E3779B9u;
    explicit RNG(uint32_t seed = 0x9E3779B9u) : s(seed ? seed : 1) {}
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    /// [0, 1)
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }
    /// [-1, 1)
    float bipolar() { return uniform() * 2 - 1; }
};

} // namespace aw
