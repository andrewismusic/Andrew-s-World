#pragma once

namespace aw {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2 * kPi;

inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
inline int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

} // namespace aw
