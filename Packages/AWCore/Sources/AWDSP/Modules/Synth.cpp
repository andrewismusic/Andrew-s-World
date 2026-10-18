#include "Synth.hpp"

#include <algorithm>
#include <cmath>

namespace aw {

namespace {

const int kBaseMidi[3] = {24, 48, 60}; // bass, chords, lead

int floorDiv(int a, int b) { return (int)std::floor((double)a / b); }
int mod(int a, int b) { return ((a % b) + b) % b; }

int degMidi(const Music &m, int base, int d, int oct) {
    const int *sc = kScales[m.scale];
    int n = m.scaleLength();
    return base + m.key + 12 * (floorDiv(d, n) + oct) + sc[mod(d, n)];
}

double mtof(double m) { return 440 * std::pow(2.0, (m - 69) / 12); }

float oddEven(int n, float oe) {
    float b = (oe - .5f) * 2;
    if (n == 1) return 1;
    if (n % 2) return b > 0 ? 1 - b : 1;
    return b < 0 ? 1 + b : 1;
}

Wave waveOf(float v) { return (Wave)std::clamp((int)std::floor(v * 3 + .5f), 0, 3); }

} // namespace

void SynthVoice::trigger(const RenderEnv &e, const Setup &s) {
    env = &e;
    const float *v = s.v;
    const Music &m = *s.music;
    const double t = s.time, dur = s.duration, sr = e.sampleRate;
    active = true;
    start = t;

    double beat = 60.0 / m.bpm;
    double at = .001 * std::pow(2000.0, (double)v[sp::at]), dc = .02 * std::pow(200.0, (double)v[sp::dc]);
    float su = v[sp::su];
    double rl = .01 * std::pow(400.0, (double)v[sp::rl]);
    float cf = hz(v[sp::cf]);
    filterQ = .5f + v[sp::rs] * v[sp::rs] * 18;
    float fe = v[sp::fe] * 6;
    double fd = .01 * std::pow(200.0, (double)v[sp::fd]);
    int oct = (int)std::floor(v[sp::oc] * 4 + .5f) - 2;
    waveA = waveOf(v[sp::wa]);
    waveB = waveOf(v[sp::wb]);
    int tb = (int)std::floor(v[sp::tb] * 36 + .5f) - 12;
    float det = v[sp::dt] * 50;
    float gl = v[sp::gl] * v[sp::gl] * .5f;
    float ab = s.engine == EngineKind::Lead ? v[sp::ab] : 0, sb = 1 - ab;

    int degs[kDegrees];
    degrees = 1;
    degs[0] = s.degree;
    if (s.engine == EngineKind::Chords) {
        int ci = std::clamp((int)std::floor(v[sp::ch] * 6 + .5f), 0, 6), spread = (int)std::floor(v[sp::spr] * 2 + .5f);
        degrees = kChordSize[ci];
        for (int i = 0; i < degrees; ++i)
            degs[i] = s.degree + kChordDegrees[ci][i] + (spread >= 1 && i == 1 ? 7 : 0) + (spread == 2 && i == 0 ? -7 : 0);
    }
    float levelScale = s.engine == EngineKind::Bass ? .55f : s.engine == EngineKind::Lead ? .34f : .34f / std::sqrt((float)degrees);
    float level = v[sp::lv] * v[sp::lv] * 1.25f * levelScale;
    end = t + std::max(.012, dur);
    stopAt = end + rl * 5 + .06;
    releaseTime = (float)rl;

    for (Automation *a : {&cutoff, &amp, &partialDecay}) {
        a->reset(0);
        a->setSampleTime(e.dt);
    }
    filterType = (FilterType)std::clamp((int)std::floor(v[sp::ft] * 2 + .5f), 0, 2);
    filter.clear();
    cutoff.setValueAtTime(std::min(18000.f, cf * std::exp2(fe)), t);
    cutoff.setTargetAtTime(std::min(18000.f, cf), t + .001, fd / 3);
    drive.set(v[sp::dr]);
    crush.set(v[sp::cr]);

    attack = std::min(at, std::max(.002, dur * .95));
    amp.setValueAtTime(0, t);
    amp.linearRampToValueAtTime(level, t + attack);
    amp.setTargetAtTime(level * su, t + attack, dc / 3);
    amp.setTargetAtTime(0, end, rl / 4);

    // LFOs: rates in beats; depth in cents for pitch and cutoff
    auto rate = [&](float r) { return (float)(1 / (kLfoBeats[std::clamp((int)std::floor(r * 9 + .5f), 0, 9)] * beat)); };
    lfoPitchCents = v[sp::lp] > .01f ? v[sp::lp] * v[sp::lp] * 1200 : 0;
    lfoPitchHz = rate(v[sp::lpr]);
    lfoCutCents = v[sp::lc] > .01f ? v[sp::lc] * 4 * 1200 : 0;
    lfoCutHz = rate(v[sp::lcr]);
    lfoAmp = v[sp::la] > .01f ? v[sp::la] : 0;
    lfoAmpHz = rate(v[sp::lar]);

    route.set((v[sp::pn] - .5f) * 2, v[sp::dl], v[sp::rv]);

    oscs = sb > .01f;
    ratioB = std::exp2(tb / 12.f);
    detuneB = std::exp2(det / 1200);
    gainA = (1 - v[sp::mx]) * sb;
    gainB = v[sp::mx] * sb;
    gainSub = v[sp::sb] > .01f ? v[sp::sb] * .8f * sb : 0;
    gainNoise = v[sp::nz] > .01f && oscs ? v[sp::nz] * .6f * sb : 0;

    bool chromatic = m.scaleLength() == 12;
    int base = kBaseMidi[s.engine == EngineKind::Bass ? 0 : s.engine == EngineKind::Chords ? 1 : 2];
    int rootMidi = degMidi(m, base, s.degree, oct);
    partialCount = 0;
    float amps[8], total = 0;
    for (int n = 1; n <= 8; ++n) total += amps[n - 1] = v[sp::h1 + n - 1] * oddEven(n, v[sp::oe]);
    if (total == 0) total = 1;
    additiveGain = ab > .01f ? ab * 1.1f / std::max(1.f, total * .55f) : 0;
    float stretch = (v[sp::st] - .5f) * .12f;
    if (additiveGain > 0) {
        partialDecay.setValueAtTime(1, t);
        partialDecay.setTargetAtTime(0, t + attack, dc / 3);
    }

    for (int d = 0; d < degrees; ++d) {
        int dd = degs[d];
        double fr;
        if (chromatic) {
            int rel = dd - s.degree;
            fr = mtof(rootMidi + 12 * floorDiv(rel, 7) + kScales[1][mod(rel, 7)]);
        } else {
            fr = mtof(degMidi(m, base, dd, oct));
        }
        float prev = s.lastFreq[d] ? s.lastFreq[d] : s.lastFreq[0];
        freq[d].reset(0);
        freq[d].setSampleTime(e.dt);
        if (gl > .002f && prev) {
            freq[d].setValueAtTime(prev, t);
            freq[d].exponentialRampToValueAtTime((float)fr, t + gl);
        } else {
            freq[d].setValueAtTime((float)fr, t);
        }
        phA[d] = phB[d] = phSub[d] = 0;
        fmDepth[d] = v[sp::fm] > .01f ? v[sp::fm] * v[sp::fm] * (float)fr * 8 : 0;
        if (additiveGain > 0)
            for (int n = 1; n <= 8; ++n) {
                float am = amps[n - 1];
                if (am < .01f) continue;
                float ra = std::pow((float)n, 1 + stretch);
                if (fr * ra > 18000) continue;
                Partial &p = partials[partialCount++];
                p.degree = d;
                p.ratio = ra;
                p.detune = std::exp2((rng.uniform() - .5f) * v[sp::sh] * 30 / 1200);
                p.amp = am;
                p.decays = n > 1 && v[sp::dm] > .01f;
                p.target = p.decays ? am * (1 - v[sp::dm] * .92f * (n - 1) / 7) : am;
                p.phase = 0;
            }
        s.lastFreq[d] = (float)fr;
    }
    (void)sr;
}

void SynthVoice::release(double t2, bool fast) {
    if (!active || t2 >= stopAt) return;
    t2 = std::max(t2, start + .003);
    amp.cancelAndHoldAtTime(t2);
    amp.setTargetAtTime(0, t2, fast ? .018 : releaseTime / 4);
    stopAt = t2 + (fast ? .12 : releaseTime * 5 + .06);
    end = std::min(end, t2);
}

void SynthVoice::render(double t0, int frames, const TrackBus &bus) {
    const double dt = env->dt;
    const Wavetables &tables = *env->tables;
    float pitchF = 1, ampLfo = 1;
    double t = t0;
    for (int i = 0; i < frames; ++i, t += dt) {
        if (t < start) continue;
        if (t >= stopAt) {
            active = false;
            return;
        }
        double age = t - start;
        float cutCents = 0;
        if ((i & 15) == 0 || age < dt * 1.5) {
            pitchF = lfoPitchCents ? std::exp2((float)std::sin(kTwoPi * lfoPitchHz * age) * lfoPitchCents / 1200) : 1.f;
            if (lfoCutCents) cutCents = (float)std::sin(kTwoPi * lfoCutHz * age) * lfoCutCents;
            ampLfo = lfoAmp ? (1 - lfoAmp * .5f) + (float)std::sin(kTwoPi * lfoAmpHz * age) * lfoAmp * .5f : 1.f;
        }
        float sum = 0;
        float decay = additiveGain > 0 ? partialDecay.step(t, dt) : 0;
        for (int d = 0; d < degrees; ++d) {
            float f = freq[d].step(t, dt);
            if (oscs) {
                float fB = f * ratioB * detuneB * pitchF;
                float yB = tables.sample(waveB, phB[d], fB);
                phB[d] += fB * dt;
                phB[d] -= std::floor(phB[d]);
                float fA = (f + yB * fmDepth[d]) * pitchF;
                float yA = tables.sample(waveA, phA[d], fA);
                phA[d] += fA * dt;
                phA[d] -= std::floor(phA[d]);
                sum += yA * gainA + yB * gainB;
                if (gainSub) {
                    float fs = f * .5f * pitchF;
                    sum += (float)std::sin(kTwoPi * phSub[d]) * gainSub;
                    phSub[d] += fs * dt;
                    phSub[d] -= std::floor(phSub[d]);
                }
            }
        }
        if (additiveGain > 0) {
            float add = 0;
            for (int k = 0; k < partialCount; ++k) {
                Partial &p = partials[k];
                float fp = freq[p.degree].current() * p.ratio * p.detune * pitchF;
                float g = p.decays ? p.target + (p.amp - p.target) * decay : p.amp;
                add += (float)std::sin(kTwoPi * p.phase) * g;
                p.phase += fp * dt;
                p.phase -= std::floor(p.phase);
            }
            sum += add * additiveGain;
        }
        if (gainNoise) sum += rng.bipolar() * gainNoise;

        float fc = cutoff.step(t, dt);
        if ((i & 15) == 0 || age < dt * 1.5) {
            float f = lfoCutCents ? fc * std::exp2(cutCents / 1200) : fc;
            filter.set(filterType, f, filterQ, env->sampleRate);
        }
        float y = filter.process(sum);
        if (drive.on) y = drive(y);
        if (crush.on) y = crush(y);
        y *= amp.step(t, dt) * ampLfo;
        route.add(bus, i, y);
    }
    filter.settle();
}

SynthPool::Handle SynthPool::allocate(const void *owner) {
    int pick = -1;
    for (int i = 0; i < kVoices; ++i)
        if (!voices[i].active) { pick = i; break; }
    if (pick < 0) {
        pick = 0;
        for (int i = 1; i < kVoices; ++i)
            if (voices[i].stopAt < voices[pick].stopAt) pick = i;
    }
    SynthVoice &v = voices[pick];
    v.active = false;
    v.generation++;
    v.owner = owner;
    return {pick, v.generation};
}

void SynthPool::admit(Handle h, double now, double t) {
    // liveV = liveV.filter(x => x.end + .6 > now)
    int k = 0;
    for (int i = 0; i < liveCount_; ++i) {
        SynthVoice *v = get(live_[i]);
        if (v && v->end + .6 > now) live_[k++] = live_[i];
    }
    liveCount_ = k;
    if (liveCount_ < (int)(sizeof live_ / sizeof live_[0])) live_[liveCount_++] = h;
    while (liveCount_ > kCap) {
        if (SynthVoice *v = get(live_[0])) v->release(std::max(now, t - .002), true);
        for (int i = 1; i < liveCount_; ++i) live_[i - 1] = live_[i];
        --liveCount_;
    }
}

void SynthPool::panic() {
    for (auto &v : voices) v.active = false;
    liveCount_ = 0;
}

void SynthModule::start(const NoteStart &n) {
    const float *v = n.params;
    bool mono = engine_ == EngineKind::Chords || engine_ == EngineKind::Bass || (engine_ == EngineKind::Lead && v[sp::mo] < .5f);
    if (mono)
        if (SynthVoice *pv = pool_.get(mono_)) pv->release(n.time, true);
    SynthPool::Handle h = pool_.allocate(this);
    SynthVoice &voice = pool_.voices[h.index];
    voice.rng = RNG(seed_ = seed_ * 1664525u + 1013904223u);
    voice.trigger(env_, {engine_, n.music, n.pitch, n.time, n.duration, v, lastFreq_});
    if (mono) mono_ = h;
    pool_.admit(h, now_, n.time);
}

uint32_t SynthModule::startHeld(const NoteStart &n) {
    start(n);
    // the newest voice of this module is the one just started
    SynthPool::Handle h{-1, 0};
    double latest = -1;
    for (int i = 0; i < SynthPool::kVoices; ++i) {
        SynthVoice &v = pool_.voices[i];
        if (v.active && v.owner == this && v.start == n.time && v.start > latest) {
            latest = v.start;
            h = {i, v.generation};
        }
    }
    if (h.index < 0) return 0;
    return (uint32_t)(h.index + 1) | (h.generation << 8);
}

void SynthModule::release(uint32_t token, double t) {
    if (!token) return;
    SynthPool::Handle h{(int)(token & 0xFF) - 1, token >> 8};
    if (SynthVoice *v = pool_.get(h)) v->release(t, false);
}

void SynthModule::render(double t0, int frames, const TrackBus &bus) {
    for (auto &v : pool_.voices)
        if (v.active && v.owner == this) v.render(t0, frames, bus);
}

void SynthModule::stopped() {
    for (float &f : lastFreq_) f = 0;
}

void SynthModule::panic() {
    for (auto &v : pool_.voices)
        if (v.owner == this) v.active = false;
    mono_ = {};
}

} // namespace aw
