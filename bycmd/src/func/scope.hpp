#pragma once
// Custom scope: an overlay reticle while the local weapon is scoped, and the game's own sniper
// sight hidden while the option is on.
//  * Scoped state = PlayerAnimationData.WeaponCurAimState (+0xF1, enum spi: 0 None, 1 InScope,
//    2 SwitchScope, 3 ScopeReload) of the local first-person player. It is read (never written)
//    by camera_overrides::tick, which already resolves that object, and published here.
//  * Sight: SniperScopeComponent.qwaa (lib+0xA7ABFE4) sets the sight CanvasGroup alpha to 1.0 on
//    every scoped frame. One word is swapped:
//        lib+0xA7AC004  fmov s0,#1.0 (0x1E2E1000)  ->  movi d0,#0 (0x2F00E400)
//    i.e. the same alpha 0 the game's own hide path writes (nvoh, lib+0xA7AFF08). Prologue and
//    neighbour words are checked first; the original word is written back on disable, pause and
//    exit. Device-unverified.
#include <algorithm>
#include <cmath>
#include "imgui.h"
#include "../game/game.hpp"
#include "../game/code_patch.hpp"
#include "../ui/cfg.hpp"
#include "../ui/theme/theme.hpp"

namespace scope {
inline const char* g_status = "off";
inline int g_aim_state = -1;                      // -1 unknown
inline bool scoped() { return g_aim_state == 1 || g_aim_state == 2; }

namespace detail {
inline constexpr uint64_t site = 0xA7AC004;
inline constexpr uint32_t original = 0x1E2E1000u, hidden = 0x2F00E400u;
inline int pid = -1; inline uint64_t lib = 0; inline bool owned = false; inline long long retry = 0;
inline bool build_ok(uint64_t base) {
    return rpm<uint32_t>(base + 0xA7ABFE4) == 0x6DBC2BEBu && rpm<uint32_t>(base + 0xA7AC000) == 0x1E204008u &&
           rpm<uint32_t>(base + 0xA7AC008) == 0xAA1F03E1u && rpm<uint32_t>(base + 0xA7AFF08) == 0x2F00E400u;
}
inline bool restore() {
    if (!owned) return true;
    if (pid != proc::pid || lib != proc::lib || proc::pid <= 0) { owned = false; return true; }
    const uint32_t now = rpm<uint32_t>(lib + site);
    if (now == 0 || now == original) { owned = false; return true; }        // unreadable (process gone) or already original
    if (now == hidden && code_patch::swap(lib + site, hidden, original)) { owned = false; return true; }
    return false;
}
} // namespace detail

inline void tick(bool run) {
    using namespace detail;
    if (pid != proc::pid || lib != proc::lib) { owned = false; pid = proc::pid; lib = proc::lib; retry = 0; }
    if (!run || !cfg::scope::enabled || proc::pid <= 0 || !proc::lib) {
        g_status = restore() ? (cfg::scope::enabled ? "waiting for game" : "off") : "restore pending"; return;
    }
    if (!build_ok(lib)) { restore(); g_status = "game build differs: sight untouched, reticle only"; return; }
    const uint32_t now_word = rpm<uint32_t>(lib + site);
    if (now_word == hidden) { owned = true; g_status = "game sight hidden; reticle while scoped (device-unverified)"; return; }
    if (now_word != original) { g_status = "unexpected code at sight site: no write, reticle only"; return; }
    const long long now = diag::now_ms();
    if (now < retry) return;
    if (code_patch::swap(lib + site, original, hidden)) { owned = true; g_status = "game sight hidden; reticle while scoped (device-unverified)"; }
    else { retry = now + 3000; g_status = "sight patch failed (needs root /proc/pid/mem write): reticle only"; }
}

// Reticle at `c`; `extent` bounds the full-length Lines style (screen, or a menu sample box).
inline void reticle(ImDrawList* d, ImVec2 c, ImVec2 lo, ImVec2 hi, float alpha) {
    if (!d || alpha <= 0.f) return;
    ImVec4 col = cfg::scope::color; col.w *= std::clamp(alpha, 0.f, 1.f);
    const ImU32 ink = ImGui::ColorConvertFloat4ToU32(col);
    const float t = std::isfinite(cfg::scope::thickness) ? std::clamp(cfg::scope::thickness, .5f, 6.f) : 1.5f;
    const float s = std::isfinite(cfg::scope::size) ? std::clamp(cfg::scope::size, 16.f, 400.f) : 60.f;
    c = ImVec2(std::round(c.x), std::round(c.y));
    switch (cfg::scope::style) {
    case 1: {   // Cross: four arms with a small centre gap
        const float g = std::max(3.f, t * 2.f);
        d->AddLine({c.x - s, c.y}, {c.x - g, c.y}, ink, t); d->AddLine({c.x + g, c.y}, {c.x + s, c.y}, ink, t);
        d->AddLine({c.x, c.y - s}, {c.x, c.y - g}, ink, t); d->AddLine({c.x, c.y + g}, {c.x, c.y + s}, ink, t);
        d->AddCircleFilled(c, std::max(1.2f, t * .8f), ink, 12);
    } break;
    case 2: {   // Circle with centre dot and inner ticks
        d->AddCircle(c, s, ink, 96, t);
        const float k = std::max(6.f, s * .18f);
        d->AddLine({c.x - s, c.y}, {c.x - s + k, c.y}, ink, t); d->AddLine({c.x + s - k, c.y}, {c.x + s, c.y}, ink, t);
        d->AddLine({c.x, c.y - s}, {c.x, c.y - s + k}, ink, t); d->AddLine({c.x, c.y + s - k}, {c.x, c.y + s}, ink, t);
        d->AddCircleFilled(c, std::max(1.5f, t), ink, 12);
    } break;
    case 3:     // Dot
        d->AddCircleFilled(c, std::max(1.5f, t * 1.6f), ink, 16);
        break;
    default:    // Lines: full-length cross lines, like a sight without its overlay
        d->AddLine({lo.x, c.y}, {hi.x, c.y}, ink, t);
        d->AddLine({c.x, lo.y}, {c.x, hi.y}, ink, t);
        break;
    }
}

// Overlay: called every frame from the main loop, independent of the ESP camera.
inline void draw() {
    static float fade = 0.f;
    const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.f, .1f);
    const bool show = cfg::style::overlay_enabled && cfg::scope::enabled && scoped();
    fade += ((show ? 1.f : 0.f) - fade) * (1.f - std::exp(-24.f * dt));
    if (!show && fade < .02f) { fade = 0.f; return; }
    if (g_sw <= 0.f || g_sh <= 0.f) return;
    reticle(ImGui::GetBackgroundDrawList(), ImVec2(g_sw * .5f, g_sh * .5f), ImVec2(0, 0), ImVec2(g_sw, g_sh), fade);
}
} // namespace scope
