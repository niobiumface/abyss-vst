#pragma once

namespace abyss
{

class PressureComp
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setPressure (float pressure01) noexcept;

    // Linked stereo. Returns the linear gain applied this sample.
    float process (float& l, float& r, int channels) noexcept;

private:
    void rebuild (float p) noexcept;

    float sampleRate = 48000.0f;
    float pressure = -1.0f;
    float ms = 0.0f;
    float gain = 1.0f;
    float coefRms = 0.0f;
    float atk = 0.0f, rel = 0.0f;
    float lut[256] {};
};

} // namespace abyss
