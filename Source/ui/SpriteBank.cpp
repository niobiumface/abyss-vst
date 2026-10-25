#include "SpriteBank.h"

#include "PressureFx.h"

#include "BinaryData.h"

#include <cstdio>
#include <cstring>

namespace abyss
{

juce::Image SpriteBank::loadPng (const char* name)
{
    int size = 0;
    const char* data = AbyssData::getNamedResource (name, size);
    if (data == nullptr)
    {
        for (int i = 0; i < AbyssData::namedResourceListSize; ++i)
        {
            const char* candidate = AbyssData::namedResourceList[i];
            if (std::strstr (candidate, name) != nullptr)
            {
                data = AbyssData::getNamedResource (candidate, size);
                break;
            }
        }
    }
    if (data == nullptr || size <= 0)
        return {};
    return juce::ImageFileFormat::loadFrom (data, (size_t) size);
}

IndexedImage SpriteBank::mustLoad (const char* name, bool* ok)
{
    auto image = loadPng (name);
    return IndexedImage::fromImage (image, ok);
}

SpriteBank::SpriteBank()
{
    bool ok = true;
    for (int level = 0; level < 11; ++level)
    {
        for (int frame = 0; frame < 3; ++frame)
        {
            char name[32];
            std::snprintf (name, sizeof (name), "depth_%03d_f%d_png", level * 10, frame);
            bool frameOk = true;
            anim[level][frame] = mustLoad (name, &frameOk);
            ok = ok && frameOk;
        }

        juce::Rectangle<int> dirty;
        bool any = false;
        const auto& a = anim[level][0];
        for (int y = 0; y < a.h; ++y)
            for (int x = 0; x < a.w; ++x)
            {
                const auto p0 = a.at (x, y);
                if (p0 != anim[level][1].at (x, y) || p0 != anim[level][2].at (x, y))
                {
                    if (! any) { dirty = { x, y, 1, 1 }; any = true; }
                    else dirty = dirty.getUnion ({ x, y, 1, 1 });
                }
            }
        if (any)
            dirty.setX (0), dirty.setWidth (a.w);
        else
            dirty = { 0, 0, a.w, a.h };
        animDirty[level] = dirty;
    }

    cabinet = mustLoad ("ui_frame_png", &ok);
    screenOff = mustLoad ("screen_off_png", &ok);
    depthStrip = mustLoad ("knob_depth_strip_png", &ok);
    pressureStrip = mustLoad ("knob_pressure_strip_png", &ok);
    smallStrip = mustLoad ("knob_small_strip_png", &ok);
    bypassOff = mustLoad ("bypass_off_png", &ok);
    bypassOn = mustLoad ("bypass_on_png", &ok);
    loaded = ok && cabinet.w == 240 && anim[0][0].w == 112;
    jassert (loaded);
}

juce::Image SpriteBank::cached (const Key& key, const juce::Image& built)
{
    const auto found = cache.find (key);
    if (found != cache.end())
        return found->second;

    const size_t bytes = built.isValid()
                             ? (size_t) built.getWidth() * (size_t) built.getHeight() * 4u
                             : 0u;
    while (cacheBytes + bytes > 24u * 1024u * 1024u && ! cache.empty())
    {
        const auto& victim = cache.begin()->second;
        cacheBytes -= (size_t) victim.getWidth() * (size_t) victim.getHeight() * 4u;
        cache.erase (cache.begin());
    }
    cacheBytes += bytes;
    cache.emplace (key, built);
    return built;
}

juce::Image SpriteBank::screenImage (int level, int frame, int stage, int scale, bool bypass)
{
    level = juce::jlimit (0, 10, level);
    frame = juce::jlimit (0, 2, frame);
    stage = juce::jlimit (0, 3, stage);
    scale = juce::jlimit (1, 8, scale);
    Key key { 1, level, frame, bypass ? -1 : stage, scale };
    if (cache.count (key) != 0)
        return cache[key];

    IndexedImage src = bypass ? screenOff : anim[level][frame];
    if (! bypass && stage > 0)
        src = applyPressureFx (src, stage, frame);
    return cached (key, scaleNearest (src.toImage(), scale));
}

juce::Image SpriteBank::cabinetImage (int scale)
{
    scale = juce::jlimit (1, 8, scale);
    Key key { 2, scale, 0, 0, 0 };
    if (cache.count (key) != 0)
        return cache[key];
    return cached (key, scaleNearest (cabinet.toImage(), scale));
}

juce::Image SpriteBank::knobImage (int which, int frameIndex, int scale)
{
    const IndexedImage* strip = &depthStrip;
    int frameSize = 24;
    int frames = 64;
    if (which == 1) strip = &pressureStrip;
    if (which == 2) { strip = &smallStrip; frameSize = 12; frames = 32; }
    frameIndex = juce::jlimit (0, frames - 1, frameIndex);
    scale = juce::jlimit (1, 8, scale);
    Key key { 3, which, frameIndex, scale, 0 };
    if (cache.count (key) != 0)
        return cache[key];

    IndexedImage frame;
    frame.w = frameSize;
    frame.h = frameSize;
    frame.idx.resize ((size_t) frameSize * (size_t) frameSize);
    for (int y = 0; y < frameSize; ++y)
        for (int x = 0; x < frameSize; ++x)
            frame.idx[(size_t) y * (size_t) frameSize + (size_t) x]
                = strip->at (x, frameIndex * frameSize + y);
    return cached (key, scaleNearest (frame.toImage(), scale));
}

juce::Image SpriteBank::bypassImage (bool on, int scale)
{
    scale = juce::jlimit (1, 8, scale);
    Key key { 4, on ? 1 : 0, scale, 0, 0 };
    if (cache.count (key) != 0)
        return cache[key];
    return cached (key, scaleNearest ((on ? bypassOn : bypassOff).toImage(), scale));
}

} // namespace abyss
