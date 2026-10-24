#include "Engine.hpp"

#include "../DSP/Common.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aw {

namespace {

int kindIndex(EngineKind e) {
    switch (e) {
    case EngineKind::Drums: return 0;
    case EngineKind::Bass: return 1;
    case EngineKind::Chords: return 2;
    case EngineKind::Lead: return 3;
    default: return -1;
    }
}

constexpr int kTicksPerQuarter = 12;

int partLength(const SongSnap &so, int c) { return so.parts.empty() ? 1 : so.parts[c].length; }

int barSteps(const Music &mu, bool triplet) { return triplet ? (mu.ts * mu.tpb) / 2 : mu.ts * mu.tpb / 3; }

} // namespace

Engine::Engine(double sampleRate) : builder_(sampleRate, samples_) {
    env_.sampleRate = sampleRate;
    env_.dt = 1.0 / sampleRate;
    tables_.build(sampleRate);
    env_.tables = &tables_;
    for (int s = 0; s < 4; ++s) {
        modules_[s][0] = std::make_unique<DrumKit>(env_);
        modules_[s][1] = std::make_unique<SynthModule>(env_, pool_, EngineKind::Bass);
        modules_[s][2] = std::make_unique<SynthModule>(env_, pool_, EngineKind::Chords);
        modules_[s][3] = std::make_unique<SynthModule>(env_, pool_, EngineKind::Lead);
    }
    size_t delayLen = (size_t)(4.0 * sampleRate) + 2;
    delayL_.assign(delayLen, 0);
    delayR_.assign(delayLen, 0);
    for (Automation *a : {&delayTime_, &delayFeedback_, &delayTone_}) a->setSampleTime(env_.dt);
    delayTime_.reset(.375f);
    delayFeedback_.reset(.35f * .9f);
    delayTone_.reset(hz(.6f));
    reverb_.prepare((size_t)(4.5 * sampleRate) + 1);
    for (auto &row : buildings_)
        for (auto &b : row) b.prepare(sampleRate);
    lowcutL_.set(FilterType::Highpass, 10, 1, sampleRate);
    lowcutR_ = lowcutL_;
    highcutL_.set(FilterType::Lowpass, 20000, 1, sampleRate);
    highcutR_ = highcutL_;
    saturate_.set(0);
    saturate_.on = true; // the master shaper is always in the chain, so it always clips at ±1
}

Engine::~Engine() {
    collect();
    delete pending_.exchange(nullptr);
    delete current_;
}

void Engine::retire(LocationSnap *s) {
    if (!s) return;
    if (!retired_.push(s)) {
        // can't happen in practice (the producer drains on every post); leak rather than free here
    }
}

void Engine::collect() {
    LocationSnap *s;
    while (retired_.pop(s)) delete s;
}

bool Engine::setLocation(const std::string &locationJSON, const std::string &masterJSON) {
    collect();
    std::unique_ptr<LocationSnap> snap = builder_.build(locationJSON, masterJSON);
    if (!snap) return false;
    // a snapshot the audio thread never picked up is ours to free
    delete pending_.exchange(snap.release());
    return true;
}

bool Engine::setSong(int index, const std::string &songJSON) {
    collect();
    std::unique_ptr<LocationSnap> snap = builder_.buildWithSong(index, songJSON);
    if (!snap) return false;
    delete pending_.exchange(snap.release());
    return true;
}

Module *Engine::moduleFor(int slot) {
    if (!current_) return nullptr;
    int k = kindIndex(current_->engines[slot]);
    return k < 0 ? nullptr : modules_[slot][k].get();
}

void Engine::render(int frames, float *left, float *right) {
    while (frames > 0) {
        int n = std::min(frames, kMaxBlock);
        renderBlock(n, left, right);
        left += n;
        right += n;
        frames -= n;
    }
}

