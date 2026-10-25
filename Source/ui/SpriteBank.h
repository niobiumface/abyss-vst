#pragma once

#include "IndexedImage.h"
#include "PixelScaler.h"

#include <juce_graphics/juce_graphics.h>

#include <map>
#include <vector>

namespace abyss
{

struct SpriteBank
{
    SpriteBank();

    IndexedImage anim[11][3];
    IndexedImage cabinet;
    IndexedImage screenOff;
    IndexedImage depthStrip;
    IndexedImage pressureStrip;
    IndexedImage smallStrip;
    IndexedImage bypassOff;
    IndexedImage bypassOn;
    juce::Rectangle<int> animDirty[11];
    bool loaded = false;

    juce::Image screenImage (int level, int frame, int stage, int scale, bool bypass);
    juce::Image cabinetImage (int scale);
    juce::Image knobImage (int which, int frameIndex, int scale);
    juce::Image bypassImage (bool on, int scale);

private:
    struct Key
    {
        int a, b, c, d, e;
        bool operator< (const Key& o) const noexcept
        {
            if (a != o.a) return a < o.a;
            if (b != o.b) return b < o.b;
            if (c != o.c) return c < o.c;
            if (d != o.d) return d < o.d;
            return e < o.e;
        }
    };

    juce::Image cached (const Key& key, const juce::Image& built);
    std::map<Key, juce::Image> cache;
    size_t cacheBytes = 0;

    static juce::Image loadPng (const char* name);
    static IndexedImage mustLoad (const char* name, bool* ok);
};

} // namespace abyss
