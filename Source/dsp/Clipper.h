#pragma once

#include "FastMath.h"

namespace abyss
{

struct Clipper
{
    static float process (float x, float ceilingLin) noexcept
    {
        return softClip (x, ceilingLin);
    }
};

} // namespace abyss
