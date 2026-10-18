#include "Params.hpp"

#include <cmath>
#include <map>

namespace aw {

const float kDrumMaxDecay[8] = {1.5f, .4f, .4f, .05f, .4f, 2.f, .4f, .3f};
const float kLfoBeats[10] = {16, 8, 4, 2, 1, 2.f / 3, .5f, 1.f / 3, .25f, .125f};
const float kDelayBeats[7] = {.25f, 1.f / 3, .5f, .75f, 1, 1.5f, 2};
const float kRatchetSteps[6] = {0, 1, 2.f / 3, .5f, 1.f / 3, .25f};
// SCALE_ORDER: major, minor, dorian, phrygian, lydian, mixolydian, harmonic, chromatic
const int kScales[8][12] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10},
    {0, 2, 4, 6, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
};
const int kScaleLength[8] = {7, 7, 7, 7, 7, 7, 7, 12};
const int kChordDegrees[7][5] = {{0, 2, 4}, {0, 2, 4, 6}, {0, 2, 4, 6, 8}, {0, 1, 4}, {0, 3, 4}, {0, 4, 7}, {0, 7}};
const int kChordSize[7] = {3, 4, 5, 3, 3, 3, 2};

EngineKind engineNamed(const std::string &n) {
    if (n == "drums") return EngineKind::Drums;
    if (n == "bass") return EngineKind::Bass;
    if (n == "chords") return EngineKind::Chords;
    if (n == "lead") return EngineKind::Lead;
    if (n == "prism") return EngineKind::Prism;
    return EngineKind::None;
}

const char *engineName(EngineKind e) {
    switch (e) {
    case EngineKind::Drums: return "drums";
    case EngineKind::Bass: return "bass";
    case EngineKind::Chords: return "chords";
    case EngineKind::Lead: return "lead";
    case EngineKind::Prism: return "prism";
    case EngineKind::None: return "";
    }
    return "";
}

void enginesForSkin(const std::string &skin, EngineKind out[4]) {
    out[0] = EngineKind::Drums;
    out[1] = EngineKind::Bass;
    out[2] = EngineKind::Chords;
    out[3] = (skin == "classic" || skin == "solucan") ? EngineKind::Prism : EngineKind::Lead;
}

namespace {

constexpr float LF(int i) { return i / 9.f; }

struct Tables {
    std::vector<ParamRef> synth[3]; // bass, chords, lead
    std::vector<float> synthDef[3];
    std::vector<ParamRef> drums[8];
    std::vector<float> drumDef[8];
    std::map<std::string, float> globals;

