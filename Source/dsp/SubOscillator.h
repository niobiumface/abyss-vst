#pragma once

#include "FastMath.h"
#include "PitchTracker.h"

namespace abyss
{

class SubOscillator
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // mode 0 = Clean, 1 = Grit. Writes raw oscillator outputs (before envSub/gate).
    void process (const PitchFrame& frame, float depth01, int mode,
                  float& clean1, float& clean2, float& grit1, float& grit2) noexcept;

private:
    float sampleRate = 48000.0f;
    float phi1 = 0.0f, phi2 = 0.0f;
    float bias1 = 0.0f, bias2 = 0.0f;
    float corr1 = 0.0f, corr2 = 0.0f;
    int coeffCount = 0;
    float gritFc = -1.0f;

    struct TptSvf g1a, g1b, g2a, g2b;

    static float wrapHalf (float p) noexcept;
};

} // namespace abyss
