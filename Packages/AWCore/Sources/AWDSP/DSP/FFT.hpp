// A plain radix-2 complex FFT, portable (no Accelerate needed, so the engine also builds off Apple platforms).
#pragma once

#include <complex>
#include <vector>

namespace aw {

using cfloat = std::complex<float>;

class FFT {
public:
    void init(int n);
    int size() const { return n_; }
    /// In place. forward(): e^{-i…}; inverse() is unscaled (divide by n yourself).
    void forward(cfloat *x) const { run(x, false); }
    void inverse(cfloat *x) const { run(x, true); }

private:
    void run(cfloat *x, bool inverse) const;
    int n_ = 0;
    std::vector<cfloat> twiddle_;
    std::vector<int> bitrev_;
};

} // namespace aw
