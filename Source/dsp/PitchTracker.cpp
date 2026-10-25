#include "PitchTracker.h"

#include <algorithm>
#include <cmath>

namespace abyss
{

void PitchTracker::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    const float fs = (float) sampleRate;
    hpf.set (35.0f, 0.707f, fs);
    lpf.set (260.0f, 0.707f, fs);
    atk = 1.0f - std::exp (-1.0f / (0.002f * fs));
    rel = 1.0f - std::exp (-1.0f / (0.060f * fs));
    atkSub = 1.0f - std::exp (-1.0f / (0.005f * fs));
    relSub = 1.0f - std::exp (-1.0f / (0.120f * fs));
    invalidSpan = 0.050 * sampleRate;
    reset();
}

void PitchTracker::reset() noexcept
{
    hpf.reset();
    lpf.reset();
    pos = 0.0;
    lastZc = prevZc = lastValidPos = 0.0;
    prevY = env = envSub = tAvg = 0.0f;
    armed = hasZc = hasPrevZc = ff1 = ff2 = valid = false;
    confidence = 0;
}

PitchFrame PitchTracker::process (float x) noexcept
{
    const float y = lpf.tick (hpf.tick (x, false), true);

    const float ax = std::abs (y);
    env += (ax > env ? atk : rel) * (ax - env);
    envSub += (ax > envSub ? atkSub : relSub) * (ax - envSub);
    if (env < 1.0e-20f) env = 0.0f;
    if (envSub < 1.0e-20f) envSub = 0.0f;

    if ((prevY < 0.0f && y >= 0.0f) || (prevY >= 0.0f && y < 0.0f))
    {
        const float den = y - prevY;
        float frac = den != 0.0f ? (-prevY / den) : 0.0f;
        frac = std::clamp (frac, 0.0f, 1.0f);
        lastZc = pos - 1.0 + (double) frac;
        hasZc = true;
    }

    const float thr = std::max (1.0e-5f, 0.12f * env);
    bool rising1 = false;
    bool rising2 = false;
    bool didTrigger = false;

    if (y < -thr)
        armed = true;

    if (armed && y > thr)
    {
        armed = false;
        didTrigger = true;

        if (hasZc && hasPrevZc)
        {
            const double period = lastZc - prevZc;
            if (period > 1.0)
            {
                const float freq = (float) (sampleRate / period);
                if (freq >= 30.0f && freq <= 600.0f)
                {
                    const float t = (float) period;
                    if (tAvg > 1.0f && std::abs (t - tAvg) / tAvg < 0.25f)
                    {
                        tAvg = 0.7f * tAvg + 0.3f * t;
                        confidence = std::min (confidence + 1, 4);
                    }
                    else
                    {
                        tAvg = t;
                        confidence = 1;
                    }
                    valid = true;
                    lastValidPos = pos;
                }
            }
        }

        if (hasZc)
        {
            prevZc = lastZc;
            hasPrevZc = true;
        }

        const bool prev = ff1;
        ff1 = ! ff1;
        if (! prev && ff1)
        {
            rising1 = true;
            const bool prev2 = ff2;
            ff2 = ! ff2;
            if (! prev2 && ff2)
                rising2 = true;
        }
    }

    if (valid && (pos - lastValidPos) > invalidSpan)
        valid = false;

    PitchFrame frame;
    frame.trackSig = y;
    frame.env = env;
    frame.envSub = envSub;
    frame.tAvg = tAvg;
    frame.valid = valid;
    frame.ff1 = ff1;
    frame.ff2 = ff2;
    frame.ff1Rising = rising1;
    frame.ff2Rising = rising2;
    frame.triggered = didTrigger;
    frame.confidence = confidence;

    prevY = y;
    pos += 1.0;
    return frame;
}

} // namespace abyss
