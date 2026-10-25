#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace abyss
{

inline float lerp (float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

inline float smoothstep (float edge0, float edge1, float x) noexcept
{
    const float t = std::clamp ((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float dbToGain (float db) noexcept
{
    if (db == 0.0f)
        return 1.0f;
    return std::exp (db * 0.11512925464970228420089957273422f);
}

inline float gainToDb (float gain) noexcept
{
    return 20.0f * std::log10 (std::max (gain, 1.0e-12f));
}

// Padé [7/6] for tanh, clamped to ±3. Monotonic on that interval.
inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return num / den;
}

// 2048-entry sine LUT with linear interpolation. Argument is radians.
inline float fastSin (float radians) noexcept
{
    static const auto table = [] {
        constexpr int n = 2048;
        std::array<float, n + 1> t {};
        for (int i = 0; i <= n; ++i)
            t[(size_t) i] = (float) std::sin (6.28318530717958647692 * (double) i / (double) n);
        return t;
    }();

    constexpr float kInvTwoPi = 0.15915494309189533577f;
    float cycles = radians * kInvTwoPi;
    cycles -= std::floor (cycles);
    constexpr float n = 2048.0f;
    const float x = cycles * n;
    const int i = (int) x;
    const float f = x - (float) i;
    return table[(size_t) i] + f * (table[(size_t) i + 1] - table[(size_t) i]);
}

inline float fastLog2 (float x) noexcept
{
    if (! (x > 1.0e-20f))
        return -66.0f;

    union
    {
        float f;
        std::uint32_t u;
    } v { x };

    const float e = (float) ((int) ((v.u >> 23) & 255u) - 127);
    v.u = (v.u & 0x007fffffu) | 0x3f800000u;
    const float m = v.f - 1.0f;
    // log2(1+m) on [0, 1], max error ~1e-5
    const float p = m * (1.44269504089f
                          + m * (-0.72134752044f
                                 + m * (0.48089834696f
                                        + m * (-0.36067376022f
                                               + m * (0.28853900817f
                                                      + m * -0.19922601356f)))));
    return e + p;
}

inline float fastExp2 (float x) noexcept
{
    if (x < -80.0f)
        return 0.0f;
    if (x > 80.0f)
        return 1.0e24f;

    const int i = (int) std::floor (x);
    const float f = x - (float) i;
    const float y = f * 0.69314718056f;
    const float mantissa = 1.0f + y * (1.0f + y * (0.5f + y * (0.1666666716f
                                                                + y * (0.0416666679f
                                                                       + y * 0.0083333338f))));
    return std::ldexp (mantissa, i);
}

inline float flushTiny (float x) noexcept
{
    return std::abs (x) < 1.0e-30f ? 0.0f : x;
}

// tan(pi * fc / fs) via a Padé form, valid for fc < 0.45*fs.
inline float tptG (float fc, float sampleRate) noexcept
{
    const float ratio = std::clamp (fc / sampleRate, 0.0f, 0.45f);
    const float x = 3.14159265358979323846f * ratio;
    const float x2 = x * x;
    const float den = 15.0f - 6.0f * x2;
    if (den < 0.05f)
        return std::tan (x);
    return x * (15.0f - x2) / den;
}

struct TptSvf
{
    float g = 0.0f, k = 1.0f, a1 = 1.0f, a2 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;

    void set (float fc, float q, float sampleRate) noexcept
    {
        g = tptG (fc, sampleRate);
        k = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    float tick (float v0, bool lowpass) noexcept
    {
        const float v1 = a1 * ic1 + a2 * (v0 - ic2);
        const float v2 = ic2 + g * v1;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return lowpass ? v2 : (v0 - k * v1 - v2);
    }
};

struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    void setLowShelf (float sampleRate, float freq, float gainDb) noexcept
    {
        if (std::abs (gainDb) < 1.0e-5f)
        {
            b0 = 1.0f; b1 = 0.0f; b2 = 0.0f; a1 = 0.0f; a2 = 0.0f;
            return;
        }

        const double A = std::pow (10.0, (double) gainDb / 40.0);
        const double w0 = 6.28318530717958647692 * (double) freq / (double) sampleRate;
        const double c = std::cos (w0);
        const double s = std::sin (w0);
        const double alpha = s * 0.7071067811865476; // S = 1 → sin/2 * sqrt(2)
        const double sqrtA = std::sqrt (A);
        const double a0 = (A + 1.0) + (A - 1.0) * c + 2.0 * sqrtA * alpha;
        b0 = (float) (A * ((A + 1.0) - (A - 1.0) * c + 2.0 * sqrtA * alpha) / a0);
        b1 = (float) (2.0 * A * ((A - 1.0) - (A + 1.0) * c) / a0);
        b2 = (float) (A * ((A + 1.0) - (A - 1.0) * c - 2.0 * sqrtA * alpha) / a0);
        a1 = (float) (-2.0 * ((A - 1.0) + (A + 1.0) * c) / a0);
        a2 = (float) (((A + 1.0) + (A - 1.0) * c - 2.0 * sqrtA * alpha) / a0);
    }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

inline float softClip (float x, float ceilingLin) noexcept
{
    const float ax = std::abs (x);
    const float thr = ceilingLin * 0.7079457843841379f; // −3 dB
    if (ax <= thr)
        return x;
    const float sign = x < 0.0f ? -1.0f : 1.0f;
    if (ax >= ceilingLin)
        return sign * ceilingLin;
    const float w = ceilingLin - thr;
    const float u = (ax - thr) / w;
    const float y = thr + w * (u + u * u - u * u * u);
    return sign * y;
}

} // namespace abyss
