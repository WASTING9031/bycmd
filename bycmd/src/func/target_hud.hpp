#pragma once
// Target HUD: one compact panel for the enemy nearest the crosshair. It shows only values the
// ESP already reads (name, HP, armor / helmet, active weapon, money, distance) and is shared by
// the overlay and the menu sample, like esp_style::player().
#include "esp_style.hpp"
#include <string>
#include <limits>

namespace target_hud {
inline const char* status = "off";
struct info {
    std::string name;
    int hp = 100, max_hp = 100, armor = -1; bool helmet = false;
    bool has_weapon = false; uint8_t weapon = 0;
    bool has_money = false; unsigned money = 0;
    float distance = -1.f;
};
// Distance to the projected box, or to a projected target point when a full box
// cannot be constructed. Infinity means no usable on-screen projection.
inline float target_distance(ImVec2 center, ImVec2 mn, ImVec2 mx, bool box_ok, ImVec2 fallback, bool fallback_ok) {
    if(box_ok && std::isfinite(mn.x) && std::isfinite(mn.y) && std::isfinite(mx.x) && std::isfinite(mx.y) && mn.x<=mx.x && mn.y<=mx.y){
        const float dx=std::max({mn.x-center.x,0.f,center.x-mx.x});
        const float dy=std::max({mn.y-center.y,0.f,center.y-mx.y});return std::sqrt(dx*dx+dy*dy);
    }
    if(fallback_ok && std::isfinite(fallback.x) && std::isfinite(fallback.y))return std::hypot(fallback.x-center.x,fallback.y-center.y);
    return std::numeric_limits<float>::infinity();
}
inline ImVec2 panel_size(float u) { return ImVec2(std::round(280.f * u), std::round(94.f * u)); }
namespace detail {
inline void txt(ImDrawList* d, ImFont* f, float size, ImVec2 p, ImU32 c, const char* s) {
    p = ImVec2(std::round(p.x), std::round(p.y));
    d->AddText(f, size, ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, int(((c >> IM_COL32_A_SHIFT) & 0xFF) * .6f)), s);
    d->AddText(f, size, p, c, s);
}
inline void bar(ImDrawList* d, ImVec2 a, ImVec2 b, float frac, ImU32 fill, ImU32 back) {
    const float r = (b.y - a.y) * .5f;
    d->AddRectFilled(a, b, back, r);
    frac = std::clamp(frac, 0.f, 1.f);
    if (frac > 0.f) d->AddRectFilled(a, ImVec2(std::max(a.x + r * 2.f, a.x + (b.x - a.x) * frac), b.y), fill, r);
}
} // namespace detail