void Engine::handle(const Command &c) {
    switch (c.type) {
    case Command::Type::Play: {
        ctx_ = c.context;
        if (!current_ || current_->songs.empty() || ctx_.level < 0) return;
        int L = ctx_.level;
        pos_ = {ctx_.song, L >= 1 ? ctx_.part : 0, L >= 3 ? ctx_.bar : 0, 0};
        nextT_ = now_;
        if (c.a > 0) {
            // count-in: one bar of clicks before the music starts
            clampPosition();
            const Music &mu = current_->songs[pos_.s]->parts[pos_.c].music;
            double beat = 60.0 / mu.bpm * mu.tpb / kTicksPerQuarter;
            for (int b = 0; b < mu.ts * c.a; ++b) addClick(now_ + b * beat, b % mu.ts == 0 ? 2 : 1);
            nextT_ = now_ + mu.ts * c.a * beat;
        }
        cued_ = false;
        pendingCount_ = 0;
        for (int s = 0; s < 4; ++s)
            for (auto &m : modules_[s]) m->stopped();
        playing_ = true;
        break;
    }
    case Command::Type::Stop:
        playing_ = false;
        pendingCount_ = 0;
        cued_ = false;
        break;
    case Command::Type::SetContext:
        ctx_ = c.context;
        break;
    case Command::Type::Cue:
        cue_ = {c.context.song, c.context.part, c.context.bar, 0};
        cued_ = c.context.song >= 0;
        break;
    case Command::Type::Audition:
        audition(c.a);
        break;
    case Command::Type::LiveOn:
        startLive(c);
        break;
    case Command::Type::LiveOff:
        stopLive(c.a);
        break;
    case Command::Type::Metronome:
        metronome_ = c.a != 0;
        metronomeRate_ = std::max(1, c.b);
        metronomeVolume_ = c.f;
        break;
    }
}

void Engine::audition(int32_t noteId) {
    if (!current_ || current_->songs.empty()) return;
    int s = std::clamp(ctx_.song, 0, (int)current_->songs.size() - 1);
    const SongSnap &so = *current_->songs[s];
    for (const NoteEvent &nt : so.notes) {
        if (nt.id != noteId) continue;
        Module *m = moduleFor(nt.slot);
        if (!m || nt.part >= so.parts.size()) return;
        const PartSnap &part = so.parts[nt.part];
        double sd = 60.0 / part.music.bpm / kTicksPerQuarter * (nt.triplet ? 2 : 3);
        const Sample *smp = current_->engines[nt.slot] == EngineKind::Drums ? so.drumSamples[nt.slot][std::clamp((int)nt.pitch, 0, 7)] : nullptr;
        m->start({now_, nt.len * sd, nt.pitch, &so.params[nt.params], &part.music, smp, nt.id, (int16_t)s, (int16_t)nt.part});
        return;
    }
}

void Engine::startLive(const Command &c) {
    if (!current_ || current_->songs.empty()) return;
    stopLive(c.a);
    int s = std::clamp(c.context.song, 0, (int)current_->songs.size() - 1);
    const SongSnap &so = *current_->songs[s];
    if (so.parts.empty()) return;
    const PartSnap &part = so.parts[std::clamp(c.context.part, 0, (int)so.parts.size() - 1)];
    int slot = std::clamp(c.b, 0, 3);
    Module *m = moduleFor(slot);
    if (!m || part.laneParams[slot] < 0) return;
    bool drums = current_->engines[slot] == EngineKind::Drums;
    int row = drums ? std::clamp(c.c, 0, 7) : 0;
    const float *params = &so.params[part.laneParams[slot] + row * paramCount(current_->engines[slot])];
    int partIndex = std::clamp(c.context.part, 0, (int)so.parts.size() - 1);
    uint32_t token = m->startHeld({now_, 60, c.c, params, &part.music, drums ? so.drumSamples[slot][row] : nullptr, -1, (int16_t)s, (int16_t)partIndex});
    for (auto &l : live_)
        if (l.id < 0) {
            l = {c.a, slot, token};
            break;
        }
}

void Engine::stopLive(int32_t id) {
    for (auto &l : live_)
        if (l.id == id) {
            if (Module *m = moduleFor(l.slot)) m->release(l.token, now_);
            l.id = -1;
        }
}

