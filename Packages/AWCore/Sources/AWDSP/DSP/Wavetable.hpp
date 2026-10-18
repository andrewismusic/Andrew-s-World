// Band-limited oscillator waveforms, like Web Audio's OscillatorNode: each waveform is stored as a set of tables,
// one per octave of fundamental frequency, each holding only the harmonics that fit below Nyquist.
// Built once per sample rate (not on the audio thread).
#pragma once

#include <vector>

namespace aw {

enum class Wave { Sine = 0, Triangle = 1, Saw = 2, Square = 3, Pulse = 4 };

class Wavetables {
public:
    static constexpr int kSize = 2048;
    static constexpr int kRanges = 11; // fundamentals up to 20 Hz · 2^i

    void build(double sampleRate);

    /// One sample at `phase` in [0, 1) for a fundamental of `freq` Hz.
    float sample(Wave w, double phase, float freq) const;

private:
    // tables_[wave][range] has kSize + 1 samples (the last repeats the first)
    std::vector<float> tables_[5][kRanges];
    double sampleRate_ = 0;
    float rangeTop_[kRanges] = {};
};


} // namespace aw
