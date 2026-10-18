// aw-render: renders a song from a save to a WAV file with the native engine.
//   aw-render <save.json> <out.wav> [--location 0] [--song 0] [--level 0] [--part 0] [--bar 0] [--seconds 20]
//             [--rate 48000] [--notes]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Core/Engine.hpp"
#include "Support.hpp"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: aw-render save.json out.wav [--location N] [--song N] [--level N] [--part N] [--bar N] [--seconds S] [--rate R] [--solo N] [--notes] [--loop]\n");
        return 2;
    }
    int location = 0, song = 0, level = 0, part = 0, bar = 0, rate = 48000, solo = -1;
    double seconds = 20;
    bool notes = false, loop = false;
    for (int i = 3; i < argc; ++i) {
        auto arg = [&](const char *name) { return !std::strcmp(argv[i], name) && i + 1 < argc; };
        if (arg("--location")) location = std::atoi(argv[++i]);
        else if (arg("--song")) song = std::atoi(argv[++i]);
        else if (arg("--level")) level = std::atoi(argv[++i]);
        else if (arg("--part")) part = std::atoi(argv[++i]);
        else if (arg("--bar")) bar = std::atoi(argv[++i]);
        else if (arg("--seconds")) seconds = std::atof(argv[++i]);
        else if (arg("--rate")) rate = std::atoi(argv[++i]);
        else if (arg("--solo")) solo = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--notes")) notes = true;
        else if (!std::strcmp(argv[i], "--loop")) loop = true;
    }
    std::string save = awtest::readFile(argv[1]), loc, master;
    if (!awtest::locationText(save, location, loc, master)) {
        std::fprintf(stderr, "couldn't read %s\n", argv[1]);
        return 1;
    }
    if (solo >= 0) {
        // solo one track of the song: rewrite its mix (the demo saves all start unmuted)
        size_t at = std::string::npos, from = 0;
        for (int k = 0; k <= song; ++k) {
            at = loc.find("\"s\":[0,0,0,0]", from);
            if (at == std::string::npos) break;
            from = at + 1;
        }
        if (at != std::string::npos) {
            std::string s = "\"s\":[0,0,0,0]";
            s[5 + solo * 2] = '1';
            loc.replace(at, s.size(), s);
        }
    }
    aw::Engine engine(rate);
    std::vector<aw::StartedNote> log;
    engine.log = &log;
    if (!engine.setLocation(loc, master)) {
        std::fprintf(stderr, "couldn't build the location\n");
        return 1;
    }
    aw::Command play{aw::Command::Type::Play, {level, song, part, bar, loop}};
    engine.post(play);
    size_t total = (size_t)(seconds * rate);
    std::vector<float> L(total), R(total);
    const int block = 256;
    for (size_t i = 0; i < total; i += block) {
        int n = (int)std::min<size_t>(block, total - i);
        engine.render(n, &L[i], &R[i]);
    }
    double peak = 0, sum = 0;
    for (size_t i = 0; i < total; ++i) {
        peak = std::fmax(peak, std::fmax(std::fabs(L[i]), std::fabs(R[i])));
        sum += L[i] * L[i] + R[i] * R[i];
    }
    std::printf("%zu frames, peak %.3f (%.1f dBFS), rms %.1f dBFS, %zu notes\n", total, peak, 20 * std::log10(peak + 1e-12),
                10 * std::log10(sum / (2.0 * total) + 1e-20), log.size());
    if (notes)
        for (auto &n : log) std::printf("%9.4f  slot %d  pitch %2d  id %d\n", n.time, n.slot, n.pitch, n.id);
    awtest::writeWav(argv[2], L, R, rate);
    return 0;
}
