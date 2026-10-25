#include "BypassButton.h"

namespace abyss
{

BypassButton::BypassButton()
    : juce::Button ("Bypass")
{
    setClickingTogglesState (true);
    setTitle ("Bypass");
    setWantsKeyboardFocus (false);
}

void BypassButton::setVisuals (juce::Image off, juce::Image on, int scale)
{
    offImage = std::move (off);
    onImage = std::move (on);
    uiScale = scale;
    repaint();
}

void BypassButton::paintButton (juce::Graphics& g, bool, bool down)
{
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    const bool showOn = getToggleState() || down;
    const auto& image = showOn ? onImage : offImage;
    const int dy = down ? uiScale : 0;
    if (image.isValid())
        g.drawImageAt (image, 0, dy);
}

bool BypassButton::hitTest (int x, int y)
{
    const float rx = 10.0f * (float) uiScale;
    const float ry = 8.0f * (float) uiScale;
    const float dx = (float) x - rx;
    const float dy = (float) y - ry;
    if (rx <= 0.0f || ry <= 0.0f)
        return false;
    const float nx = dx / rx;
    const float ny = dy / ry;
    return nx * nx + ny * ny <= 1.0f;
}

} // namespace abyss
