#pragma once
// Visual prototype only. This file never injects input or writes game memory.
#include "../ui/cfg.hpp"
#include "../ui/theme/theme.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
namespace legit_overlay {
inline void render() {
    if(!cfg::style::overlay_enabled || !std::isfinite(g_sw) || !std::isfinite(g_sh) || g_sw<100 || g_sh<100)return;
    if(cfg::legit::draw_fov) {
        float radius=std::isfinite(cfg::legit::fov)?std::clamp(cfg::legit::fov,10.f,360.f):120.f;
        ImGui::GetBackgroundDrawList()->AddCircle({g_sw*.5f,g_sh*.5f},radius,ImGui::ColorConvertFloat4ToU32(cfg::legit::fov_color),96,1.5f);
    }
    if(!cfg::legit::edit_position)return;
    auto& io=ImGui::GetIO();
    float nx=std::isfinite(cfg::legit::point_x)?std::clamp(cfg::legit::point_x,0.f,1.f):.7f;
    float ny=std::isfinite(cfg::legit::point_y)?std::clamp(cfg::legit::point_y,0.f,1.f):.65f;
    constexpr float r=24.f;
    ImVec2 c{std::clamp(nx*g_sw,r,g_sw-r),std::clamp(ny*g_sh,r,g_sh-r)};
    ImGui::SetNextWindowPos({c.x-r,c.y-r},ImGuiCond_Always);
    ImGui::SetNextWindowSize({r*2,r*2},ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,{0,0,0,0});
    const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoBringToFrontOnFocus;
    if(ImGui::Begin("##legit_point",nullptr,flags)) {
        ImGui::InvisibleButton("##drag_point",{r*2,r*2});
        if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0,0.f)) {
            c.x=std::clamp(c.x+io.MouseDelta.x,r,g_sw-r);c.y=std::clamp(c.y+io.MouseDelta.y,r,g_sh-r);
            cfg::legit::point_x=c.x/g_sw;cfg::legit::point_y=c.y/g_sh;
        }
        auto* d=ImGui::GetWindowDrawList();ImVec2 drawn=ImGui::GetWindowPos();drawn.x+=r;drawn.y+=r;
        d->AddCircleFilled(drawn,r-2,IM_COL32(5,5,7,230),32);
        d->AddLine({drawn.x-10,drawn.y},{drawn.x+10,drawn.y},IM_COL32(255,255,255,255),3.f);
        d->AddLine({drawn.x,drawn.y-10},{drawn.x,drawn.y+10},IM_COL32(255,255,255,255),3.f);
    }
    ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar();
}
}
