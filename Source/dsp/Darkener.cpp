#include "Darkener.h"

#include <algorithm>
#include <cmath>

namespace abyss
{

void Darkener::prepare (double sr) noexcept
{
    sampleRate = (float) (sr > 0.0 ? sr : 48000.0);
    const float fs = sampleRate;
    hfAtk = 1.0f - std::exp (-1.0f / (0.001f * fs));
    hfRel = 1.0f - std::exp (-1.0f / (0.080f * fs));
    hf.set (2000.0f, 0.707f, fs);
    reset();
}

void Darkener::reset() noexcept
{
    lp1L.reset(); lp2L.reset(); lp1R.reset(); lp2R.reset(); hf.reset();
    shelfL.reset(); shelfR.reset();
    hfEnv = 0.0f;
    hold = 0;
    lastFc = lastQ = -1.0f;
    lastShelf = -100.0f;
}

void Darkener::process (float depth01, float inL, float inR, int channels,
                        float& outL, float& outR) noexcept
{
    const float d = std::clamp (depth01, 0.0f, 1.0f);
    const float mid = channels > 1 ? 0.5f * (inL + inR) : inL;
    const float hfSig = hf.tick (mid, false);
    const float ax = std::abs (hfSig);
    hfEnv += (ax > hfEnv ? hfAtk : hfRel) * (ax - hfEnv);
    if (hfEnv < 1.0e-20f)
        hfEnv = 0.0f;

    if ((hold++ & 15) == 0)
    {
        const float nyquist = 0.45f * sampleRate;
        const float shaped = std::pow (d, 0.85f);
        float fc = 20000.0f * std::pow (0.02f, shaped);
        const float envDbAmp = 6.020599913f * fastLog2 (std::max (hfEnv, 1.0e-20f));
        const float open = std::clamp ((envDbAmp + 36.0f) / 24.0f, 0.0f, 1.0f);
        fc *= std::exp2 (open * d);
        fc = std::clamp (fc, 20.0f, nyquist);

        const float q1 = lerp (0.707f, 1.05f, d);
        constexpr float q2 = 0.54f;
        if (fc != lastFc || q1 != lastQ)
        {
            lp1L.set (fc, q1, sampleRate);
            lp2L.set (fc, q2, sampleRate);
            lp1R.set (fc, q1, sampleRate);
            lp2R.set (fc, q2, sampleRate);
            lastFc = fc;
            lastQ = q1;
        }

        const float shelfDb = lerp (0.0f, 3.0f, d);
        if (shelfDb != lastShelf)
        {
            shelfL.setLowShelf (sampleRate, 110.0f, shelfDb);
            shelfR.setLowShelf (sampleRate, 110.0f, shelfDb);
            lastShelf = shelfDb;
        }
    }

    outL = shelfL.process (lp2L.tick (lp1L.tick (inL, true), true));
    if (channels > 1)
        outR = shelfR.process (lp2R.tick (lp1R.tick (inR, true), true));
    else
        outR = outL;
}

} // namespace abyss
