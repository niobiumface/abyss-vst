#include "IndexedImage.h"

#include <cstring>

namespace abyss
{

namespace
{
constexpr PaletteEntry kPalette[] = {
    { "K0", 0x05, 0x04, 0x08 }, { "K1", 0x0C, 0x0A, 0x12 }, { "V0", 0x16, 0x12, 0x2A },
    { "V1", 0x22, 0x1B, 0x37 }, { "B1", 0x1B, 0x25, 0x39 }, { "V2", 0x31, 0x29, 0x4B },
    { "B2", 0x2B, 0x3B, 0x55 }, { "V3", 0x47, 0x3D, 0x67 }, { "V4", 0x5F, 0x55, 0x85 },
    { "N0", 0x6E, 0x66, 0x58 }, { "N1", 0xAB, 0xA2, 0x8C }, { "N2", 0xE9, 0xE2, 0xCC },
    { "R0", 0x2E, 0x07, 0x0B }, { "R1", 0x6A, 0x0E, 0x16 }, { "R2", 0xA8, 0x16, 0x1F },
    { "R3", 0xE2, 0x3A, 0x32 }, { "C0", 0x0B, 0x2E, 0x34 }, { "C1", 0x1A, 0x7F, 0x8A },
    { "C2", 0x59, 0xE3, 0xEA },
};
}

const PaletteEntry* palette() noexcept { return kPalette; }
int paletteSize() noexcept { return 19; }

int indexForName (const char* name) noexcept
{
    for (int i = 0; i < 19; ++i)
        if (std::strcmp (kPalette[i].name, name) == 0)
            return i;
    return -1;
}

juce::Colour colourForIndex (int index) noexcept
{
    if (index < 0 || index >= 19)
        return juce::Colours::transparentBlack;
    const auto& c = kPalette[index];
    return juce::Colour (c.r, c.g, c.b);
}

IndexedImage IndexedImage::fromImage (const juce::Image& image, bool* ok)
{
    IndexedImage out;
    if (! image.isValid())
    {
        if (ok != nullptr) *ok = false;
        return out;
    }

    out.w = image.getWidth();
    out.h = image.getHeight();
    out.idx.assign ((size_t) out.w * (size_t) out.h, 0);
    bool good = true;

    for (int y = 0; y < out.h; ++y)
    {
        for (int x = 0; x < out.w; ++x)
        {
            const auto c = image.getPixelAt (x, y);
            const size_t n = (size_t) y * (size_t) out.w + (size_t) x;
            if (c.getAlpha() == 0)
            {
                out.idx[n] = 255;
                continue;
            }
            if (c.getAlpha() != 255)
                good = false;

            int found = -1;
            for (int i = 0; i < 19; ++i)
                if (c.getRed() == kPalette[i].r && c.getGreen() == kPalette[i].g && c.getBlue() == kPalette[i].b)
                {
                    found = i;
                    break;
                }
            if (found < 0)
            {
                good = false;
                found = 0;
            }
            out.idx[n] = (juce::uint8) found;
        }
    }

    if (ok != nullptr)
        *ok = good;
    jassert (good);
    return out;
}

juce::uint8 IndexedImage::at (int x, int y) const noexcept
{
    if (x < 0 || y < 0 || x >= w || y >= h)
        return 255;
    return idx[(size_t) y * (size_t) w + (size_t) x];
}

bool IndexedImage::samePixels (const IndexedImage& other) const noexcept
{
    return w == other.w && h == other.h && idx == other.idx;
}

int IndexedImage::countIndexRange (int first, int last) const noexcept
{
    int n = 0;
    for (auto v : idx)
        if (v >= first && v <= last)
            ++n;
    return n;
}

int IndexedImage::countTransparent() const noexcept
{
    int n = 0;
    for (auto v : idx)
        if (v == 255)
            ++n;
    return n;
}

juce::Image IndexedImage::toImage() const
{
    juce::Image image (juce::Image::ARGB, std::max (1, w), std::max (1, h), true);
    if (w <= 0 || h <= 0)
        return image;

    juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const auto v = at (x, y);
            if (v == 255)
                continue;
            data.setPixelColour (x, y, colourForIndex (v));
        }
    return image;
}

} // namespace abyss
