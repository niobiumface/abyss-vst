#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace abyss
{

struct FactoryPreset
{
    const char* resource;
    const char* name;
    float depth;
    float pressure;
    int subMode;
};

constexpr int kNumFactoryPresets = 10;

const FactoryPreset& factoryPreset (int index) noexcept;

juce::File userPresetDirectory();

void applyFactoryPreset (juce::AudioProcessorValueTreeState& state, int index);
bool applyPresetXml (juce::AudioProcessorValueTreeState& state, const juce::XmlElement& xml);

} // namespace abyss
