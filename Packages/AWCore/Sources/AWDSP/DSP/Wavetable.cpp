#include "Wavetable.hpp"

#include <cmath>

#include "Common.hpp"

namespace aw {

namespace {

// Fourier sine coefficients from the Web Audio spec's built-in waveforms.
double coefficient(Wave w, int k) {
    switch (w) {
    case Wave::Sine: return k == 1 ? 1 : 0;
    case Wave::Triangle: return 8 * std::sin(k * kPi / 2) / ((kPi * k) * (kPi * k));
    case Wave::Saw: return (k % 2 ? 2.0 : -2.0) / (k * kPi);
    case Wave::Square: return k % 2 ? 4 / (k * kPi) : 0;
    case Wave::Pulse: return 0; // pulse has cosine terms too; see below
    }
    return 0;
}

} // namespace

void Wavetables::build(double sampleRate) {
    if (sampleRate == sampleRate_) return;
    sampleRate_ = sampleRate;
    double nyquist = sampleRate / 2;
    for (int w = 0; w < 5; ++w) {
        double peak = 0;
        for (int r = 0; r < kRanges; ++r) {
            double top = 20 * std::pow(2.0, r + 1);
            rangeTop_[r] = (float)top;
            int harmonics = (int)(nyquist / top);
            if (harmonics < 1) harmonics = 1;
            if (harmonics > kSize / 2 - 1) harmonics = kSize / 2 - 1;
            auto &t = tables_[w][r];
            t.assign(kSize + 1, 0.f);
            std::vector<double> acc(kSize, 0.0);
            for (int k = 1; k <= harmonics; ++k) {
                double b = coefficient((Wave)w, k), a = 0;
                if ((Wave)w == Wave::Pulse) {
                    // the prototype's 20% pulse PeriodicWave (real and imaginary parts)
                    const double d = 0.2;
                    a = std::sin(kTwoPi * k * d) / (kPi * k);
                    b = (1 - std::cos(kTwoPi * k * d)) / (kPi * k);
                }
                if (a == 0 && b == 0) continue;
                for (int i = 0; i < kSize; ++i) {
                    double ph = kTwoPi * k * i / kSize;
                    acc[i] += b * std::sin(ph) + a * std::cos(ph);
                }
            }
            for (int i = 0; i < kSize; ++i) t[i] = (float)acc[i];
            t[kSize] = t[0];
            if (r == 0)
                for (int i = 0; i < kSize; ++i) peak = std::fmax(peak, std::fabs(acc[i]));
        }
        // normalised like PeriodicWave: the fullest table peaks at 1, the rest share its scale
        float scale = peak > 0 ? (float)(1 / peak) : 1.f;
        for (int r = 0; r < kRanges; ++r)
            for (float &x : tables_[w][r]) x *= scale;
    }
}

float Wavetables::sample(Wave w, double phase, float freq) const {
    float f = std::fabs(freq);
    int r = 0;
    while (r < kRanges - 1 && f > rangeTop_[r]) ++r;
    const float *t = tables_[(int)w][r].data();
    double x = phase * kSize;
    int i = (int)x;
    if (i >= kSize) i -= kSize;
    if (i < 0) i += kSize;
    float fr = (float)(x - std::floor(x));
    return t[i] + (t[i + 1] - t[i]) * fr;
}

} // namespace aw
