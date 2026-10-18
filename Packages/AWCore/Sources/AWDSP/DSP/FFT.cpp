#include "FFT.hpp"

#include <cmath>
#include <utility>

#include "Common.hpp"

namespace aw {

void FFT::init(int n) {
    n_ = n;
    twiddle_.resize(n / 2);
    for (int k = 0; k < n / 2; ++k) twiddle_[k] = std::polar(1.0f, (float)(-kTwoPi * k / n));
    bitrev_.resize(n);
    int bits = 0;
    while ((1 << bits) < n) ++bits;
    for (int i = 0; i < n; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        bitrev_[i] = r;
    }
}

void FFT::run(cfloat *x, bool inverse) const {
    const int n = n_;
    for (int i = 0; i < n; ++i)
        if (i < bitrev_[i]) std::swap(x[i], x[bitrev_[i]]);
    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1, stride = n / len;
        for (int i = 0; i < n; i += len)
            for (int k = 0; k < half; ++k) {
                cfloat w = twiddle_[k * stride];
                if (inverse) w = std::conj(w);
                cfloat a = x[i + k], b = x[i + k + half] * w;
                x[i + k] = a + b;
                x[i + k + half] = a - b;
            }
    }
}

} // namespace aw
