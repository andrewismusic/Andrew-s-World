#include "Snapshot.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "../DSP/Noise.hpp"
#include "Json.hpp"

namespace aw {

namespace {

/// JavaScript's Math.round: halves go up, also for negatives.
inline int jsRound(double x) { return (int)std::floor(x + 0.5); }

using LockTable = std::unordered_map<std::string, float>;

LockTable locksOf(const json::Value *node) {
    LockTable t;
    if (!node) return t;
    const json::Value *locks = node->get("locks");
    if (!locks || !locks->isObject()) return t;
    t.reserve(locks->object.size());
    // a null lock is a NaN written by the prototype; JavaScript arithmetic reads it as 0
    for (auto &kv : locks->object) t[kv.first] = (float)kv.second.asNumber(0);
    return t;
}

/// rv(): the deepest lock wins, else the default.
struct Chain {
    const LockTable *nodes[5];
    int count = 0;
    void push(const LockTable *t) { nodes[count++] = t; }
    bool find(const std::string &key, float &out) const {
        for (int i = count - 1; i >= 0; --i) {
            auto it = nodes[i]->find(key);
            if (it != nodes[i]->end()) {
                out = it->second;
                return true;
            }
        }
        return false;
    }
    float value(const std::string &key, float def) const {
        float v;
        return find(key, v) ? v : def;
    }
    float global(const std::string &key) const { return value(key, globalDefault(key)); }
};

Music musicAt(const Chain &c) {
    Music m;
    m.bpm = 50 + jsRound(c.global("g.bpm") * 150);
    m.swing = c.global("g.sw");
    m.key = jsRound(c.global("g.key") * 11);
    m.scale = std::clamp(jsRound(c.global("g.sc") * 7), 0, 7);
    const int DEN_V[3] = {16, 8, 4};
    m.den = DEN_V[std::clamp(jsRound(c.global("g.td") * 2), 0, 2)];
    m.ts = std::min(2 + jsRound(c.global("g.ts") * 14), std::min(16, m.den * 2));
    m.tpb = 48 / m.den;
    return m;
}

bool flagOn(const json::Value &v) { return v.truthy(); }

} // namespace

void makeImpulse(ReverbIR &ir, float size, float damp, double sampleRate) {
    ir.size = size;
    ir.damp = damp;
    ir.sampleRate = sampleRate;
    size_t len = (size_t)std::floor(std::min(4.5, .3 + size * size * 7.7) * sampleRate);
    RNG rng(0x5EED1234u);
    for (int ch = 0; ch < 2; ++ch) {
        auto &d = ch ? ir.right : ir.left;
        d.assign(len, 0.f);
        double y = 0;
        for (size_t i = 0; i < len; ++i) {
            double f = (double)i / len, a = 1 - damp * .92 * std::min(1.0, f * 3);
            y += (rng.bipolar() - y) * std::max(.04, a);
            d[i] = (float)(y * std::pow(1 - f, 2.2));
        }
    }
}

std::shared_ptr<const ReverbIR> SnapshotBuilder::reverbFor(float size, float damp) {
    // the prototype rebuilds its impulse only when size or damp change at two decimals
    float qs = jsRound(size * 100) / 100.f, qd = jsRound(damp * 100) / 100.f;
    for (auto &ir : irs_)
        if (ir->size == qs && ir->damp == qd) return ir;
    auto ir = std::make_shared<ReverbIR>();
    makeImpulse(*ir, size, damp, sampleRate_);
    ir->kernel.build(ir->left.data(), ir->right.data(), ir->left.size(), sampleRate_);
    ir->size = qs;
    ir->damp = qd;
    if (irs_.size() >= 8) irs_.erase(irs_.begin());
    irs_.push_back(ir);
    return ir;
}

std::shared_ptr<const SongSnap> SnapshotBuilder::buildSong(const json::Value &song, const json::Value &master,
                                                           const EngineKind engines[4]) {
    auto out = std::make_shared<SongSnap>();
    LockTable masterLocks = locksOf(&master), songLocks = locksOf(&song);

    // key → block index for each slot's engine (drums: per row)
    struct SlotKeys {
        std::unordered_map<std::string, int> index[8];
    } keys[4];
    for (int s = 0; s < 4; ++s) {
        EngineKind e = engines[s];
        if (e == EngineKind::None || e == EngineKind::Prism) continue;
        int rows = e == EngineKind::Drums ? 8 : 1;
        for (int r = 0; r < rows; ++r)
            for (auto &p : paramTable(e, r)) keys[s].index[r][std::to_string(s) + "." + p.key] = p.index;
    }

    if (const json::Value *mix = song.get("mix")) {
        const json::Value *m = mix->get("m"), *so = mix->get("s");
        bool anySolo = false;
        if (so && so->isArray())
            for (auto &x : so->array) anySolo |= flagOn(x);
        for (int s = 0; s < 4; ++s) {
            if (anySolo) out->audible[s] = so && s < (int)so->array.size() && flagOn(so->array[s]);
            else out->audible[s] = !(m && m->isArray() && s < (int)m->array.size() && flagOn(m->array[s]));
        }
    }

    if (const json::Value *fx = song.get("fx"); fx && fx->isArray())
        for (int s = 0; s < 4 && s < (int)fx->array.size(); ++s) {
            const json::Value &row = fx->array[s];
            for (int i = 0; i < 3 && row.isArray() && i < (int)row.array.size(); ++i)
                if (row.array[i].isString()) out->fx[s][i] = buildingNamed(row.array[i].string);
        }

    {
        auto g = [&](const char *k) {
            auto it = songLocks.find(k);
            return it != songLocks.end() ? it->second : globalDefault(k);
        };
        out->space = {g("space.sz"), g("space.dp"), g("space.dt"), g("space.fb"), g("space.tn")};
        Chain c;
        c.push(&masterLocks);
        c.push(&songLocks);
        out->master = {c.global("master.vol"), c.global("master.glue"), c.global("master.sat")};
        out->reverb = reverbFor(out->space.size, out->space.damp);
    }

    const json::Value *parts = song.get("sections");
    if (!parts || !parts->isArray()) return out;
    for (auto &part : parts->array) {
        PartSnap ps;
        LockTable partLocks = locksOf(&part);
        Chain pc;
        pc.push(&masterLocks);
        pc.push(&songLocks);
        pc.push(&partLocks);
        ps.music = musicAt(pc);
        const json::Value *lanes = part.get("lanes");
        for (int s = 0; s < 4 && lanes && lanes->isArray() && s < (int)lanes->array.size(); ++s) {
            const json::Value &lane = lanes->array[s];
            LockTable laneLocks = locksOf(&lane);
            Chain lc = pc;
            lc.push(&laneLocks);
            EngineKind e = engines[s];
            bool plays = e != EngineKind::None && e != EngineKind::Prism;
            int rows = e == EngineKind::Drums ? 8 : 1;
            // the lane's resolved block per row; notes copy it and apply their own locks
            std::vector<std::vector<float>> laneBlock(rows);
            if (plays)
                for (int r = 0; r < rows; ++r) {
                    laneBlock[r] = paramDefaults(e, r);
                    for (auto &p : paramTable(e, r)) laneBlock[r][p.index] = lc.value(std::to_string(s) + "." + p.key, p.def);
                }
            float laneRatchet = lc.global("rat.rt");
            for (int i = 0; i < 3; ++i) {
                BuildingType bt = out->fx[s][i];
                if (bt == BuildingType::None) continue;
                std::string pre = std::to_string(s) + ".fx" + std::to_string(i) + "." + buildingKey(bt) + ".";
                for (int q = 0; q < 3; ++q) ps.fx[s][i][q] = lc.value(pre + kBuildingParams[(int)bt][q], buildingDefault(bt, q));
            }
            const json::Value *bars = lane.get("bars");
            if (!bars || !bars->isArray()) continue;
            ps.length = std::max(ps.length, (int)bars->array.size());
            for (auto &bar : bars->array) {
                BarSnap bs;
                const json::Value *trip = bar.get("trip");
                bs.triplet = trip && trip->truthy();
                bs.first = (uint32_t)out->notes.size();
                const json::Value *notes = bar.get("notes");
                if (plays && notes && notes->isArray())
                    for (auto &n : notes->array) {
                        NoteEvent ev{};
                        ev.id = (int32_t)n.num("id", 0);
                        ev.step = (int16_t)n.num("step", 0);
                        ev.pitch = (int16_t)n.num("pitch", 0);
                        ev.len = (int16_t)std::max(1.0, n.num("len", 1));
                        int row = e == EngineKind::Drums ? std::clamp((int)ev.pitch, 0, 7) : 0;
                        auto &index = keys[s].index[row];
                        ev.params = (uint32_t)out->params.size();
                        out->params.insert(out->params.end(), laneBlock[row].begin(), laneBlock[row].end());
                        float ratchet = laneRatchet;
                        if (const json::Value *nl = n.get("locks"); nl && nl->isObject())
                            for (auto &kv : nl->object) {
                                if (kv.first == "rat.rt") { ratchet = (float)kv.second.asNumber(0); continue; }
                                auto it = index.find(kv.first);
                                if (it != index.end()) out->params[ev.params + it->second] = (float)kv.second.asNumber(0);
                            }
                        ev.ratchet = (uint8_t)std::clamp(jsRound(ratchet * 5), 0, 5);
                        ev.rampMode = RampMode::None;
                        ev.ramp = -1;
                        if (const json::Value *rm = n.get("rm"); rm && rm->isString())
                            ev.rampMode = rm->string == "away" ? RampMode::Away : rm->string == "toward" ? RampMode::Toward : RampMode::None;
                        const json::Value *ramp = n.get("ramp");
                        if (ev.rampMode != RampMode::None && ramp && ramp->isObject() && !ramp->object.empty()) {
                            ev.ramp = (int32_t)out->params.size();
                            out->params.insert(out->params.end(), paramCount(e), NAN);
                            for (auto &kv : ramp->object) {
                                auto it = index.find(kv.first);
                                // probability never ramps
                                int pr = e == EngineKind::Drums ? (int)dp::pr : (int)sp::pr;
                                if (it != index.end() && it->second != pr) out->params[ev.ramp + it->second] = (float)kv.second.asNumber(0);
                            }
                        }
                        out->notes.push_back(ev);
                    }
                bs.count = (uint32_t)out->notes.size() - bs.first;
                std::stable_sort(out->notes.begin() + bs.first, out->notes.end(),
                                 [](const NoteEvent &a, const NoteEvent &b) { return a.step < b.step; });
                ps.lanes[s].bars.push_back(bs);
            }
        }
        out->parts.push_back(std::move(ps));
    }
    return out;
}

std::unique_ptr<LocationSnap> SnapshotBuilder::build(const std::string &locationJSON, const std::string &masterJSON) {
    json::Value loc, master;
    if (!json::parse(locationJSON, loc) || !loc.isObject()) return nullptr;
    if (!json::parse(masterJSON, master) || !master.isObject()) return nullptr;
    auto snap = std::make_unique<LocationSnap>();
    if (const json::Value *skin = loc.get("skin"); skin && skin->isString()) snap->skin = skin->string;
    enginesForSkin(snap->skin, snap->engines);
    const json::Value *songs = loc.get("songs");
    if (!songs || !songs->isArray()) return nullptr;

    std::vector<Cached> next;
    for (auto &song : songs->array) {
        std::string key = snap->skin + "\n" + masterJSON + "\n" + locationJSON.substr(song.begin, song.end - song.begin);
        std::shared_ptr<const SongSnap> built;
        for (auto &c : cache_)
            if (c.key == key) { built = c.song; break; }
        if (!built) built = buildSong(song, master, snap->engines);
        snap->songs.push_back(built);
        next.push_back({std::move(key), built});
    }
    cache_ = std::move(next);
    return snap;
}

} // namespace aw
