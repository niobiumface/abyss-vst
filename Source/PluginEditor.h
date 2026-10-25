#pragma once

#include "PluginProcessor.h"
#include "ui/AbyssLookAndFeel.h"
#include "ui/BypassButton.h"
#include "ui/KnobStrip.h"
#include "ui/ScreenComponent.h"
#include "ui/SpriteBank.h"

#include <array>

namespace abyss
{

class AbyssAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer,
                                  private juce::AudioProcessorValueTreeState::Listener,
                                  private juce::ChangeListener,
                                  private juce::AsyncUpdater
{
public:
    explicit AbyssAudioProcessorEditor (AbyssAudioProcessor&);
    ~AbyssAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

    void setUiScale (int newScale);
    void pumpUiForTest();
    int getUiScale() const noexcept { return scale; }
    int animationFrame() const noexcept { return shownFrame; }
    bool isTimerRunning() const noexcept { return juce::Timer::isTimerRunning(); }
    void advanceAnimationForTest();

private:
    void timerCallback() override;
    void parameterChanged (const juce::String&, float) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void handleAsyncUpdate() override;

    void layoutControls();
    void updateTimerState();
    void refreshImages (bool fullScreen);
    void rebuildValueImage();
    void showPresetMenu();
    void showContextMenu();
    juce::String currentValueText() const;
    void captureState (juce::MemoryBlock& dest) const;
    void restoreState (const juce::MemoryBlock& src);

    AbyssAudioProcessor& proc;
    juce::SharedResourcePointer<SpriteBank> bank;
    AbyssLookAndFeel lookAndFeel;
    ScreenComponent screen;
    KnobStrip depthKnob { "DEPTH", 13 };
    KnobStrip inKnob { "Input", 7 };
    KnobStrip outKnob { "Output", 7 };
    KnobStrip pressureKnob { "PRESSURE", 13 };
    BypassButton bypassButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt, inAtt, outAtt, pressureAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    int scale = 3;
    int shownFrame = 0;
    int animStep = 0;
    int shownLevel = 0;
    bool dragTip = false;
    juce::String dragText;
    juce::Point<int> dragPos;
    juce::String shownText;
    juce::Image valueImage;
    juce::Image creditImage;
    juce::Image backdrop;
    juce::Image tipImage;
    juce::MemoryBlock stateA, stateB;

    static constexpr std::array<int, 8> kSequence { 0, 1, 0, 1, 0, 1, 0, 2 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssAudioProcessorEditor)
};

} // namespace abyss
