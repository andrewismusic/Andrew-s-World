#include "DrumKit.hpp"

#include <algorithm>
#include <cmath>

namespace aw {

namespace {

/// env(): 0, a linear rise to `peak` over `a`, then an exponential fall with time constant `tau`.
void envelope(Automation &p, double t, float peak, double tau, double a = .001) {
    p.setValueAtTime(0, t);
    p.linearRampToValueAtTime(peak, t + a);
    p.setTargetAtTime(0, t + a, tau);
}

} // namespace

void DrumVoice::trigger(const RenderEnv &e, int r, double t, const float *o) {
    env = &e;
    row = r;
    active = true;
    start = t;
    double sr = e.sampleRate;
    float ratio = std::exp2((o[dp::tu] - .5f) * 2);
    double dec = .01 * std::pow(kDrumMaxDecay[r] / .01, (double)o[dp::dc]);
    float pu = o[dp::pu];
    float amp = o[dp::lv] * o[dp::lv] * 1.25f * o[dp::klv] * o[dp::klv] * 1.25f * .8f;
    end = t + dec * 5 + .15;

    for (Automation *a : {&level, &freq, &gain, &noiseGain, &n2freq, &n2gain}) {
        a->reset(0);
        a->setSampleTime(e.dt);
    }
    tone.clear();
    tone.set(FilterType::Lowpass, std::min(20000.f, std::max(10.f, hz(o[dp::tn]))), .6, sr);
    drive.set(o[dp::kdr]);
    crush.set(o[dp::kcr]);
    level.setValueAtTime(amp, t);
    route.set((o[dp::kpn] - .5f) * 2, o[dp::kdl], o[dp::krv]);

    phase = modPhase = 0;
    hasOsc = hasNoise = hasNoise2 = squares = n1b_on = false;
    modDepth = 0;
    noisePre = 1;
    n1a.clear();
    n1b.clear();
    n2.clear();
    n2cf = -1;

    switch (r) {
    case 0: { // kick: a sine that drops to its pitch, plus a click of noise when punchy
        hasOsc = true;
        wave = Wave::Sine;
        float f0 = 46 * ratio;
        freq.setValueAtTime(f0 * (1 + pu * 7), t);
        freq.exponentialRampToValueAtTime(f0, t + .015 + pu * .06);
        envelope(gain, t, 1.5f, dec * .45);
        if (pu > .05f) {
            hasNoise = true;
            noiseStop = t + .04;
            n1a.set(FilterType::Highpass, 2500, 1, sr);
            envelope(noiseGain, t, pu * .4f, .005);
        }
        break;
    }
    case 1: { // snare: a falling triangle and high-passed noise
        hasOsc = true;
        wave = Wave::Triangle;
        freq.setValueAtTime(190 * ratio, t);
        freq.exponentialRampToValueAtTime(150 * ratio, t + .04);
        envelope(gain, t, 1 - pu * .75f, dec * .25);
        hasNoise = true;
        noiseStop = t + dec * 4 + .1;
        n1a.set(FilterType::Highpass, 1000 * ratio, 1, sr);
        envelope(noiseGain, t, .25f + pu * .9f, dec * .5);
        break;
    }
    case 2: { // clap: SPREAD sets how far apart the hand-claps land; PUNCH brightens them
        hasNoise = true;
        noiseStop = t + dec * 4 + .2;
        n1a.set(FilterType::Bandpass, 1100 * ratio * (1 + pu * .7f), 1.4, sr);
        double spread = .0025 + o[dp::spr] * .024;
        noiseGain.setValueAtTime(0, t);
        for (int i = 0; i < 3; ++i) {
            double tt = t + i * spread * (1 + (i == 2 ? .3 : 0));
            noiseGain.setValueAtTime(1.3f, tt);
            noiseGain.setTargetAtTime(.12f, tt + .001, .004);
        }
        double tl = t + 3 * spread * 1.1;
        noiseGain.setValueAtTime(1.2f, tl);
        noiseGain.setTargetAtTime(0, tl + .001, dec * .35);
        break;
    }
    case 3: { // rim: a hard crack of noise, a knock that drops in pitch, and a short resonant ring
        float f0 = 1650 * ratio;
        hasNoise = true;
        noiseStop = t + .012;
        n1a.set(FilterType::Highpass, 3000, .7, sr);
        n1b.set(FilterType::Bandpass, 5200 * ratio, 1.1, sr);
        n1b_on = true;
        noiseGain.setValueAtTime(0, t);
        noiseGain.linearRampToValueAtTime(1.8f + pu * 1.4f, t + .0004);
        noiseGain.setTargetAtTime(0, t + .0006, .0016);
        hasOsc = true;
        wave = Wave::Triangle;
        freq.setValueAtTime(f0 * 2.2f, t);
        freq.exponentialRampToValueAtTime(f0, t + .004);
        freq.exponentialRampToValueAtTime(f0 * .55f, t + dec + .01);
        envelope(gain, t, 1.1f, dec * .35, .0003);
        hasNoise2 = true;
        noise2Stop = t + dec + .03;
        n2Q = 14;
        n2freq.setValueAtTime(2300 * ratio, t);
        n2freq.exponentialRampToValueAtTime(1500 * ratio, t + dec);
        envelope(n2gain, t, 2.4f, dec * .4, .0005);
        break;
    }
    case 4:
    case 5: { // hats: noise and, when punchy, four metallic squares, high-passed
        hasNoise = true;
        noiseStop = t + dec * 4 + .1;
        noisePre = 1 - pu * .6f;
        n1a.set(FilterType::Highpass, std::min(16000.f, 6500 * ratio), .8, sr);
        if (pu > .05f) {
            squares = true;
            const float f[4] = {317, 465, 540, 800};
            for (int i = 0; i < 4; ++i) {
                sqFreq[i] = f[i] * ratio;
                sqPhase[i] = 0;
            }
            sqGain = pu * .22f;
        }
        envelope(noiseGain, t, 1.2f, r == 4 ? dec * .15 : dec * .6);
        break;
    }
    case 6: { // tom
        hasOsc = true;
        wave = Wave::Sine;
        float f0 = 105 * ratio;
        freq.setValueAtTime(f0 * (1 + pu * 1.6f), t);
        freq.exponentialRampToValueAtTime(f0, t + .1);
        envelope(gain, t, 1.3f, dec * .45);
        break;
    }
    default: { // perc: a two-operator FM blip
        hasOsc = true;
        wave = Wave::Sine;
        freq.setValueAtTime(350 * ratio, t);
        modRatio = 1.47f;
        modDepth = pu * 450 * ratio;
        envelope(gain, t, 1.1f, dec * .3);
        break;
    }
    }
}

void DrumVoice::choke(double t) {
    if (!active || start >= t) return;
    level.cancelScheduledValues(t);
    level.setTargetAtTime(0, t, .004);
    end = std::min(end, t + .05);
}

void DrumVoice::render(double t0, int frames, const TrackBus &bus) {
    const double dt = env->dt;
    const Wavetables &tables = *env->tables;
    double t = t0;
    for (int i = 0; i < frames; ++i, t += dt) {
        if (t < start) continue;
        if (t >= end) {
            active = false;
            return;
        }
        float x = 0;
        if (hasOsc) {
            float f = freq.step(t, dt), g = gain.step(t, dt);
            if (t < end) {
                if (modDepth > 0) {
                    float m = (float)std::sin(modPhase * kTwoPi);
                    modPhase += f * modRatio * dt;
                    modPhase -= std::floor(modPhase);
                    f += m * modDepth;
                }
                x += tables.sample(wave, phase, f) * g;
                phase += f * dt;
                phase -= std::floor(phase);
            }
        }
        if (hasNoise) {
            float g = noiseGain.step(t, dt);
            float n = t < noiseStop ? rng.bipolar() * noisePre : 0.f;
            if (squares) {
                for (int k = 0; k < 4; ++k) {
                    n += tables.sample(Wave::Square, sqPhase[k], sqFreq[k]) * sqGain;
                    sqPhase[k] += sqFreq[k] * dt;
                    sqPhase[k] -= std::floor(sqPhase[k]);
                }
            }
            float y = n1a.process(n);
            if (n1b_on) y = n1b.process(y);
            x += y * g;
        }
        if (hasNoise2) {
            float f = n2freq.step(t, dt), g = n2gain.step(t, dt);
            if ((i & 15) == 0 || n2cf < 0) {
                n2.set(FilterType::Bandpass, f, n2Q, env->sampleRate);
                n2cf = f;
            }
            float n = t < noise2Stop ? rng.bipolar() : 0.f;
            x += n2.process(n) * g;
        }
        float y = tone.process(x);
        if (drive.on) y = drive(y);
        if (crush.on) y = crush(y);
        y *= level.step(t, dt);
        route.add(bus, i, y);
    }
    tone.settle();
}

void DrumKit::start(const NoteStart &n) {
    int row = std::clamp(n.pitch, 0, 7);
    // choke this row's last hit
    if (last_[row] >= 0 && voices_[last_[row]].row == row) voices_[last_[row]].choke(n.time);
    int pick = -1;
    for (int i = 0; i < kVoices; ++i)
        if (!voices_[i].active) { pick = i; break; }
    if (pick < 0) {
        // all busy: take the one that ends soonest
        pick = 0;
        for (int i = 1; i < kVoices; ++i)
            if (voices_[i].end < voices_[pick].end) pick = i;
    }
    voices_[pick].rng = RNG(seed_ = seed_ * 1664525u + 1013904223u);
    voices_[pick].trigger(env_, row, n.time, n.params);
    last_[row] = pick;
}

void DrumKit::render(double t0, int frames, const TrackBus &bus) {
    for (auto &v : voices_)
        if (v.active) v.render(t0, frames, bus);
}

void DrumKit::panic() {
    for (auto &v : voices_) v.active = false;
    for (int &l : last_) l = -1;
}

} // namespace aw
