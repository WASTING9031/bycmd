#pragma once
// Fog: Unity RenderSettings fog + the native recompute mirrored into the GfxDevice.
// Anti-flicker: while on, the RenderSettings fog setter icalls are patched to RET so game
// scripts can't overwrite it every frame; device/keywords are written only when they differ.
// Originals (values + code) are restored on disable. See README_FOG.md.
#include "../game/game.hpp"
#include "../game/fog_core.hpp"
#include "../ui/cfg.hpp"
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace fog {
inline const char* g_status = "off";
namespace detail {
inline int pid = -1; inline uint64_t lib = 0;
inline uint64_t rs = 0;
inline bool saved = false; inline fog_core::state original{}; inline bool linear = false;
inline bool locked = false;
inline long long next_apply = 0, next_lock_try = 0;
inline unsigned pushes = 0;
inline int mem_fd = -1, mem_fd_pid = -1;
inline auto reader() { return [](uint64_t a, void* b, size_t n) { return mem_read(a, b, n); }; }
inline auto writer() { return [](uint64_t a, const void* b, size_t n) { return mem_write(a, b, n); }; }
// Code pages are r-x: /proc/<pid>/mem opened O_RDWR (root) can write them.
inline bool code_write(uint64_t a, const void* b, size_t n) {
    if (mem_fd >= 0 && mem_fd_pid != proc::pid) { ::close(mem_fd); mem_fd = -1; }
    if (mem_fd < 0) {
        char path[64]; std::snprintf(path, sizeof path, "/proc/%d/mem", proc::pid);
        mem_fd = ::open(path, O_RDWR | O_CLOEXEC); mem_fd_pid = proc::pid;
        if (mem_fd < 0) { diag::log("FOG", "open %s O_RDWR failed errno=%d", path, errno); return false; }
    }
    return ::pwrite(mem_fd, b, n, off_t(a)) == ssize_t(n);
}
inline auto code_writer() { return [](uint64_t a, const void* b, size_t n) { return code_write(a, b, n); }; }
inline bool same_session() { return pid == proc::pid && lib == proc::lib && proc::pid > 0; }
inline uint64_t device() { return rpm<uint64_t>(proc::lib + fog_core::device_slot_rva); }
inline uint64_t keywords() { return rpm<uint64_t>(proc::lib + fog_core::keywords_slot_rva); }
inline void release(const char* why) {
    if (!same_session()) { saved = false; rs = 0; locked = false; return; }
    auto read = reader(); auto write = writer(); auto cw = code_writer();
    if (saved && rs && rs == rpm<uint64_t>(proc::lib + fog_core::rs_slot_rva)) {
        const bool a = fog_core::write_state(write, rs, original);
        const bool b = fog_core::push_device(read, write, device(), keywords(), original, linear);
        diag::log("FOG", "restored original fog (%s) rs=%d device=%d", why, a ? 1 : 0, b ? 1 : 0);
    }
    saved = false; rs = 0;
    if (locked) {
        const bool ok = fog_core::lock_setters(read, cw, lib, false);
        diag::log("FOG", "setter lock removed (%s) ok=%d", why, ok ? 1 : 0);
        locked = false;
    }
}
} // namespace detail

inline bool active() { return cfg::fog::enabled; }
inline fog_core::settings current_settings() {
    fog_core::settings s{};
    s.enabled = cfg::fog::enabled; s.mode = cfg::fog::mode + 1;
    s.color = {cfg::fog::color.x, cfg::fog::color.y, cfg::fog::color.z, cfg::fog::color.w};
    s.start = cfg::fog::start; s.end = cfg::fog::end; s.density = cfg::fog::density;
    return s;
}

inline void tick(bool game_running) {
    using namespace detail;
    if (!game_running || !active() || proc::pid <= 0 || !proc::lib) {
        release(game_running ? "disabled" : "game not running");
        g_status = active() ? "waiting for game" : "off"; return;
    }
    if (!same_session()) { saved = false; rs = 0; locked = false; pid = proc::pid; lib = proc::lib; }
    const long long now = diag::now_ms();
    if (now < next_apply) return;
    next_apply = now + 33;
    auto read = reader(); auto write = writer(); auto cw = code_writer();
    if (!fog_core::code_ok(read, lib) || !fog_core::setters_known(read, lib)) {
        g_status = "libunity.so differs from analysed build: fog disabled (no writes)"; return;
    }
    if (!locked && now >= next_lock_try) {
        locked = fog_core::lock_setters(read, cw, lib, true);
        diag::log("FOG", "setter lock %s", locked ? "applied" : "FAILED (needs root write to /proc/pid/mem)");
        if (!locked) next_lock_try = now + 3000;
    }
    const uint64_t cur = rpm<uint64_t>(lib + fog_core::rs_slot_rva);
    if (!fog_core::plausible(cur)) { g_status = "RenderSettings not ready"; return; }
    if (cur != rs) { saved = false; rs = cur; } // scene change: new RenderSettings object
    fog_core::state now_state{};
    if (!fog_core::read_state(read, rs, now_state)) { g_status = "RenderSettings read failed"; return; }
    const auto want = fog_core::wanted(current_settings());
    if (!saved) {
        if (!fog_core::sane(now_state)) { g_status = "RenderSettings layout check failed: no writes"; return; }
        original = now_state; saved = true;
        linear = fog_core::guess_linear(read, device(), now_state);
        diag::log("FOG", "rs=0x%llx original fog=%d mode=%d start=%.1f end=%.1f density=%.3f colorspace=%s",
            (unsigned long long)rs, original.enabled, original.mode, original.start, original.end, original.density, linear ? "linear" : "gamma");
    }
    bool ok = true;
    if (!(now_state == want)) ok &= fog_core::write_state(write, rs, want);
    // Only touch the device / keyword word when they differ: no constant racing with the game.
    const uint64_t dev = device(), kw = keywords();
    if (!fog_core::device_matches(read, dev, want, linear) || !fog_core::keywords_match(read, kw, want)) {
        ok &= fog_core::push_device(read, write, dev, kw, want, linear); ++pushes;
    }
    static char text[192];
    static const char* modes[] = {"?", "linear", "exp", "exp2"};
    std::snprintf(text, sizeof text, "fog %s mode=%s lock=%s pushes=%u%s", ok ? "on" : "partial", modes[want.mode],
        locked ? "on" : "off (may flicker)", pushes, ok ? "" : " (device/keyword write failed)");
    g_status = text;
    static long long next_log = 0;
    if (diag::every(next_log, 5000)) diag::log("FOG", "%s rs=0x%llx dev=0x%llx", text, (unsigned long long)rs, (unsigned long long)dev);
}
} // namespace fog
