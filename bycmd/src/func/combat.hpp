#pragma once
// Local-shot based helpers (hit log, bullet tracers), footsteps and scope indicator.
// HONEST LIMITS: the game's hit/raycast results are not read (no RE for them). A "shot" is a decrease
// of the local magazine; a "hit" is an enemy HP drop within 450 ms after a local shot (a teammate's
// damage in that window is attributed to you). Bullet tracers end at the enemy closest to the aim ray,
// otherwise at 80 m - walls are not known. Footsteps = enemy moving faster than walking speed.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include "imgui.h"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../game/weapon_visual_data.hpp"
#include "../game/ammo_reader.hpp"
#include "../ui/cfg.hpp"
#include "esp_style.hpp"
#include "scope.hpp"

namespace combat {
struct target { uint64_t object; uint32_t id; Vector3 aim; Vector3 foot; float radius; };

namespace detail {
inline uint8_t last_raw = 0; inline int last_mag = -1; inline long long last_shot = -100000; inline int shots = 0;
struct hit { std::string name, zone; int dmg; bool kill; long long at; };
struct aim_zone { const char* zone; long long at; };
inline std::unordered_map<uint64_t, aim_zone> zones;   // object -> body part under the crosshair at the last shot
inline std::deque<hit> hits;
struct damage {int amount;bool kill;long long at;};
inline std::deque<damage> damage_hits;
struct hp_state { int hp; long long seen; };
inline std::unordered_map<uint64_t, hp_state> hp_seen;
struct bullet { Vector3 a, b; long long at; };
inline std::vector<bullet> bullets;
struct step_track { Vector3 pos; long long at; long long last_step; };
inline std::unordered_map<uint64_t, step_track> steps_track;
struct ring { Vector3 pos; long long at; };
inline std::vector<ring> rings;
inline float base_fov = 0.f; inline long long base_at = 0;
}

inline bool firearm(uint8_t raw) {
    const auto* d = weapon_catalog::lookup(raw);
    if (!d) return false;
    using K = weapon_catalog::icon_kind;
    switch (d->kind) { case K::pistol: case K::smg: case K::rifle: case K::sniper: case K::shotgun: case K::machinegun: return true; default: return false; }
}
inline uint8_t local_weapon = 0; inline bool local_weapon_ok = false;

inline Vector3 camera_position(const matrix& view) {
    const float* v = reinterpret_cast<const float*>(&view);
    return Vector3(-(v[0]*v[12] + v[1]*v[13] + v[2]*v[14]), -(v[4]*v[12] + v[5]*v[13] + v[6]*v[14]), -(v[8]*v[12] + v[9]*v[13] + v[10]*v[14]));
}
inline Vector3 camera_forward(const matrix& view) { const float* v = reinterpret_cast<const float*>(&view); return Vector3(-v[2], -v[6], -v[10]); }

// Called once per frame with the local player: detects shots from the magazine counter.
inline void update_local(uint64_t local, const matrix& view, const std::vector<target>& targets, long long now) {
    using namespace detail;
    const auto w = weapon_visual_data::resolve(local);
    local_weapon_ok = w.state == weapon_reader::status::ok; local_weapon = local_weapon_ok ? w.raw : 0;
    const auto ammo = ammo_reader::read(w, [](uint64_t a, void* b, size_t n) { return mem_read(a, b, n); });
    if (!ammo.valid) { last_mag = -1; return; }
    const int mag = int(ammo.magazine);
    if (w.raw == last_raw && last_mag >= 0 && mag < last_mag && last_mag - mag <= 5) {
        last_shot = now; shots += last_mag - mag;
        if (cfg::misc::hit_log) {   // ESTIMATE: body part the crosshair ray crosses (game hit result is not read)
            const Vector3 c = camera_position(view), f = camera_forward(view);
            const float hl = std::sqrt(f.x*f.x + f.z*f.z);
            float best_along = 1e9f; uint64_t obj = 0; const char* zn = nullptr;
            if (hl > .05f) for (const auto& t : targets) {
                const float ox = t.aim.x - c.x, oz = t.aim.z - c.z;
                const float along = (ox*f.x + oz*f.z) / (hl*hl);            // ray parameter at the target column
                if (along <= 0 || along >= best_along) continue;
                const float px = c.x + f.x*along - t.aim.x, pz = c.z + f.z*along - t.aim.z, py = c.y + f.y*along;
                if (std::sqrt(px*px + pz*pz) > .45f) continue;
                const float top = t.aim.y + .12f, bot = t.foot.y, h = top - bot;
                if (h < .3f || py < bot - .05f || py > top + .1f) continue;
                const float k = (py - bot) / h;
                best_along = along; obj = t.object;
                zn = k > .88f ? "head" : k > .80f ? "neck" : k > .60f ? "spine" : k > .47f ? "stomach" : "legs";
            }
            if (obj) zones[obj] = {zn, now};
        }
        if (cfg::misc::bullet_tracer) {
            const Vector3 c = camera_position(view), f = camera_forward(view);
            const float fl = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
            if (fl > .5f) {
                const Vector3 d(f.x/fl, f.y/fl, f.z/fl);
                float best = 80.f;
                for (const auto& t : targets) {
                    const Vector3 o(t.aim.x - c.x, t.aim.y - .35f - c.y, t.aim.z - c.z);
                    const float along = o.x*d.x + o.y*d.y + o.z*d.z;
                    if (along <= 0 || along > best) continue;
                    const Vector3 q(o.x - d.x*along, o.y - d.y*along, o.z - d.z*along);
                    if (std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z) < .65f) best = along;
                }
                bullets.push_back({Vector3(c.x + d.x*.6f, c.y + d.y*.6f - .08f, c.z + d.z*.6f), Vector3(c.x + d.x*best, c.y + d.y*best, c.z + d.z*best), now});
                if (bullets.size() > 48) bullets.erase(bullets.begin());
            }
        }
    }
    last_raw = w.raw; last_mag = mag;
}

