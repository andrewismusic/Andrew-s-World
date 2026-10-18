// The module contract. A module is one instrument on one track slot: it receives notes from the sequencer
// (with their parameters already resolved) and adds its sound into the track's bus. Locations choose which module
// sits on each slot; new instruments, including tools brought over from other apps, arrive as new modules.
#pragma once

#include <cmath>
#include <cstdint>

#include "../Core/Snapshot.hpp"
#include "../DSP/Wavetable.hpp"

namespace aw {

/// Shared by everything that renders at one sample rate.
struct RenderEnv {
    double sampleRate = 48000;
    double dt = 1.0 / 48000;
    const Wavetables *tables = nullptr;
};

/// Where a track's voices write: the track bus, and the sends to the shared delay and reverb. Voices add into these.
struct TrackBus {
    float *left, *right;
    float *delayLeft, *delayRight;
    float *reverbLeft, *reverbRight;
};

/// One note to start, at an exact time on the engine clock (seconds), possibly later than the current block.
struct NoteStart {
    double time;
    double duration; // seconds
    int pitch;       // scale step, or drum row
    const float *params;
    const Music *music;
    /// Drums only: a collected sound that replaces this row's voice.
    const Sample *sample = nullptr;
};

class Module {
public:
    virtual ~Module() = default;
    /// Called at the start of every block with the block's time, before any start().
    virtual void beginBlock(double now) { (void)now; }
    virtual void start(const NoteStart &note) = 0;
    /// A note held until release() (live playing). Returns a token for release, or 0 if the note can't be held.
    virtual uint32_t startHeld(const NoteStart &note) {
        start(note);
        return 0;
    }
    virtual void release(uint32_t token, double t) { (void)token; (void)t; }
    /// Adds the block [t0, t0 + frames·dt) into the bus.
    virtual void render(double t0, int frames, const TrackBus &bus) = 0;
    /// Transport stopped: forget per-song memory (glide), let sounding notes ring out.
    virtual void stopped() {}
    /// Silence everything now (a new location, a reset).
    virtual void panic() = 0;
};

/// The prototype's route(): an equal-power pan (unity in the centre), then post-pan sends of dl²·1.2 and rv²·1.4.
struct Route {
    float gl = 1, gr = 1, dl = 0, rv = 0;
    void set(float pan, float delay, float reverb) {
        if (std::fabs(pan) <= .01f) gl = gr = 1;
        else {
            double x = (pan + 1) * .5 * 1.5707963267948966;
            gl = (float)(std::cos(x) * 1.4142135623730951);
            gr = (float)(std::sin(x) * 1.4142135623730951);
        }
        dl = delay > .01f ? delay * delay * 1.2f : 0;
        rv = reverb > .01f ? reverb * reverb * 1.4f : 0;
    }
    void add(const TrackBus &b, int i, float x) const {
        float l = x * gl, r = x * gr;
        b.left[i] += l;
        b.right[i] += r;
        if (dl) { b.delayLeft[i] += l * dl; b.delayRight[i] += r * dl; }
        if (rv) { b.reverbLeft[i] += l * rv; b.reverbRight[i] += r * rv; }
    }
};

} // namespace aw