void Engine::retargetVoices() {
    // sounding notes take up the edit: each voice finds its note (or, played live, its lane) in the new snapshot
    for (int slot = 0; slot < 4; ++slot) {
        EngineKind e = current_->engines[slot];
        int k = kindIndex(e);
        if (k < 1) continue;
        const Module *m = modules_[slot][k].get();
        for (SynthVoice &v : pool_.voices) {
            if (!v.active || v.owner != m || !v.noteId || v.song < 0 || v.song >= (int)current_->songs.size()) continue;
            const SongSnap &so = *current_->songs[v.song];
            if (v.from == &so) continue;
            v.from = &so;
            if (v.noteId > 0) {
                const NoteEvent *nt = so.noteWithId(v.noteId);
                if (!nt || nt->slot != slot || nt->part >= so.parts.size()) continue;
                v.retarget(&so.params[nt->params], so.parts[nt->part].music, e);
            } else if (v.part >= 0 && v.part < (int)so.parts.size() && so.parts[v.part].laneParams[slot] >= 0) {
                v.retarget(&so.params[so.parts[v.part].laneParams[slot]], so.parts[v.part].music, e);
            }
        }
    }
}

void Engine::addClick(double t, int accent) {
    for (auto &c : clicks_)
        if (c.time < 0) {
            c = {t, accent};
            return;
        }
}

// the prototype's "wood" metronome: a short sine knock with a little band-passed noise, straight to the output
void Engine::renderClicks(float *L, float *R, int n) {
    const double dt = env_.dt;
    for (auto &c : clicks_) {
        if (c.time < 0 || c.time >= now_ + n * dt) continue;
        for (auto &s : sounding_)
            if (s.start < 0 || now_ - s.start > .1) {
                s = {c.time, c.accent};
                break;
            }
        c.time = -1;
    }
    for (auto &s : sounding_) {
        if (s.start < 0) continue;
        float v = metronomeVolume_ * metronomeVolume_ * (s.accent == 2 ? 1.f : s.accent ? .75f : .5f) * .9f;
        double f = s.accent == 2 ? 1250 : s.accent ? 1050 : 880;
        for (int i = 0; i < n; ++i) {
            double age = now_ + i * dt - s.start;
            if (age < 0 || age > .08) continue;
            float env = age < .001 ? (float)(age / .001) : (float)std::exp(-(age - .001) / .018);
            float y = ((float)std::sin(kTwoPi * f * age) + rng_.bipolar() * .15f) * env * v;
            L[i] += y;
            R[i] += y;
        }
        if (now_ + n * dt - s.start > .08) s.start = -1;
    }
}

void Engine::clampPosition() {
    const auto &songs = current_->songs;
    pos_.s = std::clamp(pos_.s, 0, (int)songs.size() - 1);
    const SongSnap &so = *songs[pos_.s];
    pos_.c = std::clamp(pos_.c, 0, std::max(0, (int)so.parts.size() - 1));
    pos_.p = std::clamp(pos_.p, 0, partLength(so, pos_.c) - 1);
}

void Engine::advance() {
    const SongSnap &so = *current_->songs[pos_.s];
    const Music &mu = so.parts[pos_.c].music;
    if (++pos_.tk < mu.barTicks()) return;
    pos_.tk = 0;
    if (cued_) {
        pos_ = cue_;
        cued_ = false;
        return;
    }
    int L = ctx_.level;
    // looping holds the selected bar on the track and pattern views, and the selected part on the part view
    if (L >= 4 || (L >= 2 && ctx_.loop)) {
        pos_ = {ctx_.song, ctx_.part, ctx_.bar, 0};
        return;
    }
    if (L >= 2 || (L == 1 && ctx_.loop)) {
        if (pos_.s != ctx_.song || pos_.c != ctx_.part) {
            pos_ = {ctx_.song, ctx_.part, 0, 0};
            return;
        }
        pos_.p = (pos_.p + 1) % partLength(so, pos_.c);
        return;
    }
    if (pos_.s != ctx_.song) {
        pos_ = {ctx_.song, 0, 0, 0};
        return;
    }
    if (++pos_.p >= partLength(so, pos_.c)) {
        pos_.p = 0;
        pos_.c = (pos_.c + 1) % (int)so.parts.size();
    }
}

