#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abyss
{

class ScreenComponent : public juce::Component
{
public:
    void setImages (juce::Image screen, juce::Image cabinet, int scale, int screenX, int screenY);
    void paint (juce::Graphics& g) override;

private:
    juce::Image screenImage;
    juce::Image cabinetImage;
    int scale = 1;
    int originX = 0;
    int originY = 0;
};

} // namespace abyss
