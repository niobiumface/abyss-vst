#include "AbyssLookAndFeel.h"

namespace abyss
{

void AbyssLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int, int,
                                         float, float, float, juce::Slider& slider)
{
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    if (knobImage == nullptr)
        return;
    const auto image = knobImage (slider);
    if (image.isValid())
        g.drawImageAt (image, x, y);
}

} // namespace abyss