void Engine::applySpace(const SongSnap &song, const Music &mu, double t, bool barStart) {
    if (!barStart && spaceFrom_ == &song && spaceBpm_ == mu.bpm) return;
    spaceFrom_ = &song;
    spaceBpm_ = mu.bpm;
    double beat = 60.0 / mu.bpm;
    const SpaceSettings &sp = song.space;
    int di = std::clamp((int)std::floor(sp.time * 6 + .5f), 0, 6);
    delayTime_.setTargetAtTime((float)std::min(3.9, kDelayBeats[di] * beat), t, .02);
    delayFeedback_.setTargetAtTime(sp.feedback * .9f, t, .02);
    delayTone_.setTargetAtTime(std::min(18000.f, hz(sp.tone)), t, .02);
    if (song.reverb.get() != reverbIR_) {
        reverbIR_ = song.reverb.get();
        reverb_.setKernel(reverbIR_ ? &reverbIR_->kernel : nullptr);
    }
}

void Engine::applyMaster(const SongSnap &song) {
    if (masterFrom_ == &song) return;
    masterFrom_ = &song;
    // on the Farm, SATURATE tops out where 15% used to be
    bool farm = current_ && current_->skin == "farm";
    saturate_.set(song.master.saturate * .6f * (farm ? .15f : 1.f));
    saturate_.on = true;
    float v = song.master.volume;
    volume_ = v * v * 1.25f * .8f;
}

void Engine::buildFx(const SongSnap &song) {
    bool same = buildingSong_ == pos_.s;
    for (int s = 0; s < 4; ++s)
        for (int i = 0; i < 3; ++i) same &= buildingTypes_[s][i] == song.fx[s][i];
    if (same) return;
    buildingSong_ = pos_.s;
    for (int s = 0; s < 4; ++s)
        for (int i = 0; i < 3; ++i) {
            buildingTypes_[s][i] = song.fx[s][i];
            buildings_[s][i].reset(song.fx[s][i]);
        }
}

void Engine::applyFx(const SongSnap &song, const PartSnap &part, double t) {
    double beat = 60.0 / part.music.bpm;
    for (int s = 0; s < 4; ++s)
        for (int i = 0; i < 3; ++i)
            if (buildingTypes_[s][i] != BuildingType::None) buildings_[s][i].set(part.fx[s][i], t, beat);
}

void Engine::playNote(int slot, const PartSnap &part, const NoteEvent &nt, double t, double sd) {
    const SongSnap &so = *current_->songs[pos_.s];
    Module *m = moduleFor(slot);
    if (!m) return;
    const float *base = &so.params[nt.params];
    bool drums = current_->engines[slot] == EngineKind::Drums;
    float prob = base[drums ? (int)dp::pr : (int)sp::pr];
    if (rng_.uniform() >= prob) return;
    double dur = nt.len * sd;
    const Sample *smp = drums ? so.drumSamples[slot][std::clamp((int)nt.pitch, 0, 7)] : nullptr;
    StartedNote sn{nt.id, (int16_t)slot, nt.pitch, t, dur};
    started.push(sn);
    if (log) log->push_back(sn);
    int count = 1;
    double stepLen = nt.len;
    if (nt.ratchet) {
        stepLen = kRatchetSteps[nt.ratchet];
        count = std::max(1, (int)std::ceil(nt.len / stepLen - 1e-6));
    }
    if (count <= 1) {
        m->start({t, dur, nt.pitch, base, &part.music, smp, nt.id, (int16_t)pos_.s, (int16_t)nt.part});
        return;
    }
    int cnt = std::min(count, 48);
    int nParams = paramCount(current_->engines[slot]);
    for (int i = 0; i < cnt; ++i) {
        double t0 = i * stepLen * sd;
        if (t0 >= dur - 1e-6) break;
        if (pendingCount_ >= kPending) break;
        Pending &p = pend_[pendingCount_++];
        float fr = cnt > 1 ? (float)i / (cnt - 1) : 0;
        float w = nt.rampMode == RampMode::Away ? fr : nt.rampMode == RampMode::Toward ? 1 - fr : 0;
        std::memcpy(p.params, base, sizeof(float) * nParams);
        if (w && nt.ramp >= 0)
            for (int k = 0; k < nParams; ++k) {
                float target = so.params[nt.ramp + k];
                if (target == target) p.params[k] = base[k] + (target - base[k]) * w;
            }
        p.time = t + t0;
        p.duration = std::max(.012, std::min(stepLen * sd * .85, dur - t0));
        p.slot = slot;
        p.pitch = nt.pitch;
        p.id = nt.id;
        p.music = part.music;
        p.sample = smp;
    }
}

