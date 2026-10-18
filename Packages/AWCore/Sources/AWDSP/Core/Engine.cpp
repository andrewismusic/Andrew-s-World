#include "Engine.hpp"

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

Engine::Engine(double sampleRate) : builder_(sampleRate) {
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
        cued_ = true;
        break;
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
    if (L >= 4 || (L == 3 && ctx_.loop)) {
        pos_ = {ctx_.song, ctx_.part, ctx_.bar, 0};
        return;
    }
    if (L >= 2) {
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

void Engine::applySpace(const SongSnap &song, const Music &mu, double t) {
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
    StartedNote sn{nt.id, (int16_t)slot, nt.pitch, t};
    started.push(sn);
    if (log) log->push_back(sn);
    int count = 1;
    double stepLen = nt.len;
    if (nt.ratchet) {
        stepLen = kRatchetSteps[nt.ratchet];
        count = std::max(1, (int)std::ceil(nt.len / stepLen - 1e-6));
    }
    if (count <= 1) {
        m->start({t, dur, nt.pitch, base, &part.music});
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
    }
}

void Engine::flushPending(double until) {
    int k = 0;
    for (int i = 0; i < pendingCount_; ++i) {
        Pending &p = pend_[i];
        if (p.time < until) {
            if (Module *m = moduleFor(p.slot)) m->start({p.time, p.duration, p.pitch, p.params, &p.music});
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
    if (pos_.tk == 0) {
        applySpace(so, mu, t0);
        applyMaster(so);
        buildFx(so);
    }
    if (pos_.tk % 3 == 0) applyFx(so, se, t0);
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

    frames_ += n;
    now_ = frames_ * dt;
    playhead.playing.store(playing_ ? 1 : 0, std::memory_order_relaxed);
    playhead.time.store(now_, std::memory_order_relaxed);
}

} // namespace aw
