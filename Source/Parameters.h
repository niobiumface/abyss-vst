#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace abyss
{

namespace id
{
inline constexpr const char* depth    = "depth";
inline constexpr const char* pressure = "pressure";
inline constexpr const char* inGain   = "inGain";
inline constexpr const char* outGain  = "outGain";
inline constexpr const char* mix      = "mix";
inline constexpr const char* subMode  = "subMode";
inline constexpr const char* sub2     = "sub2";
inline constexpr const char* ceiling  = "ceiling";
inline constexpr const char* autoGain = "autoGain";
inline constexpr const char* os       = "os";
inline constexpr const char* bypass   = "bypass";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

juce::String formatPercent (float value);
juce::String formatDb (float value);
juce::String formatDbfs (float value);

} // namespace abyss
