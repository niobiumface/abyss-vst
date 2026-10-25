#pragma once

namespace abyss
{

class PressureDrive
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    float process (float x, float pressure01, int channel) noexcept;

private:
    float sampleRate = 48000.0f;
    float r = 0.0f;
    float x1[2] { 0.0f, 0.0f };
    float y1[2] { 0.0f, 0.0f };
    float lastP = -1.0f;
    float pre = 1.0f, b = 0.0f, bias = 0.0f, norm = 1.0f, w = 0.0f;
};

} // namespace abyss
