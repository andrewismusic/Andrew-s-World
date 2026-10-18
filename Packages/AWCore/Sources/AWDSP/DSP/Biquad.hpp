// A biquad with the Web Audio BiquadFilterNode's coefficient formulas (from the Web Audio spec, which follows the
// RBJ Audio EQ Cookbook). Note that for low- and high-pass Q is in decibels; for band-pass it is the usual Q.
#pragma once

#include <cmath>

#include "Common.hpp"

namespace aw {

enum class FilterType { Lowpass, Bandpass, Highpass, Highshelf };

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    /// frequency in Hz; detune already applied by the caller. gainDb is for the shelf only.
    void set(FilterType type, double frequency, double Q, double sampleRate, double gainDb = 0) {
        double nyquist = sampleRate * 0.5;
        double f = frequency / nyquist;
        if (f < 1e-5) f = 1e-5;
        if (f > 0.9999) f = 0.9999;
        double w0 = kPi * f, cw = std::cos(w0), sw = std::sin(w0);
        double B0, B1, B2, A0, A1, A2;
        switch (type) {
        case FilterType::Lowpass: {
            double alpha = sw / (2 * std::pow(10.0, Q / 20));
            B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2;
            A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha;
            break;
        }
        case FilterType::Highpass: {
            double alpha = sw / (2 * std::pow(10.0, Q / 20));
            B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2;
            A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha;
            break;
        }
        case FilterType::Bandpass: {
            double q = Q > 1e-4 ? Q : 1e-4;
            double alpha = sw / (2 * q);
            B0 = alpha; B1 = 0; B2 = -alpha;
            A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha;
            break;
        }
        case FilterType::Highshelf: {
            double A = std::pow(10.0, gainDb / 40), sA = std::sqrt(A);
            double alpha = sw / 2 * std::sqrt(2.0);
            B0 = A * ((A + 1) + (A - 1) * cw + 2 * sA * alpha);
            B1 = -2 * A * ((A - 1) + (A + 1) * cw);
            B2 = A * ((A + 1) + (A - 1) * cw - 2 * sA * alpha);
            A0 = (A + 1) - (A - 1) * cw + 2 * sA * alpha;
            A1 = 2 * ((A - 1) - (A + 1) * cw);
            A2 = (A + 1) - (A - 1) * cw - 2 * sA * alpha;
            break;
        }
        }
        b0 = (float)(B0 / A0); b1 = (float)(B1 / A0); b2 = (float)(B2 / A0);
        a1 = (float)(A1 / A0); a2 = (float)(A2 / A0);
    }

    float process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }

    void clear() { x1 = x2 = y1 = y2 = 0; }

    /// Takes another filter's coefficients, keeping this one's state.
    void copyCoefficients(const Biquad &o) {
        b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2;
    }

    /// Flushes tiny values so a silent filter doesn't fall into denormals.
    void settle() {
        if (std::fabs(y1) < 1e-15f) y1 = 0;
        if (std::fabs(y2) < 1e-15f) y2 = 0;
    }
};

} // namespace aw
