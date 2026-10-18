// What the audio thread plays from: the location's songs with every lock already resolved. Immutable once built;
// the engine swaps whole snapshots and hands old ones back to be freed off the audio thread.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Params.hpp"
#include "Samples.hpp"
#include "../DSP/Convolver.hpp"
#include "../Modules/Buildings.hpp"

namespace aw {

/// musAt(): the score at a point in the tree.
struct Music {
    int bpm = 120;
    float swing = 0;
    int key = 0;
    int scale = 1; // index into SCALE_ORDER
    int ts = 4;    // beats in a bar
    int den = 4;   // the beat's note value
    int tpb = 12;  // clock ticks a beat
    int barTicks() const { return ts * tpb; }
    int scaleLength() const { return kScaleLength[scale]; }
};

enum class RampMode : uint8_t { None, Away, Toward };

struct NoteEvent {
    int32_t id;
    int16_t step, pitch, len;
    uint8_t ratchet; // index into kRatchetSteps; 0 is off
    RampMode rampMode;
    uint32_t params; // offset of this note's resolved parameter block in SongSnap::params
    int32_t ramp;    // offset of its ramp targets (NaN where a parameter doesn't ramp), or -1
    uint8_t slot;
    bool triplet;
    uint16_t part;
};

struct BarSnap {
    bool triplet = false;
    uint32_t first = 0, count = 0; // notes, sorted by step
};

struct LaneSnap {
    std::vector<BarSnap> bars;
};

struct PartSnap {
    Music music;
    LaneSnap lanes[4];
    int length = 1; // bars: the longest lane
    /// Each track's building settings in this part: [slot][building slot][parameter].
    float fx[4][3][3] = {};
    /// Each lane's own resolved sound, for notes played live: offset into SongSnap::params (drums: 8 rows in a row),
    /// or -1 where the slot has no engine.
    int32_t laneParams[4] = {-1, -1, -1, -1};
};

struct SpaceSettings {
    float size = .4f, damp = .5f, time = .5f, feedback = .35f, tone = .6f;
};

struct MasterSettings {
    float volume = .8f, glue = .35f, saturate = .1f;
};

/// The reverb's impulse response for one size and damp, shared between songs that use the same settings.
struct ReverbIR {
    float size = -1, damp = -1;
    double sampleRate = 0;
    std::vector<float> left, right;
    ConvKernel kernel;
};

struct SongSnap {
    std::vector<PartSnap> parts;
    std::vector<NoteEvent> notes;
    std::vector<float> params;
    SpaceSettings space;
    MasterSettings master;
    bool audible[4] = {true, true, true, true};
    BuildingType fx[4][3] = {};
    /// Drum rows replaced by a collected sound: the song's "samples" object, {"<slot>.<row>": "<sample id>"}.
    const Sample *drumSamples[4][8] = {};
    std::shared_ptr<const ReverbIR> reverb;
};

struct LocationSnap {
    std::string skin;
    EngineKind engines[4] = {EngineKind::Drums, EngineKind::Bass, EngineKind::Chords, EngineKind::Lead};
    std::vector<std::shared_ptr<const SongSnap>> songs;
};

namespace json { struct Value; }

/// Builds snapshots from save JSON, reusing songs whose text hasn't changed. One builder per engine; not
/// thread-safe, and never used on the audio thread.
class SnapshotBuilder {
public:
    SnapshotBuilder(double sampleRate, const SampleBank &samples) : sampleRate_(sampleRate), samples_(samples) {}
    /// Returns nullptr if the JSON can't be read.
    std::unique_ptr<LocationSnap> build(const std::string &locationJSON, const std::string &masterJSON);

private:
    std::shared_ptr<const SongSnap> buildSong(const json::Value &song, const json::Value &master,
                                              const EngineKind engines[4]);
    std::shared_ptr<const ReverbIR> reverbFor(float size, float damp);

    double sampleRate_;
    const SampleBank &samples_;
    struct Cached {
        std::string key;
        std::shared_ptr<const SongSnap> song;
    };
    std::vector<Cached> cache_;
    std::vector<std::shared_ptr<const ReverbIR>> irs_;
};

/// The prototype's generated impulse: noise through a one-pole lowpass that darkens over the first third,
/// fading as (1 − f)^2.2. Deterministic, so renders repeat.
void makeImpulse(ReverbIR &ir, float size, float damp, double sampleRate);

} // namespace aw