void Engine::flushPending(double until) {
    int k = 0;
    for (int i = 0; i < pendingCount_; ++i) {
        Pending &p = pend_[i];
        if (p.time < until) {
            if (Module *m = moduleFor(p.slot)) m->start({p.time, p.duration, p.pitch, p.params, &p.music, p.sample});
        } else {
            if (k != i) pend_[k] = pend_[i];
            ++k;
        }
    }
    pendingCount_ = k;
}

void Engine::scheduleTick(double t0, double td) {
    const SongSnap &so = *current_->songs[pos_.s];
    const PartSnap &se = so.parts[pos_.c];
    const Music &mu = se.music;
    if (pos_.tk == 0) buildFx(so);
    // space and master follow edits within the bar, not only from the next one
    if (pos_.tk % 3 == 0) {
        applySpace(so, mu, t0, pos_.tk == 0);
        applyMaster(so);
        applyFx(so, se, t0);
    }
    if (metronome_ && pos_.tk % metronomeRate_ == 0) addClick(t0, pos_.tk == 0 ? 2 : pos_.tk % mu.tpb == 0 ? 1 : 0);
    ticks.push({t0, pos_.s, pos_.c, pos_.p, pos_.tk, mu.barTicks()});
    for (int ti = 0; ti < 4; ++ti) {
        if (!so.audible[ti]) continue;
        const LaneSnap &lane = se.lanes[ti];
        if (pos_.p >= (int)lane.bars.size()) continue;
        const BarSnap &bar = lane.bars[pos_.p];
        int per = bar.triplet ? 2 : 3;
        if (pos_.tk % per) continue;
        int st = pos_.tk / per;
        if (st >= barSteps(mu, bar.triplet)) continue;
        double sd = td * per;
        double t = t0 + (!bar.triplet && st % 2 == 1 ? sd * mu.swing * .5 : 0);
        for (uint32_t i = bar.first; i < bar.first + bar.count; ++i) {
            const NoteEvent &nt = so.notes[i];
            if (nt.step == st) playNote(ti, se, nt, t, sd);
            else if (nt.step > st) break;
        }
    }
    playhead.song.store(pos_.s, std::memory_order_relaxed);
    playhead.part.store(pos_.c, std::memory_order_relaxed);
    playhead.bar.store(pos_.p, std::memory_order_relaxed);
    playhead.tick.store(pos_.tk, std::memory_order_relaxed);
    playhead.barTicks.store(mu.barTicks(), std::memory_order_relaxed);
}

void Engine::sequence(double blockEnd) {
    if (!playing_ || !current_ || current_->songs.empty()) return;
    while (nextT_ < blockEnd) {
        clampPosition();
        const SongSnap &so = *current_->songs[pos_.s];
        if (so.parts.empty()) {
            playing_ = false;
            return;
        }
        const Music &mu = so.parts[pos_.c].music;
        double td = 60.0 / mu.bpm / kTicksPerQuarter;
        scheduleTick(nextT_, td);
        nextT_ += td;
        advance();
    }
}