// `hp_shown` follows the real HP smoothly; `hp_ghost` trails it after damage.
inline void panel(ImDrawList* d, ImVec2 p, float u, const info& t, float hp_shown, float hp_ghost, float a) {
    using namespace detail;
    if (!d || a <= 0.f || !std::isfinite(u) || u <= 0.f) return;
    a = std::clamp(a, 0.f, 1.f);
    const ImVec2 z = panel_size(u), q(p.x + z.x, p.y + z.y);
    const float r = std::round(11.f * u);
    auto c = [&](float R, float G, float B, float A) { return esp_style::color(ImVec4(R, G, B, A), a); };
    d->AddRectFilled(ImVec2(p.x + 2 * u, p.y + 4 * u), ImVec2(q.x + 2 * u, q.y + 4 * u), c(0, 0, 0, .30f), r);
    d->AddRectFilled(p, q, c(.035f, .035f, .045f, .86f), r);
    d->AddRectFilled(ImVec2(p.x + 1, p.y + 1), ImVec2(q.x - 1, p.y + z.y * .42f), c(1, 1, 1, .035f), r, ImDrawFlags_RoundCornersTop);
    d->AddRect(p, q, c(1, 1, 1, .09f), r, 0, 1.f);
    d->AddRectFilled(ImVec2(p.x + 8 * u, p.y + 13 * u), ImVec2(p.x + 11 * u, q.y - 13 * u), esp_style::color(cfg::misc::target_hud_col, a), 2.f * u);

    ImFont* f = esp_fonts_ui::get(esp_fonts_ui::f_montserrat);
    const float x0 = p.x + 22 * u, x1 = q.x - 14 * u;
    const ImU32 white = c(.96f, .96f, .97f, 1), dim = c(.62f, .62f, .67f, 1);
    char s[32];
    // name + distance
    const float ns = std::round(17 * u), ds = std::round(13 * u);
    float dist_w = 0.f;
    if (t.distance >= 0.f) {
        std::snprintf(s, sizeof s, "%.0f m", t.distance); dist_w = esp_style::measure(f, ds, s).x;
        txt(d, f, ds, ImVec2(x1 - dist_w, p.y + 14 * u), dim, s);
    }
    const std::string nm = esp_style::shorten(f, ns, t.name.empty() ? "Enemy" : t.name.c_str(), x1 - x0 - dist_w - 10 * u);
    txt(d, f, ns, ImVec2(x0, p.y + 11 * u), white, nm.c_str());
    // HP
    const int max_hp = t.max_hp > 0 ? t.max_hp : 100;
    const float ratio = std::clamp(hp_shown / float(max_hp), 0.f, 1.f);
    ImVec4 hc = cfg::esp::health_col;
    if (cfg::esp::health_gradient) hc = ImVec4(1.f + (hc.x - 1.f) * ratio, .2f + (hc.y - .2f) * ratio, .15f + (hc.z - .15f) * ratio, hc.w);
    const float bx1 = x1 - 58 * u, hy = p.y + 39 * u, hh = std::max(3.f, std::round(6 * u));
    bar(d, ImVec2(x0, hy), ImVec2(bx1, hy + hh), std::clamp(hp_ghost / float(max_hp), 0.f, 1.f), c(1, 1, 1, .32f), c(1, 1, 1, .08f));
    if (ratio > 0.f) bar(d, ImVec2(x0, hy), ImVec2(bx1, hy + hh), ratio, esp_style::color(hc, a), 0);
    const float vs = std::round(14 * u);
    std::snprintf(s, sizeof s, "%d", std::max(0, t.hp));
    const float hw = esp_style::measure(f, vs, s).x, uw = esp_style::measure(f, std::round(12 * u), " HP").x;
    txt(d, f, vs, ImVec2(x1 - uw - hw, hy + hh * .5f - vs * .55f), white, s);
    txt(d, f, std::round(12 * u), ImVec2(x1 - uw, hy + hh * .5f - 12 * u * .5f), dim, " HP");
    // Armor (only when read)
    const float ay = hy + hh + 9 * u, ah = std::max(2.f, std::round(4 * u));
    if (t.armor >= 0) {
        bar(d, ImVec2(x0, ay), ImVec2(bx1, ay + ah), t.armor / 100.f, esp_style::color(cfg::esp::armor_col, a), c(1, 1, 1, .08f));
        std::snprintf(s, sizeof s, "%d %s", t.armor, t.helmet ? "HK" : "K");
        const float sw = esp_style::measure(f, std::round(12 * u), s).x;
        txt(d, f, std::round(12 * u), ImVec2(x1 - sw, ay + ah * .5f - 12 * u * .55f), dim, s);
    }
    // weapon + money
    const float wy = p.y + 67 * u, ih = std::round(13 * u);
    float wx = x0;
    const weapon_catalog::definition* w = t.has_weapon ? weapon_catalog::lookup(t.weapon) : nullptr;
    if (w && w->kind != weapon_catalog::icon_kind::none) {
        const float iw = weapon_icons::draw_at_height(d, w, ImVec2(std::round(wx), std::round(wy)), ih, c(.92f, .92f, .94f, .95f));
        wx += iw + 8 * u;
        char up[40]; size_t i = 0; for (; w->name[i] && i < sizeof up - 1; ++i) up[i] = (w->name[i] >= 'a' && w->name[i] <= 'z') ? char(w->name[i] - 32) : w->name[i]; up[i] = 0;
        txt(d, f, std::round(12 * u), ImVec2(wx, wy + ih * .5f - 12 * u * .55f), dim, up);
    }
    if (t.has_money) {
        std::snprintf(s, sizeof s, "$%u", t.money);
        const float mw = esp_style::measure(f, std::round(13 * u), s).x;
        txt(d, f, std::round(13 * u), ImVec2(x1 - mw, wy + ih * .5f - 13 * u * .55f), esp_style::color(cfg::esp::money_col, a), s);
    }
}

namespace detail {
inline uint64_t object = 0; inline info last{};
inline float fade = 0.f, hp = 100.f, ghost = 100.f; inline int prev_hp = 100;
inline long long seen = 0, hurt = 0;
}
inline void reset() { using namespace detail; object = 0; last = {}; fade = 0; hp = ghost = 100; prev_hp = 100; seen = hurt = 0; }
inline ImVec2 anchor(ImVec2 screen, ImVec2 z) {
    switch (cfg::misc::target_hud_pos) {
    case 1: return ImVec2(std::round(screen.x * .025f), std::round(screen.y * .5f - z.y * .5f));
    case 2: return ImVec2(std::round(screen.x * .5f - z.x * .5f), std::round(screen.y * .12f));
    default: return ImVec2(std::round(screen.x * .5f - z.x * .5f), std::round(screen.y * .5f + screen.y * .11f));
    }
}
// Once per overlay frame. `t` is null when no enemy is near the crosshair; the last
// target lingers for 450 ms and fades, so the panel does not flicker between frames.
inline void frame(ImDrawList* d, ImVec2 screen, uint64_t target, const info* t, long long now) {
    using namespace detail;
    const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.f, .1f);
    if (t) {
        if (target != object) { object = target; hp = ghost = float(t->hp); prev_hp = t->hp; hurt = 0; }
        if (t->hp < prev_hp) hurt = now;
        prev_hp = t->hp; last = *t; seen = now;
    }
    const bool live = object && (t || now - seen < 450);
    fade += ((live ? 1.f : 0.f) - fade) * (1.f - std::exp(-16.f * dt));
    if (!live && fade < .02f) { object = 0; fade = 0.f; return; }
    hp += (float(last.hp) - hp) * (1.f - std::exp(-18.f * dt));
    if (now - hurt > 260) ghost += (float(last.hp) - ghost) * (1.f - std::exp(-7.f * dt));
    ghost = std::max(ghost, hp);
    const float u = std::clamp(screen.y / 1080.f, .8f, 2.2f); // >= 9 px labels on 720p phones
    panel(d, anchor(screen, panel_size(u)), u, last, hp, ghost, fade);
}
// Continue the linger/fade even when player or camera discovery fails.
inline void waiting(ImDrawList* d, ImVec2 screen, long long now) { frame(d, screen, 0, nullptr, now); }
} // namespace target_hud
