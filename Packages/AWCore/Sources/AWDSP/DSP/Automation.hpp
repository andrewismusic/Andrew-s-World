// A parameter that changes over time the way a Web Audio AudioParam does: set, linear and exponential ramps,
// exponential approach to a target, cancel-and-hold. Porting the prototype's envelopes is then line for line.
// Times are seconds on the engine clock. Fixed capacity, no allocation.
#pragma once

#include <cmath>
#include <cstdint>

namespace aw {

class Automation {
public:
    static constexpr int kCapacity = 16;

    explicit Automation(float initial = 0) : value_(initial), initial_(initial) {}

    void reset(float v) {
        count_ = 0;
        next_ = 0;
        value_ = initial_ = v;
        mode_ = Mode::Hold;
        segStart_ = curT_ = -1e300;
        rampActive_ = false;
    }

    void setValueAtTime(float v, double t) { insert({Kind::Set, t, v, 0}); }
    void linearRampToValueAtTime(float v, double t) { insert({Kind::Linear, t, v, 0}); }
    void exponentialRampToValueAtTime(float v, double t) { insert({Kind::Exponential, t, v, 0}); }
    void setTargetAtTime(float target, double t, double tau) { insert({Kind::Target, t, target, tau}); }

    /// Removes events at or after t (cancelScheduledValues).
    void cancelScheduledValues(double t) {
        int keep = next_;
        while (keep < count_ && ev_[keep].time < t) ++keep;
        count_ = keep;
    }

    /// cancelAndHoldAtTime: the value at t is kept, nothing after t happens. A ramp that spans t is cut at t.
    void cancelAndHoldAtTime(double t) {
        float v = valueAt(t);
        int i = next_;
        while (i < count_ && ev_[i].time <= t) ++i;
        if (i < count_ && (ev_[i].kind == Kind::Linear || ev_[i].kind == Kind::Exponential)) {
            ev_[i].time = t;
            ev_[i].value = v;
            count_ = i + 1;
        } else {
            count_ = i;
            insert({Kind::Set, t, v, 0});
        }
    }

    /// The value at time t, without advancing. Used for cancel-and-hold and by tests.
    float valueAt(double t) const {
        // walk events from the current state
        float v = value_;
        double segStart = curT_ > segStart_ ? curT_ : segStart_;
        Mode mode = mode_;
        float target = target_;
        double tau = tau_;
        for (int i = next_; i < count_; ++i) {
            const Event &e = ev_[i];
            if (e.kind == Kind::Linear || e.kind == Kind::Exponential) {
                if (t < e.time) return ramp(e, v, segStart, t);
                v = e.value;
                segStart = e.time;
                mode = Mode::Hold;
                continue;
            }
            if (t < e.time) return run(mode, v, target, tau, segStart, t);
            v = run(mode, v, target, tau, segStart, e.time);
            segStart = e.time;
            if (e.kind == Kind::Set) {
                v = e.value;
                mode = Mode::Hold;
            } else {
                mode = Mode::Target;
                target = e.value;
                tau = e.tau;
            }
        }
        return run(mode, v, target, tau, segStart, t);
    }

    /// Fills out[0..n) with the value at t0, t0+dt, … and advances. Returns true if the value is constant over
    /// the block (so callers can skip per-sample work), in which case only out[0] is meaningful.
    bool process(float *out, int n, double t0, double dt) {
        if (next_ >= count_ && mode_ == Mode::Hold) {
            out[0] = value_;
            return true;
        }
        double t = t0;
        for (int s = 0; s < n; ++s, t += dt) out[s] = step(t, dt);
        return false;
    }

    /// The value at time t, advancing state. Calls must move forward in time by dt each.
    float step(double t, double dt) {
        curT_ = t + dt;
        while (next_ < count_ && ev_[next_].time <= t) enter(ev_[next_++], t);
        // a ramp is driven by the event after the current one
        if (next_ < count_) {
            const Event &e = ev_[next_];
            if (e.kind == Kind::Linear || e.kind == Kind::Exponential) {
                if (!rampActive_) startRamp(e, t, dt);
                float v = value_;
                if (e.kind == Kind::Linear) value_ += rampStep_;
                else if (rampExpOK_) value_ *= rampStep_;
                return v;
            }
        }
        if (mode_ == Mode::Target) {
            float v = value_;
            value_ = target_ + (value_ - target_) * targetK_;
            if (std::fabs(value_ - target_) < 1e-9f) value_ = target_;
            return v;
        }
        return value_;
    }

