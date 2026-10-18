// The prototype's WaveShaper curves, computed directly. A WaveShaper clamps its input to [-1, 1] first, so even a
// neutral curve clips; these do too.
#pragma once

#include <cmath>

namespace aw {

/// driveCurve(a): (1+k)x / (1+k|x|) with k = round(a·20)·2.4.
struct Drive {
    float k = 0;
    bool on = false;
    /// The prototype only inserts the shaper when the amount is at least 0.02.
    void set(float amount) {
        on = amount >= 0.02f;
        k = std::round(amount * 20) * 2.4f;
    }
    float operator()(float x) const {
        x = x < -1 ? -1 : x > 1 ? 1 : x;
        return (1 + k) * x / (1 + k * std::fabs(x));
    }
};

/// crushCurve(a): rounds to 16 − round(a·14) bits.
struct Crush {
    float steps = 32768;
    bool on = false;
    void set(float amount) {
        on = amount >= 0.02f;
        int bits = (int)std::round(16 - amount * 14);
        steps = std::pow(2.f, (float)(bits - 1));
    }
    float operator()(float x) const {
        x = x < -1 ? -1 : x > 1 ? 1 : x;
        return std::round(x * steps) / steps;
    }
};

} // namespace aw
