#include "Convolver.hpp"

#include <algorithm>
#include <cmath>

namespace aw {

int ConvKernel::maxParts(int s, size_t maxLength) {
    if ((size_t)kOffset[s] >= maxLength) return 0;
    size_t end = std::min((size_t)kEnd[s], maxLength);
    return (int)((end - kOffset[s] + kBlock[s] - 1) / kBlock[s]);
}

void ConvKernel::build(const float *L, const float *R, size_t len, double sampleRate) {
    length = len;
    // ConvolverNode normalisation
    double power = 0;
    for (size_t i = 0; i < len; ++i) power += (double)L[i] * L[i] + (double)R[i] * R[i];
    power = std::sqrt(power / (2.0 * std::max<size_t>(len, 1)));
    if (!(power >= 0.000125)) power = 0.000125;
    double scale = 1 / power * 0.00125 * (44100 / sampleRate);

    for (int i = 0; i < kHead; ++i) {
        headL[i] = i < (int)len ? (float)(L[i] * scale) : 0;
        headR[i] = i < (int)len ? (float)(R[i] * scale) : 0;
    }
    for (int s = 0; s < kStages; ++s) {
        Stage &st = stage[s];
        int B = kBlock[s], N = 2 * B;
        st.parts = maxParts(s, len);
        st.left.assign((size_t)st.parts * (B + 1), {});
        st.right.assign((size_t)st.parts * (B + 1), {});
        FFT fft;
        fft.init(N);
        std::vector<cfloat> z(N);
        for (int p = 0; p < st.parts; ++p) {
            size_t base = kOffset[s] + (size_t)p * B;
            std::fill(z.begin(), z.end(), cfloat{});
            for (int i = 0; i < B && base + i < len && base + i < (size_t)kEnd[s]; ++i)
                z[i] = cfloat((float)(L[base + i] * scale), (float)(R[base + i] * scale));
            fft.forward(z.data());
            // unpack the two real spectra
            for (int k = 0; k <= B; ++k) {
                cfloat a = z[k], b = std::conj(z[(N - k) % N]);
                st.left[(size_t)p * (B + 1) + k] = (a + b) * 0.5f;
                st.right[(size_t)p * (B + 1) + k] = (a - b) * cfloat(0, -0.5f);
            }
        }
    }
}

void Convolver::prepare(size_t maxLength) {
    for (int s = 0; s < ConvKernel::kStages; ++s) {
        Stage &st = stages_[s];
        st.block = ConvKernel::kBlock[s];
        st.offset = ConvKernel::kOffset[s];
        st.capacity = std::max(1, ConvKernel::maxParts(s, maxLength));
        st.fft.init(2 * st.block);
        st.inL.assign(2 * st.block, 0);
        st.inR.assign(2 * st.block, 0);
        st.fdlL.assign((size_t)st.capacity * (st.block + 1), {});
        st.fdlR.assign((size_t)st.capacity * (st.block + 1), {});
        st.work.assign(2 * st.block, {});
        st.accL.assign(st.block + 1, {});
        st.accR.assign(st.block + 1, {});
        st.fill = 0;
        st.head = 0;
    }
    size_t ring = 1;
    while (ring < (size_t)(ConvKernel::kOffset[2] + ConvKernel::kBlock[2] + 4096)) ring <<= 1;
    outL_.assign(ring, 0);
    outR_.assign(ring, 0);
    ringMask_ = ring - 1;
    pos_ = 0;
}

void Convolver::clear() {
    for (auto &st : stages_) {
        std::fill(st.inL.begin(), st.inL.end(), 0.f);
        std::fill(st.inR.begin(), st.inR.end(), 0.f);
        std::fill(st.fdlL.begin(), st.fdlL.end(), cfloat{});
        std::fill(st.fdlR.begin(), st.fdlR.end(), cfloat{});
        st.fill = 0;
    }
    std::fill(outL_.begin(), outL_.end(), 0.f);
    std::fill(outR_.begin(), outR_.end(), 0.f);
    std::fill(histL_, histL_ + ConvKernel::kHead, 0.f);
    std::fill(histR_, histR_ + ConvKernel::kHead, 0.f);
}

void Convolver::runStage(Stage &st, int s) {
    const int B = st.block, N = 2 * B;
    // the window is [previous block, this block]; transform both channels at once
    for (int i = 0; i < N; ++i) st.work[i] = cfloat(st.inL[i], st.inR[i]);
    st.fft.forward(st.work.data());
    st.head = (st.head + 1) % st.capacity;
    cfloat *xl = &st.fdlL[(size_t)st.head * (B + 1)], *xr = &st.fdlR[(size_t)st.head * (B + 1)];
    for (int k = 0; k <= B; ++k) {
        cfloat a = st.work[k], b = std::conj(st.work[(N - k) % N]);
        xl[k] = (a + b) * 0.5f;
        xr[k] = (a - b) * cfloat(0, -0.5f);
    }
    // slide: this block becomes the previous one
    std::copy(st.inL.begin() + B, st.inL.end(), st.inL.begin());
    std::copy(st.inR.begin() + B, st.inR.end(), st.inR.begin());

    const ConvKernel::Stage &ks = kernel_->stage[s];
    int parts = std::min(ks.parts, st.capacity);
    if (parts == 0) return;
    std::fill(st.accL.begin(), st.accL.end(), cfloat{});
    std::fill(st.accR.begin(), st.accR.end(), cfloat{});
    for (int p = 0; p < parts; ++p) {
        int slot = ((st.head - p) % st.capacity + st.capacity) % st.capacity;
        const cfloat *fl = &st.fdlL[(size_t)slot * (B + 1)], *fr = &st.fdlR[(size_t)slot * (B + 1)];
        const cfloat *hl = &ks.left[(size_t)p * (B + 1)], *hr = &ks.right[(size_t)p * (B + 1)];
        for (int k = 0; k <= B; ++k) {
            st.accL[k] += fl[k] * hl[k];
            st.accR[k] += fr[k] * hr[k];
        }
    }
    // repack: W = YL + i·YR, Hermitian halves rebuilt
    for (int k = 0; k <= B; ++k) {
        st.work[k] = st.accL[k] + cfloat(0, 1) * st.accR[k];
        if (k > 0 && k < B) st.work[N - k] = std::conj(st.accL[k]) + cfloat(0, 1) * std::conj(st.accR[k]);
    }
    st.fft.inverse(st.work.data());
    // the last B samples are this block's output, due `offset − B` samples from now
    float inv = 1.0f / N;
    size_t at = pos_ + (size_t)(st.offset - B);
    for (int i = 0; i < B; ++i) {
        size_t r = (at + i) & ringMask_;
        outL_[r] += st.work[B + i].real() * inv;
        outR_[r] += st.work[B + i].imag() * inv;
    }
}

void Convolver::process(const float *inL, const float *inR, float *outL, float *outR, int n, float gain) {
    const ConvKernel *k = kernel_;
    for (int i = 0; i < n; ++i) {
        float xl = inL[i], xr = inR[i];
        // direct head
        histL_[histPos_] = xl;
        histR_[histPos_] = xr;
        float yl = 0, yr = 0;
        if (k) {
            int h = histPos_;
            for (int j = 0; j < ConvKernel::kHead; ++j) {
                yl += k->headL[j] * histL_[h];
                yr += k->headR[j] * histR_[h];
                h = h ? h - 1 : ConvKernel::kHead - 1;
            }
        }
        histPos_ = (histPos_ + 1) % ConvKernel::kHead;
        size_t r = pos_ & ringMask_;
        yl += outL_[r];
        yr += outR_[r];
        outL_[r] = outR_[r] = 0;
        outL[i] += yl * gain;
        outR[i] += yr * gain;
        ++pos_;
        for (int s = 0; s < ConvKernel::kStages; ++s) {
            Stage &st = stages_[s];
            st.inL[st.block + st.fill] = xl;
            st.inR[st.block + st.fill] = xr;
            if (++st.fill == st.block) {
                st.fill = 0;
                if (k) runStage(st, s);
                else {
                    std::copy(st.inL.begin() + st.block, st.inL.end(), st.inL.begin());
                    std::copy(st.inR.begin() + st.block, st.inR.end(), st.inR.begin());
                }
            }
        }
    }
}

} // namespace aw
