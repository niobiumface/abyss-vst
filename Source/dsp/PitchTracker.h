#pragma once

#include "FastMath.h"

namespace abyss
{

struct PitchFrame
{
    float trackSig = 0.0f;
    float env = 0.0f;
    float envSub = 0.0f;
    float tAvg = 0.0f;
    bool valid = false;
    bool ff1 = false;
    bool ff2 = false;
    bool ff1Rising = false;
    bool ff2Rising = false;
    bool triggered = false;
    int confidence = 0;
};

class PitchTracker
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    PitchFrame process (float x) noexcept;

    bool isValid() const noexcept { return valid; }

private:
    double sampleRate = 48000.0;
    double pos = 0.0;
    double lastZc = 0.0;
    double prevZc = 0.0;
    double lastValidPos = 0.0;
    double invalidSpan = 2400.0;

    float prevY = 0.0f;
    float env = 0.0f;
    float envSub = 0.0f;
    float tAvg = 0.0f;
    float atk = 0.0f, rel = 0.0f, atkSub = 0.0f, relSub = 0.0f;

    bool armed = false;
    bool hasZc = false;
    bool hasPrevZc = false;
    bool ff1 = false;
    bool ff2 = false;
    bool valid = false;
    int confidence = 0;

    struct TptSvf hpf, lpf;
};

} // namespace abyss
