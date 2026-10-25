#include "PluginEditor.h"

#include "Parameters.h"
#include "presets/PresetManager.h"
#include "ui/DepthMapping.h"
#include "ui/PixelFont.h"
#include "ui/PixelScaler.h"
#include "ui/PressureFx.h"

#include <cmath>

namespace abyss
{

namespace
{
constexpr juce::uint32 kN1 = 0xFFABA28C;
constexpr juce::uint32 kN2 = 0xFFE9E2CC;
constexpr juce::uint32 kK0 = 0xFF050408;
constexpr juce::uint32 kC2 = 0xFF59E3EA;
constexpr juce::uint32 kR3 = 0xFFE23A32;

int knobIndex (juce::Slider& slider, int frames)
{
    const float norm = (float) slider.valueToProportionOfLength (slider.getValue());
    return juce::jlimit (0, frames - 1, (int) std::lround (norm * (float) (frames - 1)));
}
}

AbyssAudioProcessorEditor::AbyssAudioProcessorEditor (AbyssAudioProcessor& processor)
    : juce::AudioProcessorEditor (processor), proc (processor)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setResizable (false, false);
    setWantsKeyboardFocus (true);

    depthKnob.centerX = depthKnob.centerY = 11;
    pressureKnob.centerX = pressureKnob.centerY = 11;
    inKnob.centerX = inKnob.centerY = 5;
    outKnob.centerX = outKnob.centerY = 5;

    addAndMakeVisible (screen);
    addAndMakeVisible (depthKnob);
    addAndMakeVisible (inKnob);
    addAndMakeVisible (outKnob);
    addAndMakeVisible (pressureKnob);
    addAndMakeVisible (bypassButton);
    screen.setInterceptsMouseClicks (false, false);

    auto bind = [this] (KnobStrip& knob, const char* paramId)
    {
        auto* parameter = proc.apvts.getParameter (paramId);
        knob.parameter = parameter;
        if (parameter != nullptr)
            knob.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
        proc.apvts.addParameterListener (paramId, this);
    };
    bind (depthKnob, id::depth);
    bind (inKnob, id::inGain);
    bind (outKnob, id::outGain);
    bind (pressureKnob, id::pressure);
    proc.apvts.addParameterListener (id::bypass, this);
    proc.apvts.addParameterListener (id::mix, this);
    proc.apvts.addParameterListener (id::subMode, this);
    proc.apvts.addParameterListener (id::sub2, this);
    proc.apvts.addParameterListener (id::ceiling, this);
    proc.apvts.addParameterListener (id::autoGain, this);
    proc.apvts.addParameterListener (id::os, this);

    depthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id::depth, depthKnob);
    inAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id::inGain, inKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id::outGain, outKnob);
    pressureAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id::pressure, pressureKnob);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id::bypass, bypassButton);

    lookAndFeel.knobImage = [this] (juce::Slider& slider)
    {
        int which = 0;
        int frames = 64;
        if (slider.getName() == "PRESSURE") which = 1;
        if (slider.getName() == "Input" || slider.getName() == "Output")
        {
            which = 2;
            frames = 32;
        }
        return bank->knobImage (which, knobIndex (slider, frames), scale);
    };

    proc.addChangeListener (this);
    const int stored = proc.uiScale.load();
    setUiScale (stored >= 1 && stored <= 5 ? stored : 3);
}

AbyssAudioProcessorEditor::~AbyssAudioProcessorEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    for (auto* param : { id::depth, id::pressure, id::inGain, id::outGain, id::bypass, id::mix,
                         id::subMode, id::sub2, id::ceiling, id::autoGain, id::os })
        proc.apvts.removeParameterListener (param, this);
    setLookAndFeel (nullptr);
}

void AbyssAudioProcessorEditor::setUiScale (int newScale)
{
    scale = juce::jlimit (1, 5, newScale);
    proc.uiScale.store (scale);
    depthKnob.uiScale = inKnob.uiScale = outKnob.uiScale = pressureKnob.uiScale = bypassButton.uiScale = scale;
    setSize (layout::kNativeW * scale, layout::kNativeH * scale);
    creditImage = scaleNearest (renderPixelText (layout::kCreditText, juce::Colour (kN1)), scale);
    shownText.clear();
    layoutControls();
    rebuildValueImage();
    refreshImages (true);
}

void AbyssAudioProcessorEditor::pumpUiForTest()
{
    handleAsyncUpdate();
}

