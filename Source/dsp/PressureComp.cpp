#include "PressureComp.h"

#include "FastMath.h"

#include <algorithm>
#include <cmath>

namespace abyss
{

void PressureComp::prepare (double sr) noexcept
{
    sampleRate = (float) (sr > 0.0 ? sr : 48000.0);
    coefRms = 1.0f - std::exp (-1.0f / (0.005f * sampleRate));
    rel = 1.0f - std::exp (-1.0f / (0.140f * sampleRate));
    pressure = -1.0f;
    reset();
}

void PressureComp::reset() noexcept
{
    ms = 0.0f;
    gain = 1.0f;
    pressure = -1.0f;
}

void PressureComp::rebuild (float p) noexcept
{
    const float thr = lerp (0.0f, -20.0f, p);
    const float ratio = std::max (1.0f, lerp (1.0f, 3.5f, p));
    const float slope = 1.0f / ratio - 1.0f;
    constexpr float knee = 8.0f;
    const float half = knee * 0.5f;

    for (int i = 0; i < 256; ++i)
    {
        const float db = -80.0f + 100.0f * ((float) i / 255.0f);
        float gr = 0.0f;
        if (db > thr + half)
            gr = slope * (db - thr);
        else if (db > thr - half)
        {
            const float t = db - thr + half;
            gr = slope * (t * t) / (2.0f * knee);
        }
        lut[i] = fastExp2 (gr * 0.16609640474f);
    }
}

void PressureComp::setPressure (float p) noexcept
{
    p = std::clamp (p, 0.0f, 1.0f);
    if (std::abs (p - pressure) < 1.0e-4f && pressure >= 0.0f)
        return;
    pressure = p;
    const float atkMs = lerp (15.0f, 6.0f, p);
    atk = 1.0f - std::exp (-1.0f / ((atkMs * 0.001f) * sampleRate));
    rebuild (p);
}

float PressureComp::process (float& l, float& r, int channels) noexcept
{
    const float e = channels > 1 ? 0.5f * (l * l + r * r) : (l * l);
    ms += coefRms * (e - ms);
    if (ms < 1.0e-20f)
        ms = 0.0f;

    const float db = 3.0102999566f * fastLog2 (std::max (ms, 1.0e-12f));
    float t = (db + 80.0f) * (255.0f / 100.0f);
    t = std::clamp (t, 0.0f, 255.0f);
    const int i0 = std::min ((int) t, 254);
    const float f = t - (float) i0;
    const float target = lut[i0] + f * (lut[i0 + 1] - lut[i0]);

    gain += (target < gain ? atk : rel) * (target - gain);
    l *= gain;
    if (channels > 1)
        r *= gain;
    return gain;
}

} // namespace abyss
