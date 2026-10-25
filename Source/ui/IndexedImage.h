#pragma once

#include <juce_graphics/juce_graphics.h>

#include <vector>

namespace abyss
{

struct PaletteEntry
{
    const char* name;
    juce::uint8 r, g, b;
};

const PaletteEntry* palette() noexcept;
int paletteSize() noexcept;
int indexForName (const char* name) noexcept;
juce::Colour colourForIndex (int index) noexcept;

struct IndexedImage
{
    int w = 0;
    int h = 0;
    std::vector<juce::uint8> idx;

    static IndexedImage fromImage (const juce::Image& image, bool* ok = nullptr);
    juce::uint8 at (int x, int y) const noexcept;
    bool samePixels (const IndexedImage& other) const noexcept;
    int countIndexRange (int first, int last) const noexcept;
    int countTransparent() const noexcept;
    juce::Image toImage() const;
};

} // namespace abyss
