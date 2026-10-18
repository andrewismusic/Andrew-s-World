// The Farm drum machine: the prototype's eight drumHit() voices. Each row chokes its own previous hit.
#pragma once

#include "Module.hpp"
#include "../DSP/Automation.hpp"
#include "../DSP/Biquad.hpp"
#include "../DSP/Noise.hpp"
#include "../DSP/Shapers.hpp"

namespace aw {

struct DrumVoice {
    bool active = false;
    int row = 0;
    double start = 0, end = 0;
    const RenderEnv *env = nullptr;

    // shared tail: lowpass (TONE) → drive → crush → level → route
    Biquad tone;
    Drive drive;
    Crush crush;
    Automation level;
    Route route;

    // tonal source (kick, snare, rim knock, tom, perc carrier)
    Wave wave = Wave::Sine;
    double phase = 0, modPhase = 0;
    Automation freq, gain;
    float modRatio = 0, modDepth = 0; // perc FM
    // noise path one
    double noiseStop = 0;
    Biquad n1a, n1b;
    bool n1b_on = false;
    Automation noiseGain;
    float noisePre = 1; // hats: 1 − punch·0.6
    // noise path two (rim ring: resonant band-pass that sweeps down)
    double noise2Stop = 0;
    Biquad n2;
    Automation n2freq, n2gain;
    float n2Q = 14;
    double n2cf = -1;
    // hats: four detuned squares
    double sqPhase[4] = {};
    float sqFreq[4] = {}, sqGain = 0;
    bool squares = false;
    bool hasOsc = false, hasNoise = false, hasNoise2 = false;
    RNG rng;

    void trigger(const RenderEnv &e, int row, double t, const float *o);
    void choke(double t);
    void render(double t0, int frames, const TrackBus &bus);
};

class DrumKit : public Module {
public:
    static constexpr int kVoices = 48;
    explicit DrumKit(const RenderEnv &env) : env_(env) {}
    void start(const NoteStart &note) override;
    void render(double t0, int frames, const TrackBus &bus) override;
    void panic() override;

private:
    const RenderEnv &env_;
    DrumVoice voices_[kVoices];
    int last_[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    uint32_t seed_ = 1;
};

} // namespace aw
