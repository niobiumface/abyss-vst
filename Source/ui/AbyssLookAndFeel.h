#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace abyss
{

class AbyssLookAndFeel : public juce::LookAndFeel_V4
{
public:
    std::function<juce::Image (juce::Slider&)> knobImage;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};

} // namespace abyss
