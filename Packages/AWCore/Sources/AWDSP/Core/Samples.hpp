// Audio files the player has collected (the backpack's sounds), loaded into the engine by id.
// Samples are never freed while the engine lives, so a voice can keep playing one across snapshot changes.
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace aw {

struct Sample {
    std::string id;
    std::vector<float> data; // mono
    double rate = 48000;
};

class SampleBank {
public:
    void add(const std::string &id, const float *data, size_t frames, double rate) {
        auto s = std::make_unique<Sample>();
        s->id = id;
        s->data.assign(data, data + frames);
        s->rate = rate;
        // a re-added id replaces the old one for new snapshots; the old data stays alive for voices still using it
        for (auto &e : live_)
            if (e && e->id == id) { retired_.push_back(std::move(e)); e = nullptr; }
        live_.push_back(std::move(s));
        ++generation_;
    }
    const Sample *find(const std::string &id) const {
        for (auto &e : live_)
            if (e && e->id == id) return e.get();
        return nullptr;
    }
    int generation() const { return generation_; }

private:
    std::vector<std::unique_ptr<Sample>> live_, retired_;
    int generation_ = 0;
};

} // namespace aw
