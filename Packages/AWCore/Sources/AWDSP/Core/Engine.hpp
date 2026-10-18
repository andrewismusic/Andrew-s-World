// The render entry point: commands and snapshots in, sequencer, modules, shared space and master out.
#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "../DSP/Automation.hpp"
#include "../DSP/Biquad.hpp"
#include "../DSP/Noise.hpp"
#include "../DSP/Shapers.hpp"
#include "../Modules/DrumKit.hpp"
#include "../Modules/Synth.hpp"
#include "SPSC.hpp"
#include "Snapshot.hpp"

namespace aw {

struct Context {
    int level = 0, song = 0, part = 0, bar = 0;
    bool loop = false;
};

struct Command {
    enum class Type { Play, Stop, SetContext, Cue } type;
    Context context;
};

/// One note start, logged with its exact time, for tests and the UI.
struct StartedNote {
    int32_t id;
    int16_t slot, pitch;
    double time;
};

class Engine {
public:
    static constexpr int kMaxBlock = 256;

    explicit Engine(double sampleRate);
    ~Engine();

    // audio thread
    void render(int frames, float *left, float *right);

    // song-data thread
    bool setLocation(const std::string &locationJSON, const std::string &masterJSON);

    // transport thread
    void post(const Command &c) { commands_.push(c); }

    struct Playhead {
        std::atomic<int32_t> playing{0}, song{0}, part{0}, bar{0}, tick{0}, barTicks{48};
        std::atomic<double> time{0};
    } playhead;

    SPSCQueue<StartedNote, 1024> started;
    /// When set (tests), every note start is also written here; read only once rendering is done.
    std::vector<StartedNote> *log = nullptr;

    double sampleRate() const { return env_.sampleRate; }

private:
    void renderBlock(int n, float *left, float *right);
    void handle(const Command &c);
    void sequence(double blockEnd);
    void scheduleTick(double t0, double td);
    void playNote(int slot, const PartSnap &part, const NoteEvent &note, double t, double sd);
    void advance();
    void clampPosition();
    void applySpace(const SongSnap &song, const Music &mu, double t);
    void applyMaster(const SongSnap &song);
    void flushPending(double until);
    Module *moduleFor(int slot);
    void retire(LocationSnap *s);
    void collect();

    RenderEnv env_;
    Wavetables tables_;
    double now_ = 0; // start of the current block
    uint64_t frames_ = 0;
    RNG rng_{0xC0FFEEu};

    // modules, one of each kind per slot so a snapshot can change engines without allocating
    SynthPool pool_;
    std::unique_ptr<Module> modules_[4][4]; // [slot][drums, bass, chords, lead]

    // snapshots
    SnapshotBuilder builder_;
    std::atomic<LocationSnap *> pending_{nullptr};
    LocationSnap *current_ = nullptr;
    SPSCQueue<LocationSnap *, 16> retired_;

    SPSCQueue<Command, 64> commands_;

    // sequencer
    bool playing_ = false;
    struct Pos { int s = 0, c = 0, p = 0, tk = 0; } pos_;
    bool cued_ = false;
    Pos cue_;
    Context ctx_;
    double nextT_ = 0;
    struct Pending {
        double time, duration;
        int slot, pitch;
        int32_t id;
        Music music;
        float params[sp::count];
    };
    static constexpr int kPending = 256;
    Pending pend_[kPending];
    int pendingCount_ = 0;

    // buses
    float track_[4][2][kMaxBlock];
    float send_[4][kMaxBlock]; // delay L, R, reverb L, R
    float mix_[2][kMaxBlock];

    // track effects
    Building buildings_[4][3];
    BuildingType buildingTypes_[4][3] = {};
    int buildingSong_ = -1;
    void buildFx(const SongSnap &song);
    void applyFx(const SongSnap &song, const PartSnap &part, double t);

    // shared space
    std::vector<float> delayL_, delayR_;
    size_t delayPos_ = 0;
    Automation delayTime_, delayFeedback_, delayTone_;
    Biquad toneL_, toneR_;
    Convolver reverb_;
    const ReverbIR *reverbIR_ = nullptr;

    // master
    Biquad lowcutL_, lowcutR_, highcutL_, highcutR_;
    Drive saturate_;
    float volume_ = .64f;
    const SongSnap *masterFrom_ = nullptr;
};

} // namespace aw
