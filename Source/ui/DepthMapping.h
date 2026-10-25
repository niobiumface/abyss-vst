#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

namespace abyss
{

inline int depthToLevel (float depthPercent) noexcept
{
    const int tenths = (int) std::lround (juce::jlimit (0.0f, 100.0f, depthPercent) * 10.0f);
    return juce::jlimit (0, 10, (tenths + 50) / 100);
}

inline int pressureStage (float pressure) noexcept
{
    const int rounded = (int) std::lround (juce::jlimit (0.0f, 100.0f, pressure));
    return juce::jlimit (0, 3, (rounded * 3 + 50) / 100);
}

namespace layout
{
constexpr int kNativeW = 240;
constexpr int kNativeH = 186;
constexpr int kScreenX = 64;
constexpr int kScreenY = 38;
constexpr int kScreenW = 112;
constexpr int kScreenH = 74;
constexpr int kPlateX = 74;
constexpr int kPlateY = 113;
constexpr int kPlateR = 165;
constexpr int kPlateB = 122;
constexpr int kTextY = 115;
constexpr int kLogoX = 50;
constexpr int kLogoY = 4;
constexpr int kLogoR = 189;
constexpr int kLogoB = 31;
constexpr int kCreditX = 70;
constexpr int kCreditY = 174;
inline constexpr const char* kCreditText = "by n-f.pro";
}
} // namespace abyss
