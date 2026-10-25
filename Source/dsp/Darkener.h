#pragma once

#include "FastMath.h"

namespace abyss
{

class Darkener
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // depth01 > 0. Processes one stereo (or mono) sample. Returns filtered main.
    void process (float depth01, float inL, float inR, int channels,
                  float& outL, float& outR) noexcept;

private:
    float sampleRate = 48000.0f;
    int hold = 0;
    float lastFc = -1.0f;
    float lastQ = -1.0f;
    float lastShelf = -100.0f;
    float hfEnv = 0.0f;
    float hfAtk = 0.0f, hfRel = 0.0f;

    struct TptSvf lp1L, lp2L, lp1R, lp2R, hf;
    struct Biquad shelfL, shelfR;
};

} // namespace abyss
