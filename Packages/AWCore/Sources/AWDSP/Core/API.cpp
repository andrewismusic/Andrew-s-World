// The C boundary (AWDSP.h) over aw::Engine.
#include "../include/AWDSP.h"

#include <string>

#include "Engine.hpp"

struct aw_engine {
    aw::Engine engine;
    explicit aw_engine(double sr) : engine(sr) {}
};

namespace {
aw::Context toContext(aw_context c) { return {c.level, c.song, c.part, c.bar, c.loop != 0}; }
} // namespace

extern "C" {

aw_engine *aw_engine_create(double sample_rate) {
    if (!(sample_rate >= 8000 && sample_rate <= 384000)) return nullptr;
    return new aw_engine(sample_rate);
}

void aw_engine_destroy(aw_engine *e) { delete e; }

void aw_engine_render(aw_engine *e, int32_t frames, float *left, float *right) {
    if (e && frames > 0) e->engine.render(frames, left, right);
}

int32_t aw_engine_set_location(aw_engine *e, const char *location_json, const char *master_json) {
    if (!e || !location_json || !master_json) return -1;
    return e->engine.setLocation(location_json, master_json) ? 0 : -1;
}

void aw_engine_play(aw_engine *e, aw_context c) {
    if (e) e->engine.post({aw::Command::Type::Play, toContext(c)});
}

void aw_engine_stop(aw_engine *e) {
    if (e) e->engine.post({aw::Command::Type::Stop, {}});
}

void aw_engine_set_context(aw_engine *e, aw_context c) {
    if (e) e->engine.post({aw::Command::Type::SetContext, toContext(c)});
}

void aw_engine_cue(aw_engine *e, int32_t song, int32_t part, int32_t bar) {
    if (e) e->engine.post({aw::Command::Type::Cue, {0, song, part, bar, false}});
}

aw_playhead aw_engine_playhead(const aw_engine *e) {
    aw_playhead p{};
    if (!e) return p;
    auto &h = e->engine.playhead;
    p.playing = h.playing.load(std::memory_order_relaxed);
    p.song = h.song.load(std::memory_order_relaxed);
    p.part = h.part.load(std::memory_order_relaxed);
    p.bar = h.bar.load(std::memory_order_relaxed);
    p.tick = h.tick.load(std::memory_order_relaxed);
    p.bar_ticks = h.barTicks.load(std::memory_order_relaxed);
    p.time = h.time.load(std::memory_order_relaxed);
    return p;
}

void aw_engine_play_count_in(aw_engine *e, aw_context c, int32_t bars) {
    if (!e) return;
    aw::Command cmd{aw::Command::Type::Play, toContext(c)};
    cmd.a = bars;
    e->engine.post(cmd);
}

void aw_engine_cancel_cue(aw_engine *e) {
    if (e) e->engine.post({aw::Command::Type::Cue, {0, -1, 0, 0, false}});
}

void aw_engine_audition(aw_engine *e, int32_t note_id) {
    if (!e) return;
    aw::Command cmd{aw::Command::Type::Audition, {}};
    cmd.a = note_id;
    e->engine.post(cmd);
}

void aw_engine_live_on(aw_engine *e, int32_t id, int32_t song, int32_t part, int32_t slot, int32_t pitch) {
    if (!e) return;
    aw::Command cmd{aw::Command::Type::LiveOn, {0, song, part, 0, false}};
    cmd.a = id;
    cmd.b = slot;
    cmd.c = pitch;
    e->engine.post(cmd);
}

void aw_engine_live_off(aw_engine *e, int32_t id) {
    if (!e) return;
    aw::Command cmd{aw::Command::Type::LiveOff, {}};
    cmd.a = id;
    e->engine.post(cmd);
}

void aw_engine_metronome(aw_engine *e, int32_t on, int32_t ticks, float volume) {
    if (!e) return;
    aw::Command cmd{aw::Command::Type::Metronome, {}};
    cmd.a = on;
    cmd.b = ticks;
    cmd.f = volume;
    e->engine.post(cmd);
}

void aw_engine_add_sample(aw_engine *e, const char *id, const float *data, int32_t frames, double rate) {
    if (e && id && data && frames > 0 && rate > 0) e->engine.addSample(id, data, (size_t)frames, rate);
}

int32_t aw_engine_take_ticks(aw_engine *e, aw_tick_event *out, int32_t capacity) {
    if (!e) return 0;
    int32_t n = 0;
    aw::TickEvent t;
    while (n < capacity && e->engine.ticks.pop(t)) out[n++] = {t.time, t.song, t.part, t.bar, t.tick, t.barTicks, 0};
    return n;
}

int32_t aw_engine_take_notes(aw_engine *e, aw_note_event *out, int32_t capacity) {
    if (!e) return 0;
    int32_t n = 0;
    aw::StartedNote s;
    while (n < capacity && e->engine.started.pop(s)) out[n++] = {s.time, s.duration, s.id, s.slot, s.pitch, 0};
    return n;
}

int32_t aw_engine_take_started_notes(aw_engine *e, int32_t *ids, int32_t capacity) {
    if (!e) return 0;
    int32_t n = 0;
    aw::StartedNote s;
    while (n < capacity && e->engine.started.pop(s)) ids[n++] = s.id;
    // drop the rest so the queue never backs up when nobody is reading
    if (n == capacity)
        while (e->engine.started.pop(s)) {}
    return n;
}

}
