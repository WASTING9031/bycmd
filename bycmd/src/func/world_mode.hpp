#pragma once
// World Mode: three looks (Winter, Winter Realistic, Glass) built only from parts that exist:
//  * the real Fog and Sky writers, fed a preset ONLY where the user's own Fog / Sky switch is
//    off. preset_guard substitutes the values for the duration of fog::tick / sky::tick and puts
//    the menu values back immediately, so the user's own settings are never changed;
//  * a full-screen grade on our overlay (screen space: it also tints the game HUD);
//  * for the winter looks, the overlay snow layer (particles::g_extra_snow).
// HONEST LIMITS: no texture / material / lighting replacement and no snow on surfaces.
// Fog and Sky writes are device-unverified like the Fog / Sky features themselves.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "../ui/cfg.hpp"
#include "particles.hpp"

namespace world_mode {
inline char g_status[160] = "Off. A look feeds the Fog / Sky writers a preset and adds an overlay grade.";
inline bool g_fog = false, g_sky = false;         // preset fed to the Fog / Sky writer this frame
inline const char* const names[] = {"Off", "Winter", "Winter Realistic", "Glass"};

struct preset { int fog_mode; ImVec4 fog; float start, end, density; ImVec4 sky; float snow; };
inline preset look(int m, float k) {
    k = std::isfinite(k) ? std::clamp(k, 0.f, 1.f) : .65f;
    if (m == 1) return {0, {.86f, .90f, .96f, 1}, 6.f + (1 - k) * 30.f, 90.f + (1 - k) * 260.f, .01f, {.80f, .87f, .96f, 1}, 1.f};
    if (m == 2) return {1, {.70f, .75f, .82f, 1}, 0.f, 1.f, .006f + .03f * k, {.60f, .66f, .75f, 1}, 1.6f};
    return {0, {.84f, .93f, 1.f, 1}, 40.f + (1 - k) * 60.f, 260.f + (1 - k) * 400.f, .01f, {.74f, .90f, 1.f, 1}, 0.f};
}
inline int current() {
    const int m = cfg::world_mode::mode;
    return cfg::style::overlay_enabled && m >= 1 && m <= 3 ? m : 0;
}

struct preset_guard {
    bool fog = false, sky = false;
    bool fog_on = false; int fog_mode = 0; ImVec4 fog_color{}; float fog_start = 0, fog_end = 0, fog_density = 0;
    bool sky_on = false; ImVec4 sky_color{};
    preset_guard() {
        g_fog = g_sky = false;
        const int m = current();
        if (!m) return;
        const preset p = look(m, cfg::world_mode::strength);
        if (!cfg::fog::enabled) {
            fog = true; fog_on = cfg::fog::enabled; fog_mode = cfg::fog::mode; fog_color = cfg::fog::color;
            fog_start = cfg::fog::start; fog_end = cfg::fog::end; fog_density = cfg::fog::density;
            cfg::fog::enabled = true; cfg::fog::mode = p.fog_mode; cfg::fog::color = p.fog;
            cfg::fog::start = p.start; cfg::fog::end = std::max(p.end, p.start + 1.f); cfg::fog::density = p.density;
        }
        if (!cfg::world::sky) {
            sky = true; sky_on = cfg::world::sky; sky_color = cfg::world::sky_col;
            cfg::world::sky = true; cfg::world::sky_col = p.sky;
        }
        g_fog = fog; g_sky = sky;
    }
    ~preset_guard() {
        if (fog) { cfg::fog::enabled = fog_on; cfg::fog::mode = fog_mode; cfg::fog::color = fog_color;
                   cfg::fog::start = fog_start; cfg::fog::end = fog_end; cfg::fog::density = fog_density; }
        if (sky) { cfg::world::sky = sky_on; cfg::world::sky_col = sky_color; }
    }
    preset_guard(const preset_guard&) = delete; preset_guard& operator=(const preset_guard&) = delete;
};

// Soft edge light: four linear gradient strips (no stepped rings).
inline void edge(ImDrawList* d, ImVec2 a, ImVec2 b, float width, ImVec4 c) {
    const ImU32 on = ImGui::ColorConvertFloat4ToU32(c), off = ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, 0.f));
    d->AddRectFilledMultiColor(a, {b.x, a.y + width}, on, on, off, off);
    d->AddRectFilledMultiColor({a.x, b.y - width}, b, off, off, on, on);
    d->AddRectFilledMultiColor(a, {a.x + width, b.y}, on, off, off, on);
    d->AddRectFilledMultiColor({b.x - width, a.y}, b, off, on, on, off);
}
// Overlay grade between a and b (the screen, or a sample box). m: 1 Winter, 2 Winter Realistic, 3 Glass.
inline void draw(ImDrawList* d, ImVec2 a, ImVec2 b, int m, float k) {
    if (!d || m < 1 || m > 3) return;
    k = std::isfinite(k) ? std::clamp(k, 0.f, 1.f) : .65f;
    auto col = [](float r, float g, float bl, float al) { return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, bl, std::clamp(al, 0.f, 1.f))); };
    const float w = b.x - a.x, h = b.y - a.y, side = std::min(w, h);
    if (w <= 0 || h <= 0) return;
    if (m == 1) {           // Winter: cool blue-white wash, soft frost at the edges
        d->AddRectFilledMultiColor(a, b, col(.82f, .89f, .98f, .16f * k), col(.82f, .89f, .98f, .16f * k),
                                         col(.66f, .76f, .92f, .26f * k), col(.66f, .76f, .92f, .26f * k));
        edge(d, a, b, side * .16f, ImVec4(.90f, .95f, 1.f, .26f * k));
    } else if (m == 2) {    // Winter Realistic: muted, colder grade, overcast top light, heavier frost
        d->AddRectFilled(a, b, col(.52f, .55f, .60f, .10f * k));                        // lowers saturation
        d->AddRectFilledMultiColor(a, b, col(.60f, .72f, .90f, .16f * k), col(.60f, .72f, .90f, .16f * k),
                                         col(.30f, .42f, .64f, .28f * k), col(.30f, .42f, .64f, .28f * k));
        d->AddRectFilledMultiColor(a, {b.x, a.y + h * .45f}, col(.94f, .97f, 1.f, .16f * k), col(.94f, .97f, 1.f, .16f * k),
                                                             col(.94f, .97f, 1.f, 0.f), col(.94f, .97f, 1.f, 0.f));
        edge(d, a, b, side * .22f, ImVec4(.86f, .92f, 1.f, .34f * k));
    } else {                // Glass: high-key cyan-white wash, faint diagonal gloss, bright rim
        d->AddRectFilled(a, b, col(.80f, .90f, .98f, .14f * k));
        d->AddQuadFilled({a.x, a.y}, {a.x + w * .46f, a.y}, {a.x + w * .16f, b.y}, {a.x, b.y}, col(1.f, 1.f, 1.f, .045f * k));
        d->AddQuadFilled({a.x + w * .58f, a.y}, {a.x + w * .66f, a.y}, {a.x + w * .36f, b.y}, {a.x + w * .28f, b.y}, col(1.f, 1.f, 1.f, .05f * k));
        edge(d, a, b, side * .10f, ImVec4(1.f, 1.f, 1.f, .16f * k));
        d->AddRect({a.x + 1.5f, a.y + 1.5f}, {b.x - 1.5f, b.y - 1.5f}, col(1.f, 1.f, 1.f, .22f * k), 0.f, 0, 2.f);
    }
}
// No game / no overlay frame: nothing drawn, no World Mode snow, status says why.
inline void idle() {
    const int m = current();
    particles::g_extra_snow = 0.f;
    if (!m) std::snprintf(g_status, sizeof g_status, "Off. A look feeds the Fog / Sky writers a preset and adds an overlay grade.");
    else std::snprintf(g_status, sizeof g_status, "%s: waiting for game", names[m]);
}
inline void draw() {
    const int m = current();
    particles::g_extra_snow = m ? look(m, cfg::world_mode::strength).snow : 0.f;
    if (!m) { std::snprintf(g_status, sizeof g_status, "Off. A look feeds the Fog / Sky writers a preset and adds an overlay grade."); return; }
    if (g_sw > 0 && g_sh > 0) draw(ImGui::GetBackgroundDrawList(), ImVec2(0, 0), ImVec2(g_sw, g_sh), m, cfg::world_mode::strength);
    std::snprintf(g_status, sizeof g_status, "%s: %s, %s, overlay grade%s (device-unverified)", names[m],
                  g_fog ? "fog preset" : "your Fog kept", g_sky ? "sky preset" : "your Sky kept", m < 3 ? " + snow" : "");
}
} // namespace world_mode
