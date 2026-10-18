// AWDSP: Andrew's World's sound engine. C++ inside, plain C at this boundary so Swift can call it directly.
//
// Threads:
//   - aw_engine_render runs on the audio thread and never allocates, locks or logs.
//   - aw_engine_set_location (song data) and the transport calls may run on any other threads, but each kind of call
//     must come from one thread at a time: one thread posts song data, one thread posts transport commands.
//   - aw_engine_playhead may be read from anywhere.
#ifndef AWDSP_H
#define AWDSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aw_engine aw_engine;

/// Creates an engine for one sample rate. Rebuild it (and post the location again) when the rate changes.
aw_engine *aw_engine_create(double sample_rate);
void aw_engine_destroy(aw_engine *engine);

/// Renders `frames` stereo frames into two non-interleaved buffers, replacing what is there.
void aw_engine_render(aw_engine *engine, int32_t frames, float *left, float *right);

/// Posts the songs of the location being played. `location_json` is one entry of the save's `worlds` array and
/// `master_json` the save's `master` object, both exactly as the save writes them. Songs that didn't change since
/// the last post are reused, so posting after every edit is cheap. Returns 0 on success, or -1 if the JSON could
/// not be read (the engine keeps playing what it had).
int32_t aw_engine_set_location(aw_engine *engine, const char *location_json, const char *master_json);

/// Where the player is, as the transport sees it: the zoom level (0 song … 4 note), the selection and the
/// pattern level's Loop switch. The sequencer follows this the way the prototype's advance() follows `sel`.
typedef struct aw_context {
    int32_t level;
    int32_t song;
    int32_t part;
    int32_t bar;
    int32_t loop;
} aw_context;

void aw_engine_play(aw_engine *engine, aw_context context);
void aw_engine_stop(aw_engine *engine);
void aw_engine_set_context(aw_engine *engine, aw_context context);
/// Jumps to a song, part and bar when the current bar ends.
void aw_engine_cue(aw_engine *engine, int32_t song, int32_t part, int32_t bar);

typedef struct aw_playhead {
    int32_t playing;
    int32_t song;
    int32_t part;
    int32_t bar;
    /// Clock ticks: 12 a quarter note.
    int32_t tick;
    int32_t bar_ticks;
    /// Seconds of audio rendered since the engine was created.
    double time;
} aw_playhead;

aw_playhead aw_engine_playhead(const aw_engine *engine);

/// Ids of notes that started sounding since the last call, newest last. Returns how many were written.
int32_t aw_engine_take_started_notes(aw_engine *engine, int32_t *ids, int32_t capacity);

/// Starts playback with a count-in of `count_in_bars` bars of clicks (0 for none).
void aw_engine_play_count_in(aw_engine *engine, aw_context context, int32_t count_in_bars);
/// Cancels a cue.
void aw_engine_cancel_cue(aw_engine *engine);

/// Plays one note of the current song now, as it would sound in the song.
void aw_engine_audition(aw_engine *engine, int32_t note_id);
/// Holds a note from a keyboard until aw_engine_live_off with the same id. `pitch` is a scale step, or a drum row.
void aw_engine_live_on(aw_engine *engine, int32_t id, int32_t song, int32_t part, int32_t slot, int32_t pitch);
void aw_engine_live_off(aw_engine *engine, int32_t id);

/// Metronome: on or off, a click every `ticks` clock ticks (12 a quarter note), volume 0–1.
void aw_engine_metronome(aw_engine *engine, int32_t on, int32_t ticks, float volume);

/// Adds a collected sound (mono) under an id; songs refer to it by id. Call from the song-data thread.
void aw_engine_add_sample(aw_engine *engine, const char *id, const float *data, int32_t frames, double sample_rate);

typedef struct aw_tick_event {
    double time;
    int32_t song, part, bar, tick, bar_ticks, reserved;
} aw_tick_event;

typedef struct aw_note_event {
    double time;
    double duration;
    int32_t id, slot, pitch, reserved;
} aw_note_event;

/// Clock ticks and note starts scheduled since the last call, with their exact times on the engine clock
/// (seconds since the engine was created). Each returns how many it wrote.
int32_t aw_engine_take_ticks(aw_engine *engine, aw_tick_event *out, int32_t capacity);
int32_t aw_engine_take_notes(aw_engine *engine, aw_note_event *out, int32_t capacity);

#ifdef __cplusplus
}
#endif

#endif
