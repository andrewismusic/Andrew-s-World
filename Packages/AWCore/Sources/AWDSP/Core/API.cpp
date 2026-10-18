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
