#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abyss
{

class BypassButton : public juce::Button
{
public:
    BypassButton();

    void setVisuals (juce::Image offImage, juce::Image onImage, int scale);
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
    bool hitTest (int x, int y) override;

    int uiScale = 3;

private:
    juce::Image offImage;
    juce::Image onImage;
};

} // namespace abyss