void AbyssAudioProcessorEditor::layoutControls()
{
    const int s = scale;
    screen.setBounds (layout::kScreenX * s, layout::kScreenY * s, layout::kScreenW * s, layout::kScreenH * s);
    depthKnob.setBounds (43 * s, 132 * s, 24 * s, 24 * s);
    inKnob.setBounds (79 * s, 140 * s, 12 * s, 12 * s);
    outKnob.setBounds (101 * s, 140 * s, 12 * s, 12 * s);
    pressureKnob.setBounds (133 * s, 132 * s, 24 * s, 24 * s);
    bypassButton.setBounds (182 * s, 136 * s, 20 * s, 16 * s);
    bypassButton.setVisuals (bank->bypassImage (false, s), bank->bypassImage (true, s), s);
}

void AbyssAudioProcessorEditor::resized()
{
    layoutControls();
}

void AbyssAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    if (backdrop.isValid())
        g.drawImageAt (backdrop, 0, 0);
    else
        g.drawImageAt (bank->cabinetImage (scale), 0, 0);
    if (creditImage.isValid())
        g.drawImageAt (creditImage, layout::kCreditX * scale, layout::kCreditY * scale);
    if (valueImage.isValid())
    {
        const int x = ((layout::kPlateX + layout::kPlateR + 1 - pixelTextWidth (shownText)) / 2) * scale;
        g.drawImageAt (valueImage, x, layout::kTextY * scale);
    }
    if (dragTip && tipImage.isValid())
        g.drawImageAt (tipImage, dragPos.x, dragPos.y);
}

juce::String AbyssAudioProcessorEditor::currentValueText() const
{
    const bool bypassed = proc.apvts.getRawParameterValue (id::bypass)->load() >= 0.5f;
    if (bypassed)
        return "BYPASS";
    const int depth = juce::roundToInt (proc.apvts.getRawParameterValue (id::depth)->load());
    const int pressure = juce::roundToInt (proc.apvts.getRawParameterValue (id::pressure)->load());
    return "D " + juce::String (depth) + "%  P " + juce::String (pressure) + "%";
}

void AbyssAudioProcessorEditor::rebuildValueImage()
{
    const auto text = currentValueText();
    if (text == shownText && valueImage.isValid())
        return;
    shownText = text;
    const bool bypassed = text == "BYPASS";
    const auto colour = juce::Colour (bypassed ? kR3 : kC2);
    valueImage = scaleNearest (renderPixelText (text, colour), scale);
    repaint (layout::kPlateX * scale, layout::kPlateY * scale,
             (layout::kPlateR - layout::kPlateX + 1) * scale,
             (layout::kPlateB - layout::kPlateY + 1) * scale);
}

void AbyssAudioProcessorEditor::refreshImages (bool fullScreen)
{
    const bool bypassed = proc.apvts.getRawParameterValue (id::bypass)->load() >= 0.5f;
    const float depth = proc.apvts.getRawParameterValue (id::depth)->load();
    const float pressure = proc.apvts.getRawParameterValue (id::pressure)->load();
    const int level = depthToLevel (depth);
    const int frame = (bypassed || ! proc.animate.load()) ? 0 : shownFrame;
    const int stage = (bypassed || ! proc.crtFx.load()) ? 0 : pressureStage (pressure);
    const IndexedImage screenIdx = bypassed ? bank->screenOff
                                            : (stage > 0 ? applyPressureFx (bank->anim[level][frame], stage, frame)
                                                         : bank->anim[level][frame]);
    auto native = bank->cabinet.toImage();
    {
        const auto screenNative = screenIdx.toImage();
        juce::Image::BitmapData dst (native, juce::Image::BitmapData::readWrite);
        juce::Image::BitmapData src (screenNative, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < bank->cabinet.h; ++y)
            for (int x = 0; x < bank->cabinet.w; ++x)
            {
                if (bank->cabinet.at (x, y) != 255)
                    continue;
                const int sx = x - layout::kScreenX;
                const int sy = y - layout::kScreenY;
                if (sx < 0 || sy < 0 || sx >= src.width || sy >= src.height)
                    continue;
                dst.setPixelColour (x, y, src.getPixelColour (sx, sy));
            }
    }
    backdrop = scaleNearest (native, scale);
    screen.setImages (bank->screenImage (level, frame, stage, scale, bypassed),
                      bank->cabinetImage (scale), scale, layout::kScreenX, layout::kScreenY);

    if (fullScreen || level != shownLevel || bypassed)
        screen.repaint();
    else
    {
        const auto dirty = bank->animDirty[level];
        screen.repaint (dirty.getX() * scale, dirty.getY() * scale,
                        juce::jmax (1, dirty.getWidth() * scale),
                        juce::jmax (1, dirty.getHeight() * scale));
    }
    shownLevel = level;
    rebuildValueImage();
    bypassButton.setVisuals (bank->bypassImage (false, scale), bank->bypassImage (true, scale), scale);
}

