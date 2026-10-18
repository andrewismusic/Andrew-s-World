// A fixed-size single-producer, single-consumer queue. Lock-free, no allocation after construction.
#pragma once

#include <atomic>
#include <cstddef>

namespace aw {

template <typename T, size_t N> class SPSCQueue {
    static_assert((N & (N - 1)) == 0, "capacity must be a power of two");

public:
    bool push(const T &v) {
        size_t w = write_.load(std::memory_order_relaxed);
        if (w - read_.load(std::memory_order_acquire) == N) return false;
        items_[w & (N - 1)] = v;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(T &out) {
        size_t r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;
        out = items_[r & (N - 1)];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }

private:
    T items_[N];
    std::atomic<size_t> write_{0}, read_{0};
};

} // namespace aw
