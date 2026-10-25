#pragma once

#include <juce_graphics/juce_graphics.h>

namespace abyss
{

juce::Image scaleNearest (const juce::Image& source, int factor);

} // namespace abyss