void AbyssAudioProcessorEditor::updateTimerState()
{
    const bool bypassed = proc.apvts.getRawParameterValue (id::bypass)->load() >= 0.5f;
    const bool run = proc.animate.load() && ! bypassed && isShowing();
    if (run && ! isTimerRunning())
    {
        animStep = 0;
        shownFrame = 0;
        startTimerHz (8);
    }
    else if (! run && isTimerRunning())
    {
        stopTimer();
        shownFrame = 0;
        refreshImages (true);
    }
}

void AbyssAudioProcessorEditor::timerCallback()
{
    shownFrame = kSequence[(size_t) animStep];
    animStep = (animStep + 1) % (int) kSequence.size();
    refreshImages (false);
}

void AbyssAudioProcessorEditor::advanceAnimationForTest()
{
    timerCallback();
}

void AbyssAudioProcessorEditor::parameterChanged (const juce::String& paramId, float value)
{
    juce::ignoreUnused (paramId, value);
    triggerAsyncUpdate();
}

void AbyssAudioProcessorEditor::handleAsyncUpdate()
{
    updateTimerState();
    refreshImages (true);
    repaint();
}

void AbyssAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const int stored = proc.uiScale.load();
    if (stored != scale)
        setUiScale (stored);
    updateTimerState();
    refreshImages (true);
}

void AbyssAudioProcessorEditor::visibilityChanged() { updateTimerState(); }
void AbyssAudioProcessorEditor::parentHierarchyChanged() { updateTimerState(); }

bool AbyssAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        proc.undoManager.undo();
        return true;
    }
    if (key == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        proc.undoManager.redo();
        return true;
    }
    return false;
}

void AbyssAudioProcessorEditor::captureState (juce::MemoryBlock& dest) const
{
    dest.reset();
    proc.getStateInformation (dest);
}

void AbyssAudioProcessorEditor::restoreState (const juce::MemoryBlock& src)
{
    if (src.getSize() > 0)
        proc.setStateInformation (src.getData(), (int) src.getSize());
}

void AbyssAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const int x = e.x / scale;
    const int y = e.y / scale;
    if (e.mods.isPopupMenu())
    {
        showContextMenu();
        return;
    }
    if (x >= layout::kLogoX && x <= layout::kLogoR && y >= layout::kLogoY && y <= layout::kLogoB)
        showPresetMenu();
}

void AbyssAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu factory;
    for (int i = 0; i < kNumFactoryPresets; ++i)
        factory.addItem (i + 1, factoryPreset (i).name);

    juce::PopupMenu user;
    const auto files = userPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.abyss");
    int id = 100;
    for (auto file : files)
        user.addItem (id++, file.getFileNameWithoutExtension());
    if (files.isEmpty())
        user.addItem (99, "(leer)", false);

    juce::PopupMenu menu;
    menu.addSubMenu ("Factory", factory);
    menu.addSubMenu ("User", user);
    menu.addItem (200, "Save...");
    menu.addItem (201, "Load...");
    menu.addItem (202, "A/B tauschen");
    menu.addItem (203, "A nach B kopieren");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this, files] (int result)
                        {
                            if (result >= 1 && result <= kNumFactoryPresets)
                                proc.setCurrentProgram (result - 1);
                            else if (result >= 100 && result - 100 < files.size())
                            {
                                if (auto xml = juce::XmlDocument::parse (files[result - 100]))
                                    proc.loadStateXml (*xml);
                            }
                            else if (result == 200)
                            {
                                auto chooser = std::make_shared<juce::FileChooser> ("Save preset", userPresetDirectory(), "*.abyss");
                                chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                                          | juce::FileBrowserComponent::warnAboutOverwriting,
                                                      [this, chooser] (const juce::FileChooser& c)
                                                      {
                                                          auto file = c.getResult();
                                                          if (file == juce::File())
                                                              return;
                                                          if (! file.hasFileExtension (".abyss"))
                                                              file = file.withFileExtension (".abyss");
                                                          if (auto xml = proc.createStateXml())
                                                              xml->writeTo (file);
                                                      });
                            }
                            else if (result == 201)
                            {
                                auto chooser = std::make_shared<juce::FileChooser> ("Load preset", userPresetDirectory(), "*.abyss");
                                chooser->launchAsync (juce::FileBrowserComponent::openMode,
                                                      [this, chooser] (const juce::FileChooser& c)
                                                      {
                                                          auto file = c.getResult();
                                                          if (auto xml = juce::XmlDocument::parse (file))
                                                              proc.loadStateXml (*xml);
                                                      });
                            }
                            else if (result == 202)
                            {
                                juce::MemoryBlock current;
                                captureState (current);
                                if (stateA.getSize() == 0) stateA = current;
                                if (stateB.getSize() == 0) stateB = current;
                                auto next = stateA;
                                stateA = stateB;
                                stateB = next;
                                restoreState (stateA);
                            }
                            else if (result == 203)
                            {
                                captureState (stateA);
                                stateB = stateA;
                            }
                        });
}

