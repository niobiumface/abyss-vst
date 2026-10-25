#include "KnobStrip.h"

namespace abyss
{

KnobStrip::KnobStrip (const juce::String& name, int radius)
    : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox),
      nativeRadius (radius)
{
    setName (name);
    setTitle (name);
    setWantsKeyboardFocus (false);
    setMouseDragSensitivity (250);
    setOpaque (false);
}

void KnobStrip::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showEntry();
        return;
    }
    fine = e.mods.isShiftDown() || e.mods.isCtrlDown();
    setMouseDragSensitivity (fine ? 2500 : 250);
    juce::Slider::mouseDown (e);
}

void KnobStrip::mouseDrag (const juce::MouseEvent& e)
{
    const bool nowFine = e.mods.isShiftDown() || e.mods.isCtrlDown();
    if (nowFine != fine)
    {
        fine = nowFine;
        setMouseDragSensitivity (fine ? 2500 : 250);
    }
    juce::Slider::mouseDrag (e);
}

void KnobStrip::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float dir = wheel.deltaY > 0.0f ? 1.0f : (wheel.deltaY < 0.0f ? -1.0f : 0.0f);
    if (dir == 0.0f)
        return;
    if (parameter != nullptr)
        parameter->beginChangeGesture();
    setValue (getValue() + dir * 0.01 * (getMaximum() - getMinimum()));
    if (parameter != nullptr)
        parameter->endChangeGesture();
}

bool KnobStrip::hitTest (int x, int y)
{
    const float cx = (float) centerX * (float) uiScale;
    const float cy = (float) centerY * (float) uiScale;
    const float r = (float) nativeRadius * (float) uiScale;
    const float dx = (float) x - cx;
    const float dy = (float) y - cy;
    return dx * dx + dy * dy <= r * r;
}

void KnobStrip::showEntry()
{
    auto window = std::make_shared<juce::AlertWindow> (getName(), "Wert eingeben", juce::AlertWindow::NoIcon);
    window->addTextEditor ("v", juce::String (getValue(), 1));
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window] (int result)
    {
        if (result != 1)
            return;
        if (parameter != nullptr)
            parameter->beginChangeGesture();
        setValue (window->getTextEditorContents ("v").getFloatValue());
        if (parameter != nullptr)
            parameter->endChangeGesture();
    }));
}

} // namespace abyss
