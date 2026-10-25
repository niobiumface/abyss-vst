#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace abyss
{

class KnobStrip : public juce::Slider
{
public:
    KnobStrip (const juce::String& name, int nativeRadius);

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool hitTest (int x, int y) override;

    int nativeRadius = 13;
    int centerX = 11;
    int centerY = 11;
    int uiScale = 3;
    juce::RangedAudioParameter* parameter = nullptr;

private:
    void showEntry();
    bool fine = false;
};

} // namespace abyss
