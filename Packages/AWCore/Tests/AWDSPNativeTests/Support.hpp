// Shared by the native test runner and the offline renderer.
#pragma once

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Core/Json.hpp"

namespace awtest {

inline std::string readFile(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/// The raw text of `worlds[location]` and `master` from a save, as the app would post them.
inline bool locationText(const std::string &save, int location, std::string &loc, std::string &master) {
    aw::json::Value v;
    if (!aw::json::parse(save, v)) return false;
    const aw::json::Value *w = v.get("worlds"), *m = v.get("master");
    if (!w || !m || location >= (int)w->array.size()) return false;
    const aw::json::Value &l = w->array[location];
    loc = save.substr(l.begin, l.end - l.begin);
    master = save.substr(m->begin, m->end - m->begin);
    return true;
}

inline void writeWav(const std::string &path, const std::vector<float> &L, const std::vector<float> &R, int sr) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    auto u32 = [&](uint32_t x) { std::fwrite(&x, 4, 1, f); };
    auto u16 = [&](uint16_t x) { std::fwrite(&x, 2, 1, f); };
    uint32_t n = (uint32_t)L.size(), bytes = n * 4;
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(2); u32(sr); u32(sr * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for (uint32_t i = 0; i < n; ++i) {
        for (float x : {L[i], R[i]}) {
            float c = x < -1 ? -1 : x > 1 ? 1 : x;
            int16_t s = (int16_t)(c * 32767);
            std::fwrite(&s, 2, 1, f);
        }
    }
    std::fclose(f);
}

} // namespace awtest
