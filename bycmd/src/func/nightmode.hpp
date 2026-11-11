#pragma once
// Screen-space grade: it darkens the game HUD as well as the scene; it does not edit lighting.
#include "imgui.h"
#include "../ui/cfg.hpp"
#include "../ui/theme/theme.hpp"
#include <algorithm>
#include <cmath>
namespace nightmode {
inline void draw() {
    if (!cfg::style::overlay_enabled || !cfg::misc::nightmode) return;
    const ImVec2 size(g_sw,g_sh);
    if (!std::isfinite(size.x)||!std::isfinite(size.y)||size.x<=0||size.y<=0) return;
    const float op=std::isfinite(cfg::misc::nightmode_opacity)?std::clamp(cfg::misc::nightmode_opacity,0.f,.88f):0.f;
    const float edge=std::isfinite(cfg::misc::nightmode_edges)?std::clamp(cfg::misc::nightmode_edges,0.f,.5f):0.f;
    ImVec4 c=cfg::misc::nightmode_color; c.w=op;
    ImDrawList* d=ImGui::GetBackgroundDrawList();
    d->AddRectFilled({0,0},size,ImGui::ColorConvertFloat4ToU32(c));
    if(edge>0.f) {
        const float h=size.y*.25f;
        const ImU32 dark=ImGui::ColorConvertFloat4ToU32(ImVec4(0,0,0,edge)), clear=IM_COL32(0,0,0,0);
        d->AddRectFilledMultiColor({0,0},{size.x,h},dark,dark,clear,clear);
        d->AddRectFilledMultiColor({0,size.y-h},size,clear,clear,dark,dark);
    }
}
}
