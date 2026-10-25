#include "PixelScaler.h"

#include <cstring>

namespace abyss
{

juce::Image scaleNearest (const juce::Image& source, int factor)
{
    if (! source.isValid() || factor <= 1)
        return source;

    const int w = source.getWidth();
    const int h = source.getHeight();
    juce::Image dest (source.getFormat(), w * factor, h * factor, ! source.isARGB());
    juce::Image::BitmapData src (source, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst (dest, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const auto* pixel = src.getPixelPointer (x, y);
            for (int dy = 0; dy < factor; ++dy)
            {
                auto* row = dst.getLinePointer (y * factor + dy);
                for (int dx = 0; dx < factor; ++dx)
                    std::memcpy (row + (x * factor + dx) * dst.pixelStride, pixel, (size_t) dst.pixelStride);
            }
        }
    }
    return dest;
}

} // namespace abyss
