#include "PressureDrive.h"

#include "FastMath.h"

#include <algorithm>
#include <cmath>

namespace abyss
{

void PressureDrive::prepare (double sr) noexcept
{
    sampleRate = (float) (sr > 0.0 ? sr : 48000.0);
    r = std::exp (-2.0f * 3.14159265358979323846f * 8.0f / sampleRate);
    reset();
}

void PressureDrive::reset() noexcept
{
    x1[0] = x1[1] = y1[0] = y1[1] = 0.0f;
    lastP = -1.0f;
}

float PressureDrive::process (float x, float pressure01, int channel) noexcept
{
    const float p = std::clamp (pressure01, 0.0f, 1.0f);
    if (p != lastP)
    {
        lastP = p;
        const float preDb = 18.0f * std::pow (p, 1.3f);
        pre = dbToGain (preDb);
        b = 0.2f * p;
        bias = fastTanh (b);
        norm = 1.0f / std::sqrt (std::max (pre, 1.0e-6f));
        w = smoothstep (0.0f, 0.15f, p);
    }

    const int ch = channel & 1;
    const float sat = (fastTanh (pre * x + b) - bias) * norm;
    const float y = x + w * (sat - x);
    const float dc = y - x1[ch] + r * y1[ch];
    x1[ch] = y;
    y1[ch] = dc;
    return dc;
}

} // namespace abyss
