#pragma once
// Silent Aim — redirects the weapon's hit-transform toward the best target's
// head for one frame while the player fires, then restores the original
// rotation on the next frame.
//
// Mechanism: player::set_transform_rotation writes the aim transform's local
// rotation slot in the native TransformHierarchy. The game reads this to
// compute the bullet ray. No overlay, no uinput — pure memory write.
//
// Frame order:
//  1. silent_aim::begin_frame()     — restore prev frame's written rotation
//  2. silent_aim::offer(...)        — called per-enemy from visuals path (or here)
//  3. silent_aim::commit()          — write best-target rotation into aim transform
//
// Safety: rotation held exactly one frame, always restored. If restore fails
// (process died / layout changed), journal is cleared and the game's physics
// overrides the stale entry within its next tick.

#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../ui/cfg.hpp"
#include "../other/diag.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

namespace silent_aim {

inline const char* status = "off";

// ── Quaternion look-rotation (Unity left-handed, camera looks along +Z) ────
// Returns a world quaternion such that rotate(q, {0,0,1}) == fwd.
static inline Vector4 look_rotation(Vector3 fwd, Vector3 up = {0.f,1.f,0.f}) noexcept {
    float fl = sqrtf(fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z);
    if (fl < 1e-6f) return {0.f,0.f,0.f,1.f};
    fwd.x /= fl; fwd.y /= fl; fwd.z /= fl;

    Vector3 rgt = {up.y*fwd.z - up.z*fwd.y, up.z*fwd.x - up.x*fwd.z, up.x*fwd.y - up.y*fwd.x};
    float rl = sqrtf(rgt.x*rgt.x + rgt.y*rgt.y + rgt.z*rgt.z);
    if (rl < 1e-6f) {
        up = {0.f, 0.f, 1.f};
        rgt = {up.y*fwd.z - up.z*fwd.y, up.z*fwd.x - up.x*fwd.z, up.x*fwd.y - up.y*fwd.x};
        rl = sqrtf(rgt.x*rgt.x + rgt.y*rgt.y + rgt.z*rgt.z);
        if (rl < 1e-6f) return {0.f,0.f,0.f,1.f};
    }
    rgt.x /= rl; rgt.y /= rl; rgt.z /= rl;

    Vector3 nup = {fwd.y*rgt.z - fwd.z*rgt.y, fwd.z*rgt.x - fwd.x*rgt.z, fwd.x*rgt.y - fwd.y*rgt.x};

    // R[r][c]: col0=rgt, col1=nup, col2=fwd
    float r00=rgt.x, r11=nup.y, r22=fwd.z;
    float tr = r00 + r11 + r22;
    float qx,qy,qz,qw;
    if (tr > 0.f) {
        float s = 2.f*sqrtf(tr+1.f);
        qw = s*0.25f;
        qx = (nup.z - fwd.y) / s;
        qy = (fwd.x - rgt.z) / s;
        qz = (rgt.y - nup.x) / s;
    } else if (r00 > r11 && r00 > r22) {
        float s = 2.f*sqrtf(1.f+r00-r11-r22);
        qw = (nup.z - fwd.y) / s;
        qx = s*0.25f;
        qy = (nup.x + rgt.y) / s;
        qz = (fwd.x + rgt.z) / s;
    } else if (r11 > r22) {
        float s = 2.f*sqrtf(1.f+r11-r00-r22);
        qw = (fwd.x - rgt.z) / s;
        qx = (nup.x + rgt.y) / s;
        qy = s*0.25f;
        qz = (fwd.y + nup.z) / s;
    } else {
        float s = 2.f*sqrtf(1.f+r22-r00-r11);
        qw = (rgt.y - nup.x) / s;
        qx = (fwd.x + rgt.z) / s;
        qy = (fwd.y + nup.z) / s;
        qz = s*0.25f;
    }
    return {qx, qy, qz, qw};
}

// ── Aim transform resolution ─────────────────────────────────────────────────
static inline uint64_t get_aim_native(uint64_t local_player) noexcept {
    if (!plausible(local_player)) return 0;
    const uint64_t wpclass = rpm<uint64_t>(proc::lib + v100::weapon_part_typeinfo);
    if (!wpclass) return 0;
    const uint64_t wp = player::find_part(local_player, wpclass);
    if (!wp) return 0;
    const uint64_t aim_managed = rpm<uint64_t>(wp + 0x40);
    if (!plausible(aim_managed)) return 0;
    return rpm<uint64_t>(aim_managed + v100::unity_cached_ptr);
}

// ── Journal ──────────────────────────────────────────────────────────────────
namespace detail {
    struct journal_t {
        uint64_t  aim_native   = 0;
        Vector4   original_rot = {};   // world rotation stored before we wrote
        bool      active       = false;
        long long wrote_at     = 0;
    };
    inline journal_t journal;
    struct candidate_t {
        bool    valid  = false;
        Vector3 head   = {};     // world-space aim point
        float   dist2  = 1e18f; // screen-space dist² to screen centre
    };
    inline candidate_t best;
}

inline void begin_frame() {
    using namespace detail;
    best = {};
    if (!journal.active) return;

    // Restore: re-write the saved world rotation
    const bool ok = player::set_transform_rotation(journal.aim_native,
                                                   journal.original_rot, true);
    static long long restore_log = 0;
    if (!ok && diag::every(restore_log, 5000))
        diag::log("SILENT_AIM", "restore failed aim_native=0x%llx",
                  (unsigned long long)journal.aim_native);
    journal = {};
}

// Called per-enemy; view/projection come from the ESP camera already selected.
inline void offer(const Vector3& head_world,
                  const matrix&  view,
                  const matrix&  proj) {
    if (!cfg::silent::enabled) return;
    ImVec2 screen;
    if (!world_to_screen(head_world, view, proj, screen)) return;
    const float dx = screen.x - g_sw * .5f;
    const float dy = screen.y - g_sh * .5f;
    const float d2 = dx*dx + dy*dy;
    const float r  = std::clamp(cfg::silent::fov, 10.f, 500.f);
    if (d2 > r*r) return;
    if (!detail::best.valid || d2 < detail::best.dist2)
        detail::best = {true, head_world, d2};
}

inline void commit(uint64_t local_player) {
    using namespace detail;
    if (!cfg::silent::enabled || !best.valid) {
        status = cfg::silent::enabled ? "no target in FOV" : "off";
        return;
    }
    const uint64_t aim_native = get_aim_native(local_player);
    if (!plausible(aim_native)) { status = "aim transform unavailable"; return; }

    // Save original world rotation
    Vector3 aim_pos{};
    Vector4 orig_rot{};
    if (!player::decode_transform_pose(aim_native, aim_pos, orig_rot)) {
        status = "pose read failed"; return;
    }

    // Build target world rotation
    Vector3 dir = {best.head.x - aim_pos.x,
                   best.head.y - aim_pos.y,
                   best.head.z - aim_pos.z};
    const Vector4 target_rot = look_rotation(dir);

    if (!player::set_transform_rotation(aim_native, target_rot, true)) {
        status = "write failed"; return;
    }

    journal = {aim_native, orig_rot, true, diag::now_ms()};
    status = "active";
}

// Self-contained tick — called from main.cpp, internally discovers players.
inline void tick(bool run, const matrix& view, const matrix& proj) {
    begin_frame();
    if (!run || !cfg::silent::enabled) { status = "off"; return; }

    uint64_t local = 0;
    for (uint64_t p : players()) if (player::local(p)) { local = p; break; }
    if (!local) { status = "no local player"; return; }

    const int local_team = player::team(local);
    const Vector3 local_pos = player::position(local);
    const float heights[] = {1.67f, 1.2f, 0.6f};
    const float h = heights[std::clamp(cfg::silent::bone, 0, 2)];

    for (uint64_t p : players()) {
        if (!p || p == local) continue;
        if (player::team(p) == local_team) continue;
        if (player::health(p) <= 0) continue;
        const Vector3 pos = player::position(p);
        if (pos.x == 0.f && pos.y == 0.f && pos.z == 0.f) continue;
        if (calculate_distance(pos, local_pos) > cfg::silent::max_dist) continue;
        offer(Vector3(pos.x, pos.y + h, pos.z), view, proj);
    }
    commit(local);

    static long long log_at = 0;
    if (diag::every(log_at, 5000))
        diag::log("SILENT_AIM", "status=%s fov=%.0f bone=%d", status,
                  cfg::silent::fov, cfg::silent::bone);
}

} // namespace silent_aim
