#include "ScreenComponent.h"

namespace abyss
{

void ScreenComponent::setImages (juce::Image screen, juce::Image cabinet, int newScale, int screenX, int screenY)
{
    screenImage = std::move (screen);
    cabinetImage = std::move (cabinet);
    scale = newScale;
    originX = screenX;
    originY = screenY;
}

void ScreenComponent::paint (juce::Graphics&)
{
    // The editor paints a software-composited cabinet so CRT corners stay pixel-exact.
}

} // namespace abyss