    float current() const { return value_; }
    bool idle() const { return next_ >= count_ && mode_ == Mode::Hold; }

private:
    enum class Kind : uint8_t { Set, Linear, Exponential, Target };
    enum class Mode : uint8_t { Hold, Target };
    struct Event {
        Kind kind;
        double time;
        float value;
        double tau;
    };

    void insert(Event e) {
        if (count_ == kCapacity) {
            // full: drop already-passed events to make room, else replace the last
            if (next_ > 0) compact();
            else count_ = kCapacity - 1;
        }
        // keep time order; equal times stay in call order
        int i = count_;
        while (i > next_ && ev_[i - 1].time > e.time) {
            ev_[i] = ev_[i - 1];
            --i;
        }
        ev_[i] = e;
        ++count_;
        rampActive_ = false;
    }

    void compact() {
        int k = 0;
        for (int i = next_; i < count_; ++i) ev_[k++] = ev_[i];
        count_ = k;
        next_ = 0;
    }

    void enter(const Event &e, double t) {
        rampActive_ = false;
        segStart_ = e.time;
        switch (e.kind) {
        case Kind::Set:
        case Kind::Linear:
        case Kind::Exponential:
            value_ = e.value;
            mode_ = Mode::Hold;
            break;
        case Kind::Target:
            mode_ = Mode::Target;
            target_ = e.value;
            tau_ = e.tau;
            targetK_ = tau_ > 0 ? (float)std::exp(-dt_ / tau_) : 0.f;
            if (tau_ <= 0) value_ = target_;
            // catch up if we entered late within a sample
            if (tau_ > 0 && t > e.time) value_ = target_ + (value_ - target_) * (float)std::exp(-(t - e.time) / tau_);
            break;
        }
    }

    void startRamp(const Event &e, double t, double dt) {
        rampActive_ = true;
        double remaining = e.time - t;
        double steps = remaining / dt;
        if (steps < 1) steps = 1;
        if (e.kind == Kind::Linear) {
            rampStep_ = (float)((e.value - value_) / steps);
        } else {
            rampExpOK_ = value_ != 0 && (value_ > 0) == (e.value > 0) && e.value != 0;
            rampStep_ = rampExpOK_ ? (float)std::pow((double)e.value / value_, 1.0 / steps) : 1.f;
        }
        dt_ = dt;
    }

    static float ramp(const Event &e, float v0, double t0, double t) {
        double span = e.time - t0;
        if (span <= 0) return e.value;
        double f = (t - t0) / span;
        if (e.kind == Kind::Linear) return (float)(v0 + (e.value - v0) * f);
        if (v0 == 0 || e.value == 0 || (v0 > 0) != (e.value > 0)) return v0;
        return (float)(v0 * std::pow((double)e.value / v0, f));
    }

    static float run(Mode m, float v, float target, double tau, double t0, double t) {
        if (m == Mode::Hold || t <= t0) return v;
        if (tau <= 0) return target;
        return (float)(target + (v - target) * std::exp(-(t - t0) / tau));
    }

public:
    /// The per-sample step; set once by the owner (defaults to 1/48000).
    void setSampleTime(double dt) { dt_ = dt; }

private:
    Event ev_[kCapacity];
    int count_ = 0, next_ = 0;
    float value_ = 0, initial_ = 0;
    Mode mode_ = Mode::Hold;
    float target_ = 0, targetK_ = 0;
    double tau_ = 0, segStart_ = -1e300, curT_ = -1e300;
    bool rampActive_ = false, rampExpOK_ = false;
    float rampStep_ = 0;
    double dt_ = 1.0 / 48000;
};

} // namespace aw
