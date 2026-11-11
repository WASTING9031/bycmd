#pragma once
// Triggerbot — fires when the screen centre sits inside any visible enemy's
// projected bounding box. Sends a tap via /dev/uinput at the configured touch
// point; never writes game memory.
//
// Architecture: same uinput Device class as touch_aim, but instead of a
// continuous drag we emit a down→up tap at the fire-button position (the same
// normalized point touch_aim uses). One tap per detection window, with a
// configurable cooldown between shots and a jitter-delay so the timing looks
// human.

#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../ui/cfg.hpp"
#include "../other/diag.hpp"
#include "touch_aim.hpp"   // reuse Device and focused()
#include <cmath>
#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace triggerbot {

inline const char* status = "off";

namespace detail {
    inline bool  inside        = false;    // set by offer() this frame
    inline long long last_tap  = 0;        // last time we sent a tap
    inline long long tap_delay = 0;        // random jitter added before firing
    inline bool  tap_pending   = false;    // waiting out the jitter
    inline long long pending_at= 0;

    // Minimal single-tap device (down then up in one block)
    inline touch_aim::Device dev;
}

// Called per-enemy per-frame.
// Checks whether (cx, cy) = screen centre falls inside the box [x1,y1]–[x2,y2].
inline void offer(float x1, float y1, float x2, float y2) {
    if (!cfg::trigger::enabled) return;
    const float cx = g_sw * .5f, cy = g_sh * .5f;
    if (cx >= x1 && cx <= x2 && cy >= y1 && cy <= y2)
        detail::inside = true;
}

inline void begin_frame() {
    detail::inside = false;
}

inline void tick(bool run, bool menu_open, int orientation) {
    using namespace detail;
    struct LogStatus {
        ~LogStatus() {
            static char last[128] = "";
            static long long at = 0;
            if (cfg::trigger::enabled &&
                (std::strcmp(last, status) || diag::now_ms()-at >= 5000)) {
                std::snprintf(last, sizeof last, "%s", status);
                at = diag::now_ms();
                diag::log("TRIGGERBOT", "status=%s", status);
            }
        }
    } log_guard;

    if (!cfg::trigger::enabled) { dev.release(); status = "off"; return; }

    touch_aim::start_focus_monitor();

    if (!cfg::style::overlay_enabled || !run || menu_open) {
        dev.release(); tap_pending = false;
        status = "paused: game/menu"; return;
    }
    const long long now = diag::now_ms();
    if (!touch_aim::focused(now)) {
        dev.release(); tap_pending = false;
        status = "waiting for game focus"; return;
    }
    if (!inside) {
        dev.release(); tap_pending = false;
        status = "no target on crosshair"; return;
    }
    if (now - last_tap < cfg::trigger::cooldown_ms) {
        status = "cooldown"; return;
    }

    // Jitter gate: on first detection, arm a random delay then fire once.
    if (!tap_pending) {
        tap_pending = true;
        // Random delay 0–jitter_ms (using diag clock as seed proxy)
        const long long jitter = cfg::trigger::jitter_ms > 0
            ? (now % (cfg::trigger::jitter_ms + 1)) : 0;
        pending_at = now + jitter;
        status = "arming"; return;
    }
    if (now < pending_at) { status = "jitter delay"; return; }
    tap_pending = false;

    // Init uinput if needed
    if (!dev.init(int(g_sw), int(g_sh), orientation, now)) {
        status = "uinput unavailable (root/device policy)"; return;
    }

    const float px = std::clamp(cfg::legit::point_x, 0.05f, 0.95f);
    const float py = std::clamp(cfg::legit::point_y, 0.05f, 0.95f);

    // DOWN
    bool ok = dev.step(px, py, 0.f, 0.f, 1.f, 0.f, now);
    // UP — tiny synthetic pause (1 ms offset same timestamp is fine for uinput)
    dev.release();

    if (ok) {
        last_tap = now;
        status = "tap sent";
    } else {
        status = "tap failed";
    }
}

} // namespace triggerbot
