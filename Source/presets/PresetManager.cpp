#include "PresetManager.h"

#include "Parameters.h"

namespace abyss
{

namespace
{
constexpr FactoryPreset kFactory[kNumFactoryPresets] = {
    { "insert_coin_xml",    "Insert Coin",    0.0f,   0.0f, 0 },
    { "first_step_xml",     "First Step",    15.0f,  10.0f, 0 },
    { "freefall_xml",       "Freefall",      30.0f,  20.0f, 0 },
    { "black_shaft_xml",    "Black Shaft",   45.0f,  30.0f, 0 },
    { "low_light_xml",      "Low Light",     55.0f,  15.0f, 0 },
    { "red_eyes_xml",       "Red Eyes",      65.0f,  50.0f, 0 },
    { "bottomless_xml",     "Bottomless",    80.0f,  40.0f, 0 },
    { "crushing_dark_xml",  "Crushing Dark", 60.0f,  90.0f, 1 },
    { "game_over_xml",      "Game Over",     90.0f,  75.0f, 1 },
    { "the_abyss_xml",      "The Abyss",    100.0f, 100.0f, 1 },
};

void setReal (juce::AudioProcessorValueTreeState& state, const char* paramId, float real)
{
    if (auto* p = state.getParameter (paramId))
        p->setValueNotifyingHost (p->convertTo0to1 (real));
}

void setChoice (juce::AudioProcessorValueTreeState& state, const char* paramId, int index)
{
    if (auto* p = state.getParameter (paramId))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
}

void setBool (juce::AudioProcessorValueTreeState& state, const char* paramId, bool on)
{
    if (auto* p = state.getParameter (paramId))
        p->setValueNotifyingHost (on ? 1.0f : 0.0f);
}
}

const FactoryPreset& factoryPreset (int index) noexcept
{
    return kFactory[juce::jlimit (0, kNumFactoryPresets - 1, index)];
}

juce::File userPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("n-f.pro")
                   .getChildFile ("Abyss")
                   .getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

void applyFactoryPreset (juce::AudioProcessorValueTreeState& state, int index)
{
    const auto& preset = factoryPreset (index);
    for (auto* p : state.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    setReal (state, id::depth, preset.depth);
    setReal (state, id::pressure, preset.pressure);
    setChoice (state, id::subMode, preset.subMode);
}

bool applyPresetXml (juce::AudioProcessorValueTreeState& state, const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("AbyssPreset") && ! xml.hasAttribute ("depth"))
        return false;

    const float depth = (float) xml.getDoubleAttribute ("depth", 0.0);
    const float pressure = (float) xml.getDoubleAttribute ("pressure", 0.0);
    const int mode = xml.getIntAttribute ("subMode", 0);

    for (auto* p : state.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    setReal (state, id::depth, depth);
    setReal (state, id::pressure, pressure);
    setChoice (state, id::subMode, mode);
    juce::ignoreUnused (setBool);
    return true;
}

} // namespace abyss