// Every enemy (alive or just died) once per frame.
inline void observe_enemy(uint64_t object, uint32_t id, int hp, const char* name, long long now) {
    using namespace detail;
    if(hp<0||hp>200)return;
    const uint64_t key = object ^ (uint64_t(id) << 40);
    auto it = hp_seen.find(key);
    if (it != hp_seen.end() && hp < it->second.hp && it->second.hp > 0 && (cfg::misc::hit_log||cfg::misc::damage_marker) && now>=last_shot && now-last_shot<=450 && now>=it->second.seen && now-it->second.seen<=600) {
        const int dmg = it->second.hp - hp;
        const auto z = zones.find(object);
        const char* zone = (z != zones.end() && now - z->second.at <= 450) ? z->second.zone : "";
        if(cfg::misc::hit_log) hits.push_back({name && *name ? name : "enemy", zone, dmg, hp <= 0, now});
        if(cfg::misc::damage_marker){damage_hits.push_back({dmg,hp<=0,now});if(damage_hits.size()>8)damage_hits.pop_front();}
        if (hits.size() > 5) hits.pop_front();
        diag::log("HITLOG", "%s %s in %s for %d", hp <= 0 ? "killed" : "hitted", name && *name ? name : "enemy", *zone ? zone : "?", dmg);
    }
    hp_seen[key] = {hp, now};
}
inline void observe_steps(uint64_t object, const Vector3& foot, long long now) {
    using namespace detail;
    if (!cfg::misc::footsteps) return;
    auto& s = steps_track[object];
    if (s.at == 0 || now - s.at > 600) { s = {foot, now, 0}; return; }
    const float dt = (now - s.at) / 1000.f;
    if (dt < .12f) return;
    const float dx = foot.x - s.pos.x, dz = foot.z - s.pos.z, dy = std::fabs(foot.y - s.pos.y);
    const float speed = std::sqrt(dx*dx + dz*dz) / dt;
    if (speed > 3.0f && speed < 12.f && dy < .5f && now - s.last_step > 380) { rings.push_back({foot, now}); s.last_step = now; if (rings.size() > 64) rings.erase(rings.begin()); }
    s.pos = foot; s.at = now;
}

