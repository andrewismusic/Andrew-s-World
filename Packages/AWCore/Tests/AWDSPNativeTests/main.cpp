// Native tests for AWDSP: run with `ctest` (CMake) on Linux or macOS. No framework, just checks.
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "Core/Engine.hpp"
#include "DSP/Automation.hpp"
#include "DSP/Biquad.hpp"
#include "DSP/Convolver.hpp"
#include "DSP/FFT.hpp"
#include "Support.hpp"

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                                                                              \
    do {                                                                                                              \
        ++checks;                                                                                                     \
        if (!(cond)) {                                                                                                \
            ++failures;                                                                                               \
            std::printf("FAIL %s:%d: %s — ", __FILE__, __LINE__, #cond);                                              \
            std::printf(__VA_ARGS__);                                                                                 \
            std::printf("\n");                                                                                        \
        }                                                                                                             \
    } while (0)
#define NEAR(a, b, tol) CHECK(std::fabs((double)(a) - (double)(b)) <= (tol), "%g vs %g", (double)(a), (double)(b))

static std::string savePath;

static void testAutomation() {
    const double sr = 48000, dt = 1 / sr;
    aw::Automation a;
    a.setSampleTime(dt);
    a.setValueAtTime(0, 0.01);
    a.linearRampToValueAtTime(1, 0.02);
    a.setTargetAtTime(0.25f, 0.02, 0.05);
    std::vector<float> v(48000);
    for (int i = 0; i < 48000; ++i) v[i] = a.step(i * dt, dt);
    NEAR(v[480], 0, 1e-6);                           // t = 0.01
    NEAR(v[720], 0.5, 2e-3);                         // halfway up the ramp
    NEAR(v[960], 1, 2e-3);                           // top
    double t = 0.1, want = 0.25 + 0.75 * std::exp(-(t - 0.02) / 0.05);
    NEAR(v[4800], want, 1e-3);                       // exponential approach

    aw::Automation e;
    e.setSampleTime(dt);
    e.setValueAtTime(400, 0);
    e.exponentialRampToValueAtTime(100, 0.1);
    float mid = 0;
    for (int i = 0; i <= 2400; ++i) mid = e.step(i * dt, dt);
    NEAR(mid, 200, 0.5);                             // geometric midpoint

    // cancel and hold mid-ramp keeps the value reached
    aw::Automation c;
    c.setSampleTime(dt);
    c.setValueAtTime(0, 0);
    c.linearRampToValueAtTime(1, 1);
    for (int i = 0; i < 4800; ++i) c.step(i * dt, dt);
    c.cancelAndHoldAtTime(0.25);
    float held = 0;
    for (int i = 4800; i < 48000; ++i) held = c.step(i * dt, dt);
    NEAR(held, 0.25, 2e-3);
    NEAR(c.valueAt(0.5), 0.25, 2e-3);
}

static double magnitude(const aw::Biquad &b, double f, double sr) {
    std::complex<double> z = std::polar(1.0, -2 * M_PI * f / sr), z2 = z * z;
    auto num = (double)b.b0 + (double)b.b1 * z + (double)b.b2 * z2;
    auto den = 1.0 + (double)b.a1 * z + (double)b.a2 * z2;
    return std::abs(num / den);
}

static void testBiquad() {
    aw::Biquad b;
    b.set(aw::FilterType::Lowpass, 1000, 6, 48000);
    NEAR(magnitude(b, 1, 48000), 1, 1e-3);
    NEAR(20 * std::log10(magnitude(b, 1000, 48000)), 6, 0.05); // Q in dB: the peak at cutoff is Q dB
    b.set(aw::FilterType::Highpass, 1000, 0, 48000);
    NEAR(magnitude(b, 1000, 48000), 1, 1e-3);
    b.set(aw::FilterType::Bandpass, 2000, 2, 48000);
    NEAR(magnitude(b, 2000, 48000), 1, 1e-3);
}

static void testConvolver() {
    // compare against direct convolution on a random impulse spanning every stage
    const size_t len = 40000, n = 60000;
    aw::RNG rng(3);
    std::vector<float> irL(len), irR(len), x(n), ref(n + len, 0.f), refR(n + len, 0.f);
    for (size_t i = 0; i < len; ++i) {
        irL[i] = rng.bipolar() * std::exp(-(double)i / 8000);
        irR[i] = rng.bipolar() * std::exp(-(double)i / 8000);
    }
    for (size_t i = 0; i < n; i += 997) x[i] = rng.bipolar();
    auto kernel = std::make_unique<aw::ConvKernel>();
    kernel->build(irL.data(), irR.data(), len, 48000);
    // undo the normalisation for the reference
    double power = 0;
    for (size_t i = 0; i < len; ++i) power += irL[i] * irL[i] + irR[i] * irR[i];
    double scale = 1 / std::sqrt(power / (2.0 * len)) * 0.00125 * 44100 / 48000;
    for (size_t i = 0; i < n; ++i)
        if (x[i])
            for (size_t k = 0; k < len; ++k) {
                ref[i + k] += (float)(x[i] * irL[k] * scale);
                refR[i + k] += (float)(x[i] * irR[k] * scale);
            }
    aw::Convolver conv;
    conv.prepare(len);
    conv.setKernel(kernel.get());
    std::vector<float> outL(n, 0.f), outR(n, 0.f), zero(n, 0.f);
    for (size_t i = 0; i < n; i += 200) {
        int m = (int)std::min<size_t>(200, n - i);
        conv.process(&x[i], &x[i], &outL[i], &outR[i], m, 1.f);
    }
    double err = 0, mag = 0;
    for (size_t i = 0; i < n; ++i) {
        err = std::fmax(err, std::fabs(outL[i] - ref[i]));
        err = std::fmax(err, std::fabs(outR[i] - refR[i]));
        mag = std::fmax(mag, std::fabs(ref[i]));
    }
    CHECK(err < mag * 1e-3, "max error %g against peak %g", err, mag);
}

struct Rendered {
    std::vector<float> L, R;
    std::vector<aw::StartedNote> notes;
};

static Rendered renderSong(const std::string &loc, const std::string &master, aw::Context ctx, double seconds,
                           double sr = 48000, std::function<void(aw::Engine &, double)> during = nullptr) {
    aw::Engine engine(sr);
    Rendered r;
    engine.log = &r.notes;
    bool ok = engine.setLocation(loc, master);
    CHECK(ok, "setLocation");
    engine.post({aw::Command::Type::Play, ctx});
    size_t total = (size_t)(seconds * sr);
    r.L.resize(total);
    r.R.resize(total);
    for (size_t i = 0; i < total; i += 128) {
        int n = (int)std::min<size_t>(128, total - i);
        if (during) during(engine, i / sr);
        engine.render(n, &r.L[i], &r.R[i]);
    }
    return r;
}

/// A one-part song with a drum lane; notes are {step, pitch} on bar 0, `triplet` sets the bar grid.
static std::string tinySong(int bpm, double swing, const std::string &bars, const std::string &extraLocks = "") {
    char locks[256];
    std::snprintf(locks, sizeof locks, "\"g.bpm\":%.10g,\"g.sw\":%.10g%s", (bpm - 50) / 150.0, swing, extraLocks.c_str());
    std::string lane = "{\"locks\":{},\"bars\":" + bars + "}";
    std::string empty = "{\"locks\":{},\"bars\":[]}";
    return std::string("{\"skin\":\"farm\",\"name\":\"Farm\",\"songs\":[{\"name\":\"T\",\"style\":\"house\",\"hue\":0,"
                       "\"prog\":[0,0,0,0],\"locks\":{") + locks +
           "},\"mix\":{\"m\":[0,0,0,0],\"s\":[0,0,0,0]},\"sections\":[{\"name\":\"A\",\"hue\":0,\"type\":\"main\",\"locks\":{},\"lanes\":[" +
           lane + "," + empty + "," + empty + "," + empty + "]}],\"fx\":[[null,null,null],[null,null,null],[null,null,null],[null,null,null]]}]}";
}

static std::string note(int id, int step, int pitch, int len = 1) {
    char b[160];
    std::snprintf(b, sizeof b, "{\"id\":%d,\"step\":%d,\"pitch\":%d,\"len\":%d,\"locks\":{},\"ramp\":{}}", id, step, pitch, len);
    return b;
}

static void testTiming() {
    // straight bar at 120 BPM with 50% swing: odd steps are pushed back by half a step × swing
    std::string bars = "[{\"notes\":[" + note(1, 0, 0) + "," + note(2, 1, 1) + "," + note(3, 2, 4) + "," + note(4, 15, 4) + "]}]";
    Rendered r = renderSong(tinySong(120, 0.5, bars), "{\"locks\":{}}", {3, 0, 0, 0, true}, 4.2);
    double sd = 60.0 / 120 / 4; // a sixteenth
    std::vector<double> want = {0, sd + sd * .5 * .5, 2 * sd, 15 * sd + sd * .5 * .5};
    CHECK(r.notes.size() >= 8, "%zu notes", r.notes.size());
    for (size_t i = 0; i < 4 && i < r.notes.size(); ++i) NEAR(r.notes[i].time, want[i], 1e-9);
    // the bar loops: the same four notes two seconds later
    for (size_t i = 0; i < 4 && i + 4 < r.notes.size(); ++i) NEAR(r.notes[i + 4].time, want[i] + 2.0, 1e-9);

    // triplet bar: six steps a beat, no swing
    std::string trip = "[{\"trip\":true,\"notes\":[" + note(1, 0, 0) + "," + note(2, 1, 0) + "," + note(3, 23, 0) + "]}]";
    Rendered t = renderSong(tinySong(100, 0.7, trip), "{\"locks\":{}}", {3, 0, 0, 0, true}, 2.5);
    double st = 60.0 / 100 / 6;
    CHECK(t.notes.size() >= 3, "%zu notes", t.notes.size());
    if (t.notes.size() >= 3) {
        NEAR(t.notes[1].time, st, 1e-9);
        NEAR(t.notes[2].time, 23 * st, 1e-9);
    }
}

static void testDemoSong() {
    std::string save = awtest::readFile(savePath), loc, master;
    bool ok = awtest::locationText(save, 0, loc, master);
    CHECK(ok, "read %s", savePath.c_str());
    if (!ok) return;
    // Old School Hip-Hop: 90 BPM, intro A is two bars; kick on steps 0 and 10
    Rendered r = renderSong(loc, master, {0, 0, 0, 0, false}, 12);
    double sd = 60.0 / 90 / 4;
    std::vector<double> kicks;
    for (auto &n : r.notes)
        if (n.slot == 0 && n.pitch == 0) kicks.push_back(n.time);
    CHECK(kicks.size() >= 2, "%zu kicks", kicks.size());
    if (kicks.size() >= 2) {
        NEAR(kicks[0], 0, 1e-9);
        NEAR(kicks[1], 10 * sd, 1e-9);
    }
    double peak = 0, sum = 0;
    bool finite = true;
    for (size_t i = 0; i < r.L.size(); ++i) {
        finite &= std::isfinite(r.L[i]) && std::isfinite(r.R[i]);
        peak = std::fmax(peak, std::fmax(std::fabs(r.L[i]), std::fabs(r.R[i])));
        sum += r.L[i] * r.L[i];
    }
    double rms = 10 * std::log10(sum / r.L.size() + 1e-20);
    CHECK(finite, "non-finite samples");
    CHECK(peak > 0.05 && peak <= 0.65, "peak %g", peak);
    CHECK(rms > -40 && rms < -6, "rms %g dB", rms);
    // every track plays something in the first 12 seconds (bass enters in part B)
    bool slots[4] = {};
    for (auto &n : r.notes) slots[n.slot] = true;
    CHECK(slots[0] && slots[1] && slots[2], "drums %d bass %d chords %d", slots[0], slots[1], slots[2]);
}

static void testLevels() {
    std::string save = awtest::readFile(savePath), loc, master;
    if (!awtest::locationText(save, 0, loc, master)) return;
    // track level on part B (four bars): the part loops, so bar 0 comes back after four bars
    Rendered r = renderSong(loc, master, {2, 0, 1, 0, false}, 2 * 4 * 60.0 / 90 * 4 + 0.5);
    double bar = 60.0 / 90 * 4;
    int firstKicks = 0, laterKicks = 0;
    for (auto &n : r.notes)
        if (n.slot == 0 && n.pitch == 0) {
            if (std::fabs(n.time) < 1e-6) ++firstKicks;
            if (std::fabs(n.time - 4 * bar) < 1e-6) ++laterKicks;
        }
    CHECK(firstKicks == 1 && laterKicks == 1, "kicks at 0: %d, at 4 bars: %d", firstKicks, laterKicks);

    // pattern level with Loop on bar 2: only bar 2's notes, every bar
    Rendered p = renderSong(loc, master, {3, 0, 1, 2, true}, bar * 3 - 0.01);
    int perBar[3] = {};
    for (auto &n : p.notes) perBar[std::min(2, (int)((n.time + 1e-6) / bar))]++;
    CHECK(perBar[0] == perBar[1] && perBar[1] == perBar[2] && perBar[0] > 0, "%d %d %d", perBar[0], perBar[1], perBar[2]);
}

static void testSnapshotReuse() {
    std::string save = awtest::readFile(savePath), loc, master;
    if (!awtest::locationText(save, 0, loc, master)) return;
    aw::SampleBank bank;
    aw::SnapshotBuilder b(48000, bank);
    auto a = b.build(loc, master);
    auto c = b.build(loc, master);
    CHECK(a && c && a->songs.size() == 8, "songs");
    if (a && c) CHECK(a->songs[0] == c->songs[0], "unchanged songs are reused");
    if (a) {
        const aw::Music &m = a->songs[0]->parts[0].music;
        CHECK(m.bpm == 90 && m.key == 2 && m.scale == 2 && m.ts == 4, "hip-hop score %d %d %d %d", m.bpm, m.key, m.scale, m.ts);
        NEAR(m.swing, 0.55, 1e-6);
    }
}

static void testLiveAndSamples() {
    // a kick row replaced by a collected sound: a 50 ms 1 kHz tone
    std::string bars = "[{\"notes\":[" + note(1, 0, 0) + "]}]";
    std::string loc = tinySong(120, 0, bars);
    loc.insert(loc.find("\"fx\":"), "\"samples\":{\"0.0\":\"tone\"},");
    aw::Engine e(48000);
    std::vector<float> tone(2400);
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.5f * (float)std::sin(2 * M_PI * 1000 * i / 48000.0);
    e.addSample("tone", tone.data(), tone.size(), 48000);
    CHECK(e.setLocation(loc, "{\"locks\":{}}"), "setLocation with samples");
    aw::Command a{aw::Command::Type::Audition, {}};
    a.a = 1;
    e.post(a);
    std::vector<float> L(4800), R(4800);
    e.render(4800, L.data(), R.data());
    // zero crossings in the first 40 ms show the sample's 1 kHz, not a kick's ~50 Hz
    int crossings = 0;
    for (int i = 1; i < 1920; ++i) crossings += (L[i - 1] < 0) != (L[i] < 0);
    CHECK(crossings > 60, "%d zero crossings", crossings);

    // a held bass note keeps sounding until released
    std::string save = awtest::readFile(savePath), farm, master;
    if (!awtest::locationText(save, 0, farm, master)) return;
    aw::Engine f(48000);
    f.setLocation(farm, master);
    aw::Command on{aw::Command::Type::LiveOn, {0, 0, 1, 0, false}};
    on.a = 7; on.b = 1; on.c = 7;
    f.post(on);
    std::vector<float> l(48000), r(48000);
    f.render(48000, l.data(), r.data());
    double held = 0;
    for (int i = 40000; i < 48000; ++i) held = std::fmax(held, std::fabs(l[i]));
    aw::Command off{aw::Command::Type::LiveOff, {}};
    off.a = 7;
    f.post(off);
    f.render(48000, l.data(), r.data());
    double after = 0;
    for (int i = 40000; i < 48000; ++i) after = std::fmax(after, std::fabs(l[i]));
    CHECK(held > 0.01 && after < held * 0.1, "held %g, after release %g", held, after);
}

static void testLiveEdits() {
    // a long bass note keeps sounding while its level is edited: the voice follows the new snapshot
    auto song = [](double level) {
        char locks[160];
        std::snprintf(locks, sizeof locks, "\"g.bpm\":%.10g,\"1.bass.lv\":%.4g,\"1.bass.su\":1,\"1.bass.at\":0", 70.0 / 150, level);
        std::string empty = "{\"locks\":{},\"bars\":[]}", bass = "{\"locks\":{},\"bars\":[{\"notes\":[" + note(5, 0, 7, 16) + "]}]}";
        return std::string("{\"skin\":\"farm\",\"name\":\"Farm\",\"songs\":[{\"name\":\"T\",\"style\":\"house\",\"hue\":0,"
                           "\"prog\":[0,0,0,0],\"locks\":{") + locks +
               "},\"mix\":{\"m\":[0,0,0,0],\"s\":[0,0,0,0]},\"sections\":[{\"name\":\"A\",\"hue\":0,\"type\":\"main\",\"locks\":{},\"lanes\":[" +
               empty + "," + bass + "," + empty + "," + empty + "]}],\"fx\":[[null,null,null],[null,null,null],[null,null,null],[null,null,null]]}]}";
    };
    std::string quiet = song(0.35);
    Rendered r = renderSong(song(0.7), "{\"locks\":{}}", {3, 0, 0, 0, true}, 1.0, 48000, [&](aw::Engine &e, double t) {
        static bool sent = false;
        if (t < 0.01) sent = false;
        if (!sent && t >= 0.5) { e.setLocation(quiet, "{\"locks\":{}}"); sent = true; }
    });
    auto rms = [&](double a, double b) {
        double s = 0;
        size_t i0 = (size_t)(a * 48000), i1 = (size_t)(b * 48000);
        for (size_t i = i0; i < i1; ++i) s += r.L[i] * r.L[i];
        return std::sqrt(s / (i1 - i0));
    };
    double before = rms(0.3, 0.5), after = rms(0.7, 0.9);
    // level is squared: half the fader is a quarter of the amplitude
    CHECK(before > 1e-3 && after / before > 0.18 && after / before < 0.33, "before %g after %g", before, after);

    // the same edit posted as just the one song
    size_t a = quiet.find("\"songs\":[") + 9, b = quiet.rfind("]}");
    std::string one = quiet.substr(a, b - a);
    r = renderSong(song(0.7), "{\"locks\":{}}", {3, 0, 0, 0, true}, 1.0, 48000, [&](aw::Engine &e, double t) {
        static bool sent = false;
        if (t < 0.01) sent = false;
        if (!sent && t >= 0.5) { CHECK(e.setSong(0, one), "setSong"); sent = true; }
    });
    double after2 = rms(0.7, 0.9);
    CHECK(std::fabs(after2 - after) < after * 0.05, "setSong %g, setLocation %g", after2, after);
}

int main(int argc, char **argv) {
    savePath = argc > 1 ? argv[1] : "../AWCore/Resources/DefaultState.json";
    testAutomation();
    testBiquad();
    testConvolver();
    testTiming();
    testSnapshotReuse();
    testDemoSong();
    testLevels();
    testLiveAndSamples();
    testLiveEdits();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
