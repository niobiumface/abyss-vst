#pragma once

namespace abyss
{

class AutoGain
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // Called once per host block with the mean-square of the pre-gain signal
    // and of the input (after input gain). Advances the 200 ms ramp target.
    void updateTargets (float meanSquareProcessed, float meanSquareInput,
                        float blockSeconds, bool enabled, bool forceUnity) noexcept;

    float next (int steps = 1) noexcept;
    float current() const noexcept { return k; }

private:
    float sampleRate = 48000.0f;
    float rmsIn = 1.0e-6f;
    float rmsProc = 1.0e-6f;
    float k = 1.0f;
    float target = 1.0f;
    float coefK = 0.0f;
};

} // namespace abyss
