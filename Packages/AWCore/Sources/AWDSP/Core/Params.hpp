// The prototype's parameter registry (P / SHORT), reduced to what playback needs: for each engine, the lock keys
// it reads and their defaults, in a fixed order so voices index them directly.
#pragma once

#include <cmath>
#include <string>
#include <vector>

namespace aw {

enum class EngineKind { Drums, Bass, Chords, Lead, Prism, None };

EngineKind engineNamed(const std::string &name);
/// The engines on a location's four tracks, from its skin.
void enginesForSkin(const std::string &skin, EngineKind out[4]);
const char *engineName(EngineKind e);

/// Synth parameters (bass, chords, lead share one voice). Keys an engine doesn't have keep their default.
namespace sp {
enum : int {
    wa, wb, tb, dt, mx, fm,
    h1, h2, h3, h4, h5, h6, h7, h8, ab, st, dm, oe, sh,
    oc, sb, nz, ch, spr, gl, mo,
    ft, cf, rs, fe, fd,
    at, dc, su, rl,
    lpr, lp, lcr, lc, lar, la,
    dr, cr, dl, rv, lv, pn, pr,
    count
};
}

/// Drum parameters: the voice's own, then the kit's.
namespace dp {
enum : int { tu, dc, tn, pu, lv, pr, spr, kdr, kcr, kdl, krv, klv, kpn, count };
}

struct ParamRef {
    int index;
    std::string key; // without the slot prefix, e.g. "bass.cf" or "drums.d2.sp"
    float def;
};

/// The keys an engine reads. For drums, one table per voice row (0–7).
const std::vector<ParamRef> &paramTable(EngineKind e, int drumRow = 0);
/// Defaults for every slot of the engine's block, including keys the engine doesn't have.
const std::vector<float> &paramDefaults(EngineKind e, int drumRow = 0);
int paramCount(EngineKind e);

/// Defaults for keys outside the engines.
float globalDefault(const std::string &key);

/// Drum decay maxima per voice, seconds.
extern const float kDrumMaxDecay[8];

/// Shared constants from the prototype.
extern const float kLfoBeats[10];
extern const float kDelayBeats[7];
extern const float kRatchetSteps[6];
extern const int kScales[8][12];
extern const int kScaleLength[8];
extern const int kChordDegrees[7][5];
extern const int kChordSize[7];

inline float hz(float v) { return 40.f * std::exp2(v * 9.f); }

} // namespace aw
