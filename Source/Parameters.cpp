#include "Parameters.h"

namespace abyss
{

namespace
{
juce::String signedNumber (float value, int decimals)
{
    juce::String text (value, decimals);
    if (value > 0.0f)
        text = "+" + text;
    return text;
}
}

juce::String formatPercent (float value) { return juce::String (value, 1) + " %"; }
juce::String formatDb (float value)      { return signedNumber (value, 1) + " dB"; }
juce::String formatDbfs (float value)    { return signedNumber (value, 1) + " dBFS"; }

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (const char* paramId, const char* name, float def, float skew = 1.0f)
    {
        NormalisableRange<float> range (0.0f, 100.0f, 0.1f);
        juce::ignoreUnused (skew);
        return std::make_unique<AudioParameterFloat> (
            ParameterID { paramId, 1 }, name, range, def,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return formatPercent (v); })
                .withValueFromStringFunction ([] (const String& text) { return text.getFloatValue(); }));
    };

    layout.add (percent (id::depth, "DEPTH", 0.0f));
    layout.add (percent (id::pressure, "PRESSURE", 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::inGain, 1 }, "Input",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return formatDb (v); })
            .withValueFromStringFunction ([] (const String& text) { return text.getFloatValue(); })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::outGain, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return formatDb (v); })
            .withValueFromStringFunction ([] (const String& text) { return text.getFloatValue(); })));

    layout.add (percent (id::mix, "Mix", 100.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { id::subMode, 1 }, "Sub Mode", StringArray { "Clean", "Grit" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { id::sub2, 1 }, "Sub -2", true));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { id::ceiling, 1 }, "Ceiling",
        NormalisableRange<float> (-6.0f, 0.0f, 0.1f), -0.3f,
        AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return formatDbfs (v); })
            .withValueFromStringFunction ([] (const String& text) { return text.getFloatValue(); })));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { id::autoGain, 1 }, "Auto Gain", true));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { id::os, 1 }, "Oversampling", StringArray { "Off", "2x", "4x" }, 1));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { id::bypass, 1 }, "Bypass", false));

    return layout;
}

} // namespace abyss
