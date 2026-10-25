#include "SubOscillator.h"

#include <algorithm>
#include <cmath>

namespace abyss
{

void SubOscillator::prepare (double sr) noexcept
{
    sampleRate = (float) (sr > 0.0 ? sr : 48000.0);
    reset();
}

void SubOscillator::reset() noexcept
{
    phi1 = phi2 = bias1 = bias2 = 0.0f;
    corr1 = corr2 = 0.0f;
    gritFc = -1.0f;
    coeffCount = 0;
    g1a.reset(); g1b.reset(); g2a.reset(); g2b.reset();
}

float SubOscillator::wrapHalf (float p) noexcept
{
    p -= std::floor (p + 0.5f);
    if (p >= 0.5f)
        p -= 1.0f;
    return p;
}

void SubOscillator::process (const PitchFrame& frame, float depth01, int mode,
                             float& clean1, float& clean2, float& grit1, float& grit2) noexcept
{
    (void) mode;

    if (frame.tAvg > 1.0f)
    {
        phi1 += 0.5f / frame.tAvg + bias1;
        phi2 += 0.25f / frame.tAvg + bias2;

        if (frame.ff1Rising)
        {
            const float e = wrapHalf (phi1);
            corr1 += 0.3f * e;
            bias1 = std::clamp (bias1 - 0.02f * e / frame.tAvg, -0.02f, 0.02f);
        }
        if (frame.ff2Rising)
        {
            const float e = wrapHalf (phi2);
            corr2 += 0.3f * e;
            bias2 = std::clamp (bias2 - 0.02f * e / frame.tAvg, -0.02f, 0.02f);
        }

        // Spread large corrections so a noisy tracker cannot step the waveform.
        constexpr float maxStep = 0.0015f;
        const float s1c = std::clamp (corr1, -maxStep, maxStep);
        const float s2c = std::clamp (corr2, -maxStep, maxStep);
        phi1 -= s1c;
        phi2 -= s2c;
        corr1 -= s1c;
        corr2 -= s2c;

        phi1 -= std::floor (phi1);
        phi2 -= std::floor (phi2);

        constexpr float twoPi = 6.28318530717958647692f;
        clean1 = fastSin (twoPi * phi1);
        clean2 = fastSin (twoPi * phi2);
    }
    else
    {
        clean1 = clean2 = 0.0f;
    }

    const float fc = lerp (420.0f, 180.0f, std::clamp (depth01, 0.0f, 1.0f));
    if ((coeffCount++ & 15) == 0 || fc != gritFc)
    {
        gritFc = fc;
        g1a.set (fc, 0.707f, sampleRate);
        g1b.set (fc, 0.707f, sampleRate);
        g2a.set (fc, 0.707f, sampleRate);
        g2b.set (fc, 0.707f, sampleRate);
    }

    const float sq1 = frame.ff1 ? 1.0f : -1.0f;
    const float sq2 = frame.ff2 ? 1.0f : -1.0f;
    float s1 = g1b.tick (g1a.tick (frame.trackSig * sq1, true), true);
    float s2 = g2b.tick (g2a.tick (frame.env * sq2, true), true);
    const float nrm = 1.0f / std::max (frame.env, 1.0e-4f);
    grit1 = std::clamp (s1 * nrm, -2.0f, 2.0f);
    grit2 = std::clamp (s2 * nrm, -2.0f, 2.0f);
}

} // namespace abyss
