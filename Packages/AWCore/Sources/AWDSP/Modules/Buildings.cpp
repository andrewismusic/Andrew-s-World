#include "Buildings.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "../Core/Params.hpp"
#include "../DSP/Common.hpp"

namespace aw {

const char *const kBuildingParams[5][3] = {
    {"", "", ""}, {"amt", "tone", "mix"}, {"time", "fb", "mix"}, {"rate", "depth", "mix"}, {"type", "cf", "rs"}};

BuildingType buildingNamed(const std::string &n) {
    if (n == "mill") return BuildingType::Mill;
    if (n == "well") return BuildingType::Well;
    if (n == "windmill") return BuildingType::Windmill;
    if (n == "barn") return BuildingType::Barn;
    return BuildingType::None;
}

const char *buildingKey(BuildingType t) {
    switch (t) {
    case BuildingType::Mill: return "mill";
    case BuildingType::Well: return "well";
    case BuildingType::Windmill: return "windmill";
    case BuildingType::Barn: return "barn";
    default: return "";
    }
}

float buildingDefault(BuildingType t, int q) {
    static const float d[5][3] = {{0, 0, 0}, {.45f, .78f, .6f}, {3.f / 6, .4f, .32f}, {.3f, .5f, .5f}, {0, .7f, .25f}};
    return d[(int)t][q];
}

Oversampler2x::Oversampler2x() {
    // windowed-sinc half-band low-pass at a quarter of the doubled rate
    const int mid = kTaps / 2;
    double sum = 0;
    for (int i = 0; i < kTaps; ++i) {
        int k = i - mid;
        double s = k == 0 ? .5 : std::sin(kPi * k / 2) / (kPi * k);
        double w = .5 - .5 * std::cos(kTwoPi * i / (kTaps - 1));
        h_[i] = (float)(s * w);
        sum += h_[i];
    }
    for (float &x : h_) x = (float)(x / sum);
}

void Oversampler2x::clear() {
    std::memset(up_, 0, sizeof up_);
    std::memset(down_, 0, sizeof down_);
}

float Oversampler2x::interpolate(float x, int) {
    up_[upPos_] = x;
    float y = 0;
    int p = upPos_;
    for (int i = 0; i < kTaps; ++i) {
        y += h_[i] * up_[p];
        p = p ? p - 1 : kTaps - 1;
    }
    upPos_ = (upPos_ + 1) % kTaps;
    return y;
}

float Oversampler2x::decimate(float a, float b) {
    float y = 0;
    for (float x : {a, b}) {
        down_[downPos_] = x;
        downPos_ = (downPos_ + 1) % kTaps;
    }
    int p = downPos_ ? downPos_ - 1 : kTaps - 1;
    for (int i = 0; i < kTaps; ++i) {
        y += h_[i] * down_[p];
        p = p ? p - 1 : kTaps - 1;
    }
    return y;
}

void Building::prepare(double sampleRate) {
    sr_ = sampleRate;
    dt_ = 1 / sampleRate;
    dL_.assign((size_t)(4 * sampleRate) + 2, 0);
    dR_.assign(dL_.size(), 0);
    for (Automation *a : {&dry_, &wet_, &p1_, &p2_, &p3_}) a->setSampleTime(dt_);
}

void Building::reset(BuildingType type) {
    type_ = type;
    std::fill(dL_.begin(), dL_.end(), 0.f);
    std::fill(dR_.begin(), dR_.end(), 0.f);
    pos_ = 0;
    fL_.clear();
    fR_.clear();
    osL_.clear();
    osR_.clear();
    std::memset(millL_, 0, sizeof millL_);
    std::memset(millR_, 0, sizeof millR_);
    millPos_ = 0;
    lfoPhase_ = 0;
    for (float &x : last_) x = -1;
    dry_.reset(1);
    wet_.reset(0);
    switch (type) {
    case BuildingType::Mill: p2_.reset(8000); break;               // tone
    case BuildingType::Well:
        p1_.reset(.375f); // time
        p2_.reset(0);     // feedback
        fL_.set(FilterType::Lowpass, 5000, .5, sr_);
        fR_.copyCoefficients(fL_);
        break;
    case BuildingType::Windmill: p1_.reset(.5f); p2_.reset(.004f); break;
    case BuildingType::Barn: dry_.reset(0); wet_.reset(1); p2_.reset(2000); p3_.reset(1); break;
    default: break;
    }
}

void Building::set(const float v[3], double t, double beat) {
    if (v[0] == last_[0] && v[1] == last_[1] && v[2] == last_[2] && beat == lastBeat_) return;
    std::memcpy(last_, v, sizeof last_);
    lastBeat_ = beat;
    switch (type_) {
    case BuildingType::Mill:
        driveK_ = std::round((.15f + v[0] * .85f) * 20) * 2.4f;
        p2_.setTargetAtTime(std::min(18000.f, hz(v[1])), t, .02);
        dry_.setTargetAtTime(1 - v[2], t, .02);
        wet_.setTargetAtTime(v[2] * .7f, t, .02);
        break;
    case BuildingType::Well: {
        int di = std::clamp((int)std::floor(v[0] * 6 + .5f), 0, 6);
        p1_.setTargetAtTime((float)std::min(3.9, kDelayBeats[di] * beat), t, .03);
        p2_.setTargetAtTime(v[1] * .9f, t, .02);
        wet_.setTargetAtTime(v[2], t, .02);
        break;
    }
    case BuildingType::Windmill:
        p1_.setTargetAtTime(.05f * std::exp2(v[0] * 7), t, .05);
        p2_.setTargetAtTime(.0005f + v[1] * .007f, t, .05);
        dry_.setTargetAtTime(1 - v[2] * .45f, t, .02);
        wet_.setTargetAtTime(v[2] * .8f, t, .02);
        break;
    case BuildingType::Barn:
        filterType_ = (FilterType)std::clamp((int)std::floor(v[0] * 2 + .5f), 0, 2);
        p2_.setTargetAtTime(std::min(18000.f, hz(v[1])), t, .02);
        p3_.setTargetAtTime(.5f + v[2] * v[2] * 18, t, .02);
        break;
    default: break;
    }
}

void Building::process(float *L, float *R, int n, double t0) {
    if (type_ == BuildingType::None) return;
    const size_t len = dL_.size();
    double t = t0;
    auto readDelay = [&](const std::vector<float> &buf, double samples) {
        samples = std::clamp(samples, 1.0, (double)len - 2);
        double rp = (double)pos_ - samples;
        if (rp < 0) rp += len;
        size_t i0 = (size_t)rp, i1 = (i0 + 1) % len;
        float fr = (float)(rp - i0);
        return buf[i0] + (buf[i1] - buf[i0]) * fr;
    };
    for (int i = 0; i < n; ++i, t += dt_) {
        float dry = dry_.step(t, dt_), wet = wet_.step(t, dt_);
        float a = p1_.step(t, dt_), b = p2_.step(t, dt_), c = p3_.step(t, dt_);
        float xl = L[i], xr = R[i], yl = 0, yr = 0;
        switch (type_) {
        case BuildingType::Mill: {
            if ((i & 15) == 0) {
                fL_.set(FilterType::Lowpass, b, .7, sr_);
                fR_.copyCoefficients(fL_);
            }
            const float k = driveK_;
            auto shape = [k](float x) {
                x = x < -1 ? -1 : x > 1 ? 1 : x;
                return (1 + k) * x / (1 + k * std::fabs(x));
            };
            float sl = osL_.process(xl, shape), sr = osR_.process(xr, shape);
            float dl = millL_[millPos_], dr = millR_[millPos_];
            millL_[millPos_] = sl;
            millR_[millPos_] = sr;
            millPos_ = (millPos_ + 1) % kMillDelay;
            yl = fL_.process(dl);
            yr = fR_.process(dr);
            break;
        }
        case BuildingType::Well: {
            float dl = readDelay(dL_, a * sr_), dr = readDelay(dR_, a * sr_);
            yl = fL_.process(dl);
            yr = fR_.process(dr);
            dL_[pos_] = xl + yl * b;
            dR_[pos_] = xr + yr * b;
            pos_ = (pos_ + 1) % len;
            break;
        }
        case BuildingType::Windmill: {
            double d = (.012 + std::sin(kTwoPi * lfoPhase_) * b) * sr_;
            lfoPhase_ += a * dt_;
            lfoPhase_ -= std::floor(lfoPhase_);
            dL_[pos_] = xl;
            dR_[pos_] = xr;
            yl = readDelay(dL_, d);
            yr = readDelay(dR_, d);
            pos_ = (pos_ + 1) % len;
            break;
        }
        case BuildingType::Barn: {
            if ((i & 15) == 0) {
                fL_.set(filterType_, b, c, sr_);
                fR_.copyCoefficients(fL_);
            }
            yl = fL_.process(xl);
            yr = fR_.process(xr);
            break;
        }
        default: break;
        }
        L[i] = xl * dry + yl * wet;
        R[i] = xr * dry + yr * wet;
    }
    fL_.settle();
    fR_.settle();
}

} // namespace aw
