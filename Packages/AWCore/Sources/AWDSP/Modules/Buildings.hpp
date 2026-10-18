// Track effects, the Farm's "buildings": up to three in series on each track, chosen per song.
//   Mill: distortion (2× oversampled shaper, then a tone low-pass), dry/wet.
//   Well: echo with a darkened feedback loop, added on top of the dry signal.
//   Windmill: chorus, a short delay swept by a slow LFO.
//   Barn: a filter (low-, band- or high-pass), fully wet.
#pragma once

#include <string>
#include <vector>

#include "../DSP/Automation.hpp"
#include "../DSP/Biquad.hpp"

namespace aw {

enum class BuildingType : uint8_t { None, Mill, Well, Windmill, Barn };

BuildingType buildingNamed(const std::string &name);
const char *buildingKey(BuildingType t);
/// Default value of a building's parameter q (0–2) when nothing locks it.
float buildingDefault(BuildingType t, int q);
extern const char *const kBuildingParams[5][3];

/// A 2× oversampler for the Mill's shaper (half-band FIR up and down), 15 samples of latency.
class Oversampler2x {
public:
    Oversampler2x();
    template <typename F> float process(float x, F &&shape) {
        // upsample: zero-stuff and filter, giving two samples per input
        float a = shape(interpolate(x * 2, 0)), b = shape(interpolate(0, 1));
        return decimate(a, b);
    }
    void clear();

private:
    float interpolate(float x, int phase);
    float decimate(float a, float b);
    static constexpr int kTaps = 31;
    float h_[kTaps];
    float up_[kTaps] = {}, down_[kTaps] = {};
    int upPos_ = 0, downPos_ = 0;
};

class Building {
public:
    void prepare(double sampleRate);
    void reset(BuildingType type);
    BuildingType type() const { return type_; }
    /// Moves toward new settings with a 20 ms smoothing, from time t.
    void set(const float v[3], double t, double beat);
    void process(float *L, float *R, int n, double t0);

private:
    BuildingType type_ = BuildingType::None;
    double sr_ = 48000, dt_ = 1.0 / 48000;
    float last_[3] = {-1, -1, -1};
    double lastBeat_ = 0;
    Automation dry_, wet_, p1_, p2_, p3_;
    Biquad fL_, fR_;
    FilterType filterType_ = FilterType::Lowpass;
    Oversampler2x osL_, osR_;
    // Chromium's 2× WaveShaper delays its output by 128 samples; mixed with the dry signal that delay is part of
    // the Mill's sound (a comb), so the wet path is delayed to match: 15 from the oversampler plus these 113.
    static constexpr int kMillDelay = 113;
    float millL_[kMillDelay] = {}, millR_[kMillDelay] = {};
    int millPos_ = 0;
    float driveK_ = 0;
    std::vector<float> dL_, dR_;
    size_t pos_ = 0;
    double lfoPhase_ = 0;
};

} // namespace aw