    Tables() {
        const char *names[3] = {"bass", "chords", "lead"};
        // union defaults; keys an engine lacks stay here (ab = 0 keeps the additive part silent, etc.)
        std::vector<float> base(sp::count, 0.f);
        auto def = [&](int i, float v) { base[i] = v; };
        def(sp::wa, 2.f / 3); def(sp::wb, 1); def(sp::tb, 12.f / 36); def(sp::dt, .2f); def(sp::mx, .3f); def(sp::fm, 0);
        const float HDEF[8] = {1, .5f, .33f, .25f, .2f, .16f, .14f, .12f};
        for (int n = 0; n < 8; ++n) def(sp::h1 + n, HDEF[n]);
        def(sp::ab, 0); def(sp::st, .5f); def(sp::dm, .4f); def(sp::oe, .5f); def(sp::sh, .1f);
        def(sp::oc, .5f); def(sp::sb, 0); def(sp::nz, 0); def(sp::ch, 0); def(sp::spr, 0); def(sp::gl, 0); def(sp::mo, 0);
        def(sp::ft, 0); def(sp::cf, .62f); def(sp::rs, .2f); def(sp::fe, .3f); def(sp::fd, .45f);
        def(sp::at, .15f); def(sp::dc, .5f); def(sp::su, .5f); def(sp::rl, .35f);
        def(sp::lpr, LF(4)); def(sp::lp, 0); def(sp::lcr, LF(4)); def(sp::lc, 0); def(sp::lar, LF(6)); def(sp::la, 0);
        def(sp::dr, 0); def(sp::cr, 0); def(sp::dl, 0); def(sp::rv, .15f); def(sp::lv, .7f); def(sp::pn, .5f); def(sp::pr, 1);

        for (int e = 0; e < 3; ++e) {
            std::string pre = std::string(names[e]) + ".";
            auto &t = synth[e];
            auto add = [&](int i, const char *k) { t.push_back({i, pre + k, base[i]}); };
            add(sp::wa, "wa"); add(sp::wb, "wb"); add(sp::tb, "tb"); add(sp::dt, "dt"); add(sp::mx, "mx"); add(sp::fm, "fm");
            if (e == 2) {
                const char *h[8] = {"h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8"};
                for (int n = 0; n < 8; ++n) add(sp::h1 + n, h[n]);
                add(sp::ab, "ab"); add(sp::st, "st"); add(sp::dm, "dm"); add(sp::oe, "oe"); add(sp::sh, "sh");
            }
            add(sp::oc, "oc"); add(sp::sb, "sb"); add(sp::nz, "nz");
            if (e == 1) { add(sp::ch, "ch"); add(sp::spr, "sp"); }
            add(sp::gl, "gl");
            if (e == 2) add(sp::mo, "mo");
            add(sp::ft, "ft"); add(sp::cf, "cf"); add(sp::rs, "rs"); add(sp::fe, "fe"); add(sp::fd, "fd");
            add(sp::at, "at"); add(sp::dc, "dc"); add(sp::su, "su"); add(sp::rl, "rl");
            add(sp::lpr, "lpr"); add(sp::lp, "lp"); add(sp::lcr, "lcr"); add(sp::lc, "lc"); add(sp::lar, "lar"); add(sp::la, "la");
            add(sp::dr, "dr"); add(sp::cr, "cr"); add(sp::dl, "dl"); add(sp::rv, "rv"); add(sp::lv, "lv"); add(sp::pn, "pn"); add(sp::pr, "pr");
            synthDef[e] = base;
        }

        const float DRUM_DEF[8][5] = {{.5f, .62f, .75f, .5f, .85f}, {.5f, .6f, .78f, .55f, .72f}, {.5f, .62f, .64f, .5f, .68f},
                                      {.5f, .55f, .9f, .6f, .55f},  {.5f, .35f, .9f, .3f, .5f},   {.5f, .55f, .88f, .3f, .45f},
                                      {.5f, .7f, .7f, .4f, .66f},   {.5f, .6f, .82f, .4f, .52f}};
        for (int v = 0; v < 8; ++v) {
            std::string pre = "drums.d" + std::to_string(v) + ".";
            auto &t = drums[v];
            std::vector<float> d(dp::count, 0.f);
            const float *dd = DRUM_DEF[v];
            float vals[dp::count] = {dd[0], dd[1], dd[2], dd[3], dd[4], 1, .45f, 0, 0, 0, .1f, .8f, .5f};
            for (int i = 0; i < dp::count; ++i) d[i] = vals[i];
            t.push_back({dp::tu, pre + "tu", d[dp::tu]});
            t.push_back({dp::dc, pre + "dc", d[dp::dc]});
            t.push_back({dp::tn, pre + "tn", d[dp::tn]});
            t.push_back({dp::pu, pre + "pu", d[dp::pu]});
            t.push_back({dp::lv, pre + "lv", d[dp::lv]});
            t.push_back({dp::pr, pre + "pr", d[dp::pr]});
            if (v == 2) t.push_back({dp::spr, pre + "sp", d[dp::spr]});
            t.push_back({dp::kdr, "drums.kit.dr", d[dp::kdr]});
            t.push_back({dp::kcr, "drums.kit.cr", d[dp::kcr]});
            t.push_back({dp::kdl, "drums.kit.dl", d[dp::kdl]});
            t.push_back({dp::krv, "drums.kit.rv", d[dp::krv]});
            t.push_back({dp::klv, "drums.kit.lv", d[dp::klv]});
            t.push_back({dp::kpn, "drums.kit.pn", d[dp::kpn]});
            drumDef[v] = d;
        }

        globals = {
            {"space.sz", .4f}, {"space.dp", .5f}, {"space.dt", 3.f / 6}, {"space.fb", .35f}, {"space.tn", .6f},
            {"master.vol", .8f}, {"master.glue", .35f}, {"master.sat", .1f},
            {"rat.rt", 0},
            {"g.bpm", 70.f / 150}, {"g.sw", 0}, {"g.key", 0}, {"g.sc", 1.f / 7}, {"g.ts", 2.f / 14}, {"g.td", 1},
        };
    }
};

const Tables &tables() {
    static const Tables t;
    return t;
}

int synthIndex(EngineKind e) { return e == EngineKind::Bass ? 0 : e == EngineKind::Chords ? 1 : 2; }

} // namespace

const std::vector<ParamRef> &paramTable(EngineKind e, int drumRow) {
    if (e == EngineKind::Drums) return tables().drums[drumRow < 0 ? 0 : drumRow > 7 ? 7 : drumRow];
    return tables().synth[synthIndex(e)];
}

const std::vector<float> &paramDefaults(EngineKind e, int drumRow) {
    if (e == EngineKind::Drums) return tables().drumDef[drumRow < 0 ? 0 : drumRow > 7 ? 7 : drumRow];
    return tables().synthDef[synthIndex(e)];
}

int paramCount(EngineKind e) { return e == EngineKind::Drums ? (int)dp::count : (int)sp::count; }

float globalDefault(const std::string &key) {
    auto &g = tables().globals;
    auto it = g.find(key);
    return it == g.end() ? 0.f : it->second;
}

} // namespace aw
