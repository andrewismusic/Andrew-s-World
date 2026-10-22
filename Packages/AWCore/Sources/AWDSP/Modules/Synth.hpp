// The synth engine behind bass, chords and lead: the prototype's synthHit(). Two oscillators (B can frequency-
// modulate A), sub, noise, eight additive partials on the lead, a filter with its own envelope, an amp envelope,
// three tempo-synced LFOs, drive and crush. Bass and chords are mono; the lead is mono unless VOICES is POLY.
#pragma once

#include "Module.hpp"
#include "../DSP/Automation.hpp"
#include "../DSP/Biquad.hpp"
#include "../DSP/Noise.hpp"
#include "../DSP/Shapers.hpp"

namespace aw {

struct SynthVoice {
    static constexpr int kDegrees = 5;
    bool active = false;
    uint32_t generation = 0;
    const void *owner = nullptr;
    double start = 0, end = 0, stopAt = 0, attack = 0;
    float releaseTime = 0;
    const RenderEnv *env = nullptr;

    int degrees = 0;
    Automation freq[kDegrees];
    double phA[kDegrees] = {}, phB[kDegrees] = {}, phSub[kDegrees] = {};
    float fmDepth[kDegrees] = {};
    Wave waveA = Wave::Saw, waveB = Wave::Square;
    float ratioB = 1, detuneB = 1;
    bool oscs = true;

    // additive partials (lead)
    struct Partial {
        int degree, harmonic;
        float ratio, detune, amp, target;
        bool decays;
        double phase;
    };
    Partial partials[kDegrees * 8];
    int partialCount = 0;
    Automation partialDecay;

    FilterType filterType = FilterType::Lowpass;
    float filterQ = 1;
    Biquad filter;
    Automation cutoff;
    Drive drive;
    Crush crush;
    Automation amp;
    Route route;

    // LFOs: pitch (cents), cutoff (cents), amp; each keeps its own phase so a rate change doesn't jump
    float lfoPitchHz = 0, lfoPitchCents = 0, lfoCutHz = 0, lfoCutCents = 0, lfoAmpHz = 0, lfoAmp = 0;
    double lfoPhase[3] = {};

    // following edits while the note sounds: where its parameters came from, what it started with, and gains that
    // glide to their new values (level and cutoff as multipliers on their envelopes)
    int32_t noteId = 0;
    int16_t song = -1, part = -1;
    const void *from = nullptr; // the song snapshot the parameters were last read from
    float baseLevel = 0, baseCutoff = 1, levelScale = 1;
    double rootFreq[kDegrees] = {};
    enum Smoothed { GainA, GainB, GainSub, GainNoise, Additive, LevelMul, CutMul, kSmoothed };
    float cur[kSmoothed] = {}, tgt[kSmoothed] = {};

    RNG rng;

    struct Setup {
        EngineKind engine;
        const Music *music;
        int degree;
        double time, duration;
        const float *v;
        float *lastFreq; // per slot, one per chord degree
    };
    void trigger(const RenderEnv &e, const Setup &s);
    /// Takes up new parameter values while the note sounds: levels, mix, tone, filter, LFOs, effects and sends.
    /// Envelope times, pitch and chord shape stay as the note started.
    void retarget(const float *v, const Music &m, EngineKind engine);
    /// Cuts the note short at t2: a fast 18 ms fade for voice stealing and mono lines, else its own release.
    void release(double t2, bool fast);
    void render(double t0, int frames, const TrackBus &bus);
};

/// Every synth voice in the location, shared so the 20-voice cap counts across tracks like the prototype's.
class SynthPool {
public:
    static constexpr int kVoices = 32;
    static constexpr int kCap = 20;
    struct Handle {
        int index = -1;
        uint32_t generation = 0;
    };
    SynthVoice voices[kVoices];

    Handle allocate(const void *owner);
    SynthVoice *get(Handle h) {
        if (h.index < 0) return nullptr;
        SynthVoice &v = voices[h.index];
        return v.active && v.generation == h.generation ? &v : nullptr;
    }
    /// Keeps at most kCap voices sounding, releasing the oldest quickly (MAXV).
    void admit(Handle h, double now, double t);
    void panic();

private:
    Handle live_[kVoices * 2];
    int liveCount_ = 0;
};

class SynthModule : public Module {
public:
    SynthModule(const RenderEnv &env, SynthPool &pool, EngineKind engine) : env_(env), pool_(pool), engine_(engine) {}
    void start(const NoteStart &note) override;
    uint32_t startHeld(const NoteStart &note) override;
    void release(uint32_t token, double t) override;
    void render(double t0, int frames, const TrackBus &bus) override;
    void stopped() override;
    void panic() override;
    void beginBlock(double now) override { now_ = now; }

private:
    const RenderEnv &env_;
    SynthPool &pool_;
    EngineKind engine_;
    SynthPool::Handle mono_;
    float lastFreq_[SynthVoice::kDegrees] = {};
    double now_ = 0;
    uint32_t seed_ = 7;
};

} // namespace aw