inline void polyline3d(ImDrawList* d, const Vector3& a, const Vector3& b, const matrix& v, const matrix& p, ImU32 c, float w) {
    ImVec2 prev; bool has = false;
    for (int i = 0; i <= 24; ++i) {
        const float t = i / 24.f; ImVec2 s;
        const Vector3 q(a.x + (b.x - a.x)*t, a.y + (b.y - a.y)*t, a.z + (b.z - a.z)*t);
        const bool ok = world_to_screen(q, v, p, s) && std::isfinite(s.x) && std::isfinite(s.y) && std::fabs(s.x) < 1e5f && std::fabs(s.y) < 1e5f;
        if (ok && has) d->AddLine(prev, s, c, w);
        prev = s; has = ok;
    }
}
inline void draw_world(ImDrawList* d, const matrix& v, const matrix& p, long long now) {
    using namespace detail;
    const float life = std::clamp(cfg::misc::bullet_time, .2f, 5.f) * 1000.f;
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [&](const bullet& b) { return now - b.at > life; }), bullets.end());
    if (cfg::misc::bullet_tracer) for (const auto& b : bullets) {
        const float k = 1.f - (now - b.at) / life;
        polyline3d(d, b.a, b.b, v, p, esp_style::color(ImVec4(0, 0, 0, .5f), k), 3.f);
        polyline3d(d, b.a, b.b, v, p, esp_style::color(cfg::misc::bullet_col, k), 1.5f);
        ImVec2 e; if (world_to_screen(b.b, v, p, e)) d->AddCircleFilled(e, 2.5f, esp_style::color(cfg::misc::bullet_col, k), 10);
    }
    rings.erase(std::remove_if(rings.begin(), rings.end(), [&](const ring& r) { return now - r.at > 900; }), rings.end());
    if (cfg::misc::footsteps) for (const auto& r : rings) {
        const float t = (now - r.at) / 900.f, rad = .25f + .75f * t;
        ImVec2 pts[24]; int n = 0; bool ok = true;
        for (int i = 0; i < 24; ++i) {
            const float ang = i / 24.f * 6.2831853f;
            if (!world_to_screen(Vector3(r.pos.x + std::cos(ang)*rad, r.pos.y + .02f, r.pos.z + std::sin(ang)*rad), v, p, pts[n])) { ok = false; break; }
            ++n;
        }
        if (ok) d->AddPolyline(pts, n, esp_style::color(cfg::misc::footstep_col, 1.f - t), ImDrawFlags_Closed, 1.5f);
    }
}
inline void reset(){using namespace detail;last_raw=0;last_mag=-1;last_shot=-100000;shots=0;zones.clear();hits.clear();damage_hits.clear();hp_seen.clear();bullets.clear();steps_track.clear();rings.clear();base_fov=0;base_at=0;}
inline void draw_hud(ImDrawList* d, const matrix& projection, ImVec2 screen, long long now) {
    using namespace detail;
    const float life=std::isfinite(cfg::misc::damage_time)?std::clamp(cfg::misc::damage_time,.2f,2.f)*1000.f:800.f;
    while(!damage_hits.empty()&&(now<damage_hits.front().at||now-damage_hits.front().at>life))damage_hits.pop_front();
    if(!cfg::misc::damage_marker)damage_hits.clear();
    if(!damage_hits.empty()){
        const auto& newest=damage_hits.back();const float age=float(now-newest.at),fade=std::clamp(1.f-age/life,0.f,1.f);
        const ImVec2 c(screen.x*.5f,screen.y*.5f);const ImU32 col=esp_style::color(cfg::misc::damage_color,fade);
        for(int x:{-1,1})for(int y:{-1,1}){d->AddLine({c.x+x*5.f,c.y+y*5.f},{c.x+x*12.f,c.y+y*12.f},IM_COL32(0,0,0,int(180*fade)),4);d->AddLine({c.x+x*5.f,c.y+y*5.f},{c.x+x*12.f,c.y+y*12.f},col,2);}
        auto* font=esp_fonts_ui::current();int row=0;
        for(auto it=damage_hits.rbegin();it!=damage_hits.rend()&&row<3;++it,++row){const float k=std::clamp(1.f-float(now-it->at)/life,0.f,1.f);char text[40];std::snprintf(text,sizeof text,"~%d%s",it->amount,it->kill?" KILL":"");auto z=esp_style::measure(font,16,text);esp_style::text(d,font,16,{c.x-z.x*.5f,c.y+20.f+row*19.f-age*.012f},esp_style::color(cfg::misc::damage_color,k),text);}
    }
    if (cfg::misc::hit_log) {
        // "hitted Kerim in spine for 85 damage": dark rounded pill, white text, pink highlights.
        const long long life = 4000;
        while (!hits.empty() && now - hits.front().at > life) hits.pop_front();
        ImFont* f = esp_fonts_ui::get(esp_fonts_ui::f_montserrat);
        const float fs = std::round(std::clamp(screen.x * .0185f, 15.f, 46.f));
        const float padx = std::round(fs * .55f), h = std::round(fs * 1.75f), gap = std::round(fs * .35f), r = std::round(fs * .42f);
        const ImU32 white = IM_COL32(255, 255, 255, 255), pink = IM_COL32(255, 92, 168, 255);
        float y = std::round(screen.y * .035f); const float x0 = std::round(screen.x * .04f);
        for (const auto& hit : hits) {
            const float age = float(now - hit.at);
            const float k = std::clamp(age / 150.f, 0.f, 1.f) * std::clamp((float(life) - age) / 500.f, 0.f, 1.f);
            if (k <= 0.f) continue;
            char dmg[16]; std::snprintf(dmg, sizeof dmg, "%d", hit.dmg);
            struct seg { const char* s; bool hi; };
            seg segs[7]; int n = 0;
            segs[n++] = {hit.kill ? "killed " : "hitted ", false}; segs[n++] = {hit.name.c_str(), true};
            if (!hit.zone.empty()) { segs[n++] = {" in ", false}; segs[n++] = {hit.zone.c_str(), true}; }
            segs[n++] = {" for ", false}; segs[n++] = {dmg, true}; segs[n++] = {" damage", false};
            float w = 0.f; for (int i = 0; i < n; ++i) w += esp_style::measure(f, fs, segs[i].s).x;
            const float slide = (1.f - std::clamp(age / 150.f, 0.f, 1.f)) * -12.f;
            const ImVec2 p0(x0 + slide, y), p1(x0 + slide + w + padx * 2.f, y + h);
            d->AddRectFilled(p0, p1, IM_COL32(8, 8, 12, int(165 * k)), r);
            float tx = std::round(p0.x + padx); const float ty = std::round(p0.y + (h - fs) * .5f);
            for (int i = 0; i < n; ++i) {
                const ImU32 c = segs[i].hi ? pink : white;
                const ImU32 ca = (c & ~IM_COL32_A_MASK) | (ImU32(255 * k) << IM_COL32_A_SHIFT);
                d->AddText(f, fs, ImVec2(tx, ty + 1.f), IM_COL32(0, 0, 0, int(110 * k)), segs[i].s);
                d->AddText(f, fs, ImVec2(tx, ty), ca, segs[i].s);
                d->AddText(f, fs, ImVec2(tx + .5f, ty), ca, segs[i].s);   // faux-bold
                tx += esp_style::measure(f, fs, segs[i].s).x;
            }
            y += h + gap;
        }
    }
    if (cfg::misc::scope_indicator) {
        // Scoped = the local aim state when it is readable (also correct under No zoom),
        // otherwise a >20% narrowing of the live projection FOV.
        const float* pr = reinterpret_cast<const float*>(&projection);
        const float fov = 2.f * std::atan(1.f / std::max(pr[5], .01f)) * 57.29578f;
        bool narrowed = false;
        if (std::isfinite(fov) && fov > 1.f && fov < 170.f) {
            if (fov >= base_fov || now - base_at > 15000) { base_fov = fov; base_at = now; }
            if (fov >= base_fov * .97f) base_at = now;
            narrowed = fov < base_fov * .8f;
        }
        const bool on = scope::g_aim_state >= 0 ? scope::scoped() : narrowed;
        if (on) {
            const ImVec2 c(screen.x * .5f, screen.y * .5f);
            if (!cfg::scope::enabled) {   // Custom scope draws its own reticle
                d->AddLine(ImVec2(0, c.y), ImVec2(screen.x, c.y), IM_COL32(0, 0, 0, 200), 1.f);
                d->AddLine(ImVec2(c.x, 0), ImVec2(c.x, screen.y), IM_COL32(0, 0, 0, 200), 1.f);
            }
            char s[32];
            if (narrowed) std::snprintf(s, sizeof s, "SCOPED x%.1f", base_fov / fov); else std::snprintf(s, sizeof s, "SCOPED");
            ImFont* f = esp_fonts_ui::pixel(); const float ps = esp_style::pixel_size(); const ImVec2 z = esp_style::measure(f, ps, s);
            esp_style::text(d, f, ps, ImVec2(c.x - z.x * .5f, c.y + 40.f), IM_COL32(255, 220, 90, 255), s);
        }
    }
}
inline void forget(const std::vector<uint64_t>& alive) {
    using namespace detail;
    for (auto it = steps_track.begin(); it != steps_track.end();) if (std::find(alive.begin(), alive.end(), it->first) == alive.end()) it = steps_track.erase(it); else ++it;
    const long long now = diag::now_ms();
    for (auto it = hp_seen.begin(); it != hp_seen.end();) if (now - it->second.seen > 10000) it = hp_seen.erase(it); else ++it;
    for (auto it = zones.begin(); it != zones.end();) if (now - it->second.at > 2000) it = zones.erase(it); else ++it;
}
}
