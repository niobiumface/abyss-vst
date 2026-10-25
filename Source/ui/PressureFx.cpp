#include "PressureFx.h"

#include <algorithm>

namespace abyss
{

namespace
{
struct Shift { int y; int dx; };

const Shift* shiftsFor (int stage, int frame, int& count) noexcept
{
    static const Shift s2f0[] = { { 17, 1 }, { 46, -1 } };
    static const Shift s2f1[] = { { 30, 1 }, { 31, 1 }, { 58, -1 } };
    static const Shift s2f2[] = { { 9, -1 }, { 52, 1 } };
    static const Shift s3f0[] = { { 12, 2 }, { 13, 2 }, { 40, -1 }, { 41, -2 }, { 64, 1 } };
    static const Shift s3f1[] = { { 24, -2 }, { 25, -2 }, { 26, -1 }, { 50, 2 }, { 51, 1 } };
    static const Shift s3f2[] = { { 6, 1 }, { 33, -2 }, { 34, -2 }, { 60, 2 }, { 61, 2 } };

    count = 0;
    if (stage == 2 && frame == 0) { count = 2; return s2f0; }
    if (stage == 2 && frame == 1) { count = 3; return s2f1; }
    if (stage == 2 && frame == 2) { count = 2; return s2f2; }
    if (stage == 3 && frame == 0) { count = 5; return s3f0; }
    if (stage == 3 && frame == 1) { count = 5; return s3f1; }
    if (stage == 3 && frame == 2) { count = 5; return s3f2; }
    return nullptr;
}

void buildRemap (int stage, juce::uint8 map[19]) noexcept
{
    for (int i = 0; i < 19; ++i)
        map[i] = (juce::uint8) i;

    auto set = [&] (const char* from, const char* to)
    {
        const int a = indexForName (from);
        const int b = indexForName (to);
        if (a >= 0 && b >= 0)
            map[a] = (juce::uint8) b;
    };

    if (stage == 1)
    {
        set ("V1", "R0"); set ("B1", "V1");
    }
    else if (stage == 2)
    {
        set ("V0", "R0"); set ("V1", "R0"); set ("B1", "R0"); set ("V2", "R1"); set ("B2", "V2");
    }
    else if (stage >= 3)
    {
        set ("K1", "R0"); set ("V0", "R0"); set ("V1", "R1"); set ("B1", "R0");
        set ("V2", "R1"); set ("B2", "R1"); set ("V3", "R2"); set ("V4", "R2");
        set ("N0", "R2"); set ("N1", "R3");
    }
}
}

IndexedImage applyPressureFx (const IndexedImage& source, int stage, int frameIndex)
{
    IndexedImage out = source;
    if (source.w <= 0 || stage <= 0)
        return out;

    int count = 0;
    const Shift* shifts = shiftsFor (stage, frameIndex, count);
    if (shifts != nullptr)
    {
        IndexedImage shifted = source;
        for (int s = 0; s < count; ++s)
        {
            const int y = shifts[s].y;
            const int dx = shifts[s].dx;
            if (y < 0 || y >= source.h)
                continue;
            for (int x = 0; x < source.w; ++x)
            {
                const int srcX = std::clamp (x - dx, 0, source.w - 1);
                shifted.idx[(size_t) y * (size_t) source.w + (size_t) x] = source.at (srcX, y);
            }
        }
        out = std::move (shifted);
    }

    juce::uint8 map[19];
    buildRemap (stage, map);
    for (auto& v : out.idx)
        if (v < 19)
            v = map[v];
    return out;
}

} // namespace abyss
