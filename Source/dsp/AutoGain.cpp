#include "AutoGain.h"

#include "FastMath.h"

#include <algorithm>
#include <cmath>

namespace abyss
{

void AutoGain::prepare (double sr) noexcept
{
    sampleRate = (float) (sr > 0.0 ? sr : 48000.0);
    coefK = 1.0f - std::exp (-1.0f / (0.200f * sampleRate));
    reset();
}

void AutoGain::reset() noexcept
{
    rmsIn = rmsProc = 1.0e-6f;
    k = target = 1.0f;
}

void AutoGain::updateTargets (float meanSquareProcessed, float meanSquareInput,
                              float blockSeconds, bool enabled, bool forceUnity) noexcept
{
    if (forceUnity || ! enabled)
    {
        target = 1.0f;
        if (forceUnity)
            k = 1.0f;
        return;
    }

    const float coef = 1.0f - std::exp (-std::max (blockSeconds, 1.0e-5f) / 0.300f);
    rmsIn += coef * (meanSquareInput - rmsIn);
    rmsProc += coef * (meanSquareProcessed - rmsProc);
    if (rmsIn < 1.0e-20f) rmsIn = 0.0f;
    if (rmsProc < 1.0e-20f) rmsProc = 0.0f;

    // −60 dBFS amplitude → mean square 1e-12.
    if (rmsIn < 1.0e-12f)
        return;

    const float ratio = std::sqrt (rmsIn / std::max (rmsProc, 1.0e-12f));
    target = std::clamp (ratio, dbToGain (-12.0f), dbToGain (12.0f));
}

float AutoGain::next (int steps) noexcept
{
    for (int i = 0; i < steps; ++i)
        k += coefK * (target - k);
    if (std::abs (k - target) < 1.0e-6f)
        k = target;
    if (std::abs (k - 1.0f) < 1.0e-7f && target == 1.0f)
        k = 1.0f;
    return k;
}

} // namespace abyss
