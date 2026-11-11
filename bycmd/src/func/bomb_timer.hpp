#pragma once
#include "imgui.h"
#include "../game/bomb_clock.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bomb_timer {
// The last ten seconds take priority over elapsed-time colors.
enum class phase { unknown, early, middle, critical };
inline phase classify(const bomb_clock::reading& c) {
    if (!c.valid || !std::isfinite(c.remaining) || !std::isfinite(c.duration) ||
        c.duration<=0.f || c.remaining<0.f || c.remaining>c.duration) return phase::unknown;
    if (c.remaining<=10.f) return phase::critical;
    if (c.duration-c.remaining>=10.f) return phase::middle;
    return phase::early;
}
inline ImU32 phase_color(phase p) {
    return p==phase::unknown ? IM_COL32(150,157,166,110) : IM_COL32(255,89,96,148);
}
// A single world-positioned badge; invalid timing never produces a fake countdown.
inline void marker(ImDrawList* d, ImVec2 at, ImVec2 screen, const bomb_clock::reading& clock) {
    if (!d || !std::isfinite(at.x) || !std::isfinite(at.y) || !std::isfinite(screen.x) || !std::isfinite(screen.y) || screen.x<100.f || screen.y<100.f) return;
    const float s=std::clamp(screen.y/720.f,.7f,1.5f);
    const float r=26.f*s, thickness=3.6f*s;
    const phase state=classify(clock); const bool ok=state!=phase::unknown;
    const float ratio=ok?std::clamp(clock.remaining/clock.duration,0.f,1.f):0.f;
    const ImU32 accent=phase_color(state);
    d->AddCircleFilled(at,r+3.f*s,IM_COL32(9,8,14,55),48);
    d->AddCircle(at,r,IM_COL32(255,255,255,25),48,thickness);
    if(ok && ratio>0.f) {
        constexpr float pi=3.14159265358979323846f;
        const float begin=-pi*.5f,end=begin+2.f*pi*ratio;
        d->PathArcTo(at,r,begin,end,64); d->PathStroke(accent,0,thickness);
        const ImVec2 p0(at.x+std::cos(begin)*r,at.y+std::sin(begin)*r);
        const ImVec2 p1(at.x+std::cos(end)*r,at.y+std::sin(end)*r);
        d->AddCircleFilled(p0,thickness*.5f,accent,12);
        d->AddCircleFilled(p1,thickness*.5f,accent,12);
    }
    const auto P=[&](float x,float y){return ImVec2(at.x+x*s,at.y+y*s);};
    // The compact C4/keypad symbol remains legible even without an icon font.
    d->AddRectFilled(P(-10,-6),P(10,6),IM_COL32(242,242,246,145),1.4f*s);
    d->AddRectFilled(P(0,-4.5f),P(8,4.5f),IM_COL32(45,42,48,145),.7f*s);
    d->AddRectFilled(P(1,-3.5f),P(7,-1.8f),IM_COL32(224,224,229,145),.4f*s);
    for(int row=0;row<2;row++)for(int col=0;col<3;col++)
        d->AddRectFilled(P(1.f+col*2.f,.2f+row*1.9f),P(2.1f+col*2.f,1.3f+row*1.9f),IM_COL32(234,234,238,145),.2f*s);
    ImFont* f=ImGui::GetFont(); d->AddText(f,7.1f*s,P(-9, -2.8f),IM_COL32(27,25,30,145),"C4");
}
}
