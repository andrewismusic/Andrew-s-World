// Stereo convolution reverb with zero latency: the first 128 taps run directly, the rest in frequency-domain
// partitions that grow with distance into the impulse (128, 1024, 8192 samples). Each channel has its own
// impulse, like a ConvolverNode given a two-channel buffer. Kernels are prepared off the audio thread.
#pragma once

#include <memory>
#include <vector>

#include "FFT.hpp"

namespace aw {

struct ConvKernel {
    static constexpr int kHead = 128;
    static constexpr int kStages = 3;
    static constexpr int kBlock[kStages] = {128, 1024, 8192};
    static constexpr int kOffset[kStages] = {128, 2048, 16384};
    static constexpr int kEnd[kStages] = {2048, 16384, 1 << 30};

    size_t length = 0;
    float headL[kHead] = {}, headR[kHead] = {};
    struct Stage {
        int parts = 0;
        // parts × (block + 1) bins, left and right
        std::vector<cfloat> left, right;
    } stage[kStages];

    /// Applies ConvolverNode's normalisation (RMS power, calibrated to 44.1 kHz) as it builds.
    void build(const float *left, const float *right, size_t length, double sampleRate);

    static int maxParts(int stage, size_t maxLength);
};

class Convolver {
public:
    /// Allocates for impulses up to maxLength samples.
    void prepare(size_t maxLength);
    void setKernel(const ConvKernel *k) { kernel_ = k; }
    /// Adds the convolved block, scaled by gain, into outL/outR.
    void process(const float *inL, const float *inR, float *outL, float *outR, int n, float gain);
    void clear();

private:
    struct Stage {
        int block = 0, offset = 0, capacity = 0;
        FFT fft;
        std::vector<float> inL, inR; // 2·block window
        int fill = 0;
        std::vector<cfloat> fdlL, fdlR; // capacity × (block + 1) input spectra
        int head = 0;                   // newest spectrum
        std::vector<cfloat> work;
        std::vector<cfloat> accL, accR;
    };
    void runStage(Stage &s, int index);

    const ConvKernel *kernel_ = nullptr;
    Stage stages_[ConvKernel::kStages];
    float histL_[ConvKernel::kHead] = {}, histR_[ConvKernel::kHead] = {};
    int histPos_ = 0;
    std::vector<float> outL_, outR_; // future output ring
    size_t ringMask_ = 0, pos_ = 0;
};

} // namespace aw