void Engine::renderBlock(int n, float *outL, float *outR) {
    if (LocationSnap *snap = pending_.exchange(nullptr)) {
        if (current_ && snap->skin != current_->skin)
            for (int s = 0; s < 4; ++s)
                for (auto &m : modules_[s]) m->panic();
        retire(current_);
        current_ = snap;
        masterFrom_ = nullptr;
        // the old snapshot may own the impulse in use; move to the new one's now
        if (!current_->songs.empty()) {
            int s = std::clamp(playing_ ? pos_.s : ctx_.song, 0, (int)current_->songs.size() - 1);
            reverbIR_ = current_->songs[s]->reverb.get();
            reverb_.setKernel(reverbIR_ ? &reverbIR_->kernel : nullptr);
        } else {
            reverbIR_ = nullptr;
            reverb_.setKernel(nullptr);
        }
        if (!playing_ && !current_->songs.empty()) {
            int s = std::clamp(ctx_.song, 0, (int)current_->songs.size() - 1);
            applyMaster(*current_->songs[s]);
        }
        retargetVoices();
    }
    Command c;
    while (commands_.pop(c)) handle(c);

    const double dt = env_.dt, end = now_ + n * dt;
    for (int s = 0; s < 4; ++s)
        for (auto &m : modules_[s]) m->beginBlock(now_);
    sequence(end);
    flushPending(end);

    for (int s = 0; s < 4; ++s) {
        std::fill(track_[s][0], track_[s][0] + n, 0.f);
        std::fill(track_[s][1], track_[s][1] + n, 0.f);
    }
    for (auto &b : send_) std::fill(b, b + n, 0.f);
    std::fill(mix_[0], mix_[0] + n, 0.f);
    std::fill(mix_[1], mix_[1] + n, 0.f);

    for (int s = 0; s < 4; ++s) {
        Module *m = moduleFor(s);
        if (!m) continue;
        TrackBus bus{track_[s][0], track_[s][1], send_[0], send_[1], send_[2], send_[3]};
        m->render(now_, n, bus);
        for (auto &b : buildings_[s]) b.process(track_[s][0], track_[s][1], n, now_);
        for (int i = 0; i < n; ++i) {
            mix_[0][i] += track_[s][0][i];
            mix_[1][i] += track_[s][1][i];
        }
    }

    // shared delay: in → delay → tone → feedback; the tone's output returns to the mix
    const size_t dlen = delayL_.size();
    double t = now_;
    for (int i = 0; i < n; ++i, t += dt) {
        float dtime = delayTime_.step(t, dt), fb = delayFeedback_.step(t, dt), tone = delayTone_.step(t, dt);
        if ((i & 15) == 0) {
            toneL_.set(FilterType::Lowpass, tone, 1, env_.sampleRate);
            toneR_.copyCoefficients(toneL_);
        }
        double d = std::clamp((double)dtime * env_.sampleRate, 1.0, (double)dlen - 2);
        double rp = (double)delayPos_ - d;
        while (rp < 0) rp += dlen;
        size_t i0 = (size_t)rp, i1 = (i0 + 1) % dlen;
        float fr = (float)(rp - i0);
        float dl = delayL_[i0] + (delayL_[i1] - delayL_[i0]) * fr;
        float dr = delayR_[i0] + (delayR_[i1] - delayR_[i0]) * fr;
        float yl = toneL_.process(dl), yr = toneR_.process(dr);
        delayL_[delayPos_] = send_[0][i] + yl * fb;
        delayR_[delayPos_] = send_[1][i] + yr * fb;
        delayPos_ = (delayPos_ + 1) % dlen;
        mix_[0][i] += yl;
        mix_[1][i] += yr;
    }
    toneL_.settle();
    toneR_.settle();

    // shared reverb, returned at 0.8
    reverb_.process(send_[2], send_[3], mix_[0], mix_[1], n, .8f);

    // master: low cut → high cut → saturate (always clips at ±1) → volume. Glue arrives in milestone 3.
    for (int i = 0; i < n; ++i) {
        float l = highcutL_.process(lowcutL_.process(mix_[0][i]));
        float r = highcutR_.process(lowcutR_.process(mix_[1][i]));
        outL[i] = saturate_(l) * volume_;
        outR[i] = saturate_(r) * volume_;
    }
    renderClicks(outL, outR, n);

    frames_ += n;
    now_ = frames_ * dt;
    playhead.playing.store(playing_ ? 1 : 0, std::memory_order_relaxed);
    playhead.time.store(now_, std::memory_order_relaxed);
}

} // namespace aw