void AbyssAudioProcessorEditor::showContextMenu()
{
    auto tick = [] (juce::PopupMenu& menu, int item, const juce::String& name, bool on)
    {
        menu.addItem (item, name, true, on);
    };

    const float mix = proc.apvts.getRawParameterValue (id::mix)->load();
    const float ceiling = proc.apvts.getRawParameterValue (id::ceiling)->load();
    const int mode = (int) std::lround (proc.apvts.getRawParameterValue (id::subMode)->load());
    const bool sub2 = proc.apvts.getRawParameterValue (id::sub2)->load() >= 0.5f;
    const bool ag = proc.apvts.getRawParameterValue (id::autoGain)->load() >= 0.5f;
    const int osMode = (int) std::lround (proc.apvts.getRawParameterValue (id::os)->load());

    juce::PopupMenu mixMenu;
    const float mixes[] = { 0, 25, 50, 75, 100 };
    for (int i = 0; i < 5; ++i)
        tick (mixMenu, 10 + i, juce::String (mixes[i], 0) + " %", std::abs (mix - mixes[i]) < 0.2f);

    juce::PopupMenu ceilMenu;
    const float ceils[] = { -6, -3, -1, -0.3f, 0 };
    const char* ceilNames[] = { "-6 dBFS", "-3 dBFS", "-1 dBFS", "-0.3 dBFS", "0 dBFS" };
    for (int i = 0; i < 5; ++i)
        tick (ceilMenu, 20 + i, ceilNames[i], std::abs (ceiling - ceils[i]) < 0.05f);

    juce::PopupMenu menu;
    menu.addSubMenu ("Mix", mixMenu);
    menu.addSubMenu ("Ceiling", ceilMenu);
    tick (menu, 30, "Sub Mode Clean", mode == 0);
    tick (menu, 31, "Sub Mode Grit", mode == 1);
    tick (menu, 32, "Sub -2", sub2);
    tick (menu, 33, "Auto Gain", ag);
    tick (menu, 40, "Oversampling Off", osMode == 0);
    tick (menu, 41, "Oversampling 2x", osMode == 1);
    tick (menu, 42, "Oversampling 4x", osMode == 2);
    tick (menu, 50, "UI-Groesse 2x", scale == 2);
    tick (menu, 51, "UI-Groesse 3x", scale == 3);
    tick (menu, 52, "UI-Groesse 4x", scale == 4);
    tick (menu, 53, "UI-Groesse 5x", scale == 5);
    tick (menu, 60, "Animation", proc.animate.load());
    tick (menu, 61, "CRT-Effekt", proc.crtFx.load());
    menu.addItem (70, "Reset all");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this] (int result)
                        {
                            auto setP = [this] (const char* paramId, float real)
                            {
                                if (auto* p = proc.apvts.getParameter (paramId))
                                    p->setValueNotifyingHost (p->convertTo0to1 (real));
                            };
                            const float mixes[] = { 0, 25, 50, 75, 100 };
                            const float ceils[] = { -6, -3, -1, -0.3f, 0 };
                            if (result >= 10 && result <= 14) setP (id::mix, mixes[result - 10]);
                            else if (result >= 20 && result <= 24) setP (id::ceiling, ceils[result - 20]);
                            else if (result == 30) setP (id::subMode, 0);
                            else if (result == 31) setP (id::subMode, 1);
                            else if (result == 32) setP (id::sub2, proc.apvts.getRawParameterValue (id::sub2)->load() >= 0.5f ? 0.0f : 1.0f);
                            else if (result == 33) setP (id::autoGain, proc.apvts.getRawParameterValue (id::autoGain)->load() >= 0.5f ? 0.0f : 1.0f);
                            else if (result >= 40 && result <= 42) setP (id::os, (float) (result - 40));
                            else if (result >= 50 && result <= 53) setUiScale (result - 48);
                            else if (result == 60) { proc.animate.store (! proc.animate.load()); updateTimerState(); refreshImages (true); }
                            else if (result == 61) { proc.crtFx.store (! proc.crtFx.load()); refreshImages (true); }
                            else if (result == 70)
                            {
                                for (auto* p : proc.getParameters())
                                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                                        ranged->setValueNotifyingHost (ranged->getDefaultValue());
                                proc.animate.store (true);
                                proc.crtFx.store (true);
                                setUiScale (3);
                            }
                        });
}

} // namespace abyss
