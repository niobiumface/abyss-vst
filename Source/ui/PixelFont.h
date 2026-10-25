#pragma once

#include <juce_graphics/juce_graphics.h>

namespace abyss
{

int pixelTextWidth (juce::StringRef text) noexcept;
bool pixelGlyph (juce::juce_wchar ch, char rows[7][6]) noexcept;
juce::Image renderPixelText (juce::StringRef text, juce::Colour colour);

} // namespace abyss
