#pragma once
#include <cmath>
#include <algorithm>
namespace bomb_clock {
struct reading { bool valid{}; float remaining{},duration{}; };
// Same clock and formula as qik.edjp (0xA6E8A24). No guessed round duration.
inline reading evaluate(float planted_at,float duration,float world_time) {
    if (!std::isfinite(planted_at) || !std::isfinite(duration) || !std::isfinite(world_time) ||
        planted_at<0.f || world_time<0.f || duration<=0.f || duration>600.f) return {};
    const float elapsed=world_time-planted_at;
    if (elapsed<-.25f || elapsed>duration+1.f) return {};
    return {true,std::clamp(duration-std::max(0.f,elapsed),0.f,duration),duration};
}
}
