#pragma once
// Fog via Unity RenderSettings (libunity.so, Standoff 2 build analysed in this repo).
// Pure templates over read/write callbacks so the logic can be tested on synthetic memory.
//
// libunity.so analysis:
//   GetRenderSettings: 0x5EC28BC -> mov w0,#0x16; b 0x582F9D8
//   0x582F9D8: adrp x8,#0xCFB6000; add x8,#0x708; ldr x0,[x8,w0,sxtw #3]  => RenderSettings* = *(lib+0xCFB67B8)
//   RenderSettings: fog +0x150 (bool), fogMode +0x154, fogColor +0x158 (rgba), start +0x168, end +0x16C, density +0x170
//   Setters call 0x5EC0FA8 which pushes derived values into the GfxDevice (*(lib+0xD06ACE0), stored at 0x5FB07D0)
//   and toggles the FOG_LINEAR/FOG_EXP/FOG_EXP2 keyword bits (0x11/0x12/0x13) in *( *(lib+0xD011F40) + 0x100 + 0x20 ).
// An external process cannot call 0x5EC0FA8, so push_device() reproduces it 1:1 (same constants and stores).
#include <cstdint>
#include <cstring>
#include <cmath>

namespace fog_core {
inline constexpr uint64_t rs_slot_rva     = 0xCFB6708 + 0x16 * 8; // RenderSettings* slot
inline constexpr uint64_t device_slot_rva = 0xD06ACE0;            // GfxDevice* (main thread device)
inline constexpr uint64_t keywords_slot_rva = 0xD011F40;          // global keyword state holder

inline constexpr uint32_t rs_enabled = 0x150, rs_mode = 0x154, rs_color = 0x158,
                          rs_start = 0x168, rs_end = 0x16C, rs_density = 0x170;
inline constexpr uint32_t dev_dirty = 0xD90, dev_params = 0x12E4, dev_color = 0x1314,
                          dev_linear_a = 0x132C, dev_linear_b = 0x1330;
inline constexpr uint32_t kw_set = 0x100, kw_bits = 0x20;

// Code signatures checked before any write (exact libunity.so build). If anything differs: no writes.
struct sig { uint64_t rva; uint32_t insn; };
inline constexpr sig signatures[] = {
    {0x582F9D8, 0xF003BC28}, {0x582F9DC, 0x911C2108}, {0x582F9E0, 0xF860D900}, // RenderSettings table
    {0x5EC0FBC, 0x39454288}, {0x5EC0FCC, 0x52825C88},                         // ldrb +0x150 / mov #0x12E4
    {0x5EC107C, 0x39364269}, {0x5EC122C, 0xBD131668},                         // strb +0xD90 / str +0x1314
    {0x5EC1230, 0x90038A94}, {0x5EC1234, 0x913D0294}, {0x5EC1250, 0x91040100}, // keyword holder +0x100
    {0x5FB07D0, 0xF9067133}, {0x59812B4, 0xF9401009},                         // device slot / keyword bits
};

// RenderSettings fog setter icalls (only callers of the native setters 0x5EC206C/90/C0/D8/1C/A8).
// While fog is on their first instruction becomes RET so game scripts can't overwrite our fog
// every frame (that fight was the flicker). Original word is verified before patch/restore.
struct setter { uint64_t rva; uint32_t original; };
inline constexpr setter setters[] = {
    {0x57C2A84, 0xA9BF4FFE}, // set_fog
    {0x57C2AB4, 0xFC1F0FE8}, // set_fogStartDistance
    {0x57C2AE8, 0xFC1F0FE8}, // set_fogEndDistance
    {0x57C2B1C, 0xA9BF4FFE}, // set_fogMode
    {0x57C2B54, 0xA9BF4FFE}, // set_fogColor_Injected
    {0x57C2B80, 0xFC1F0FE8}, // set_fogDensity
};
inline constexpr uint32_t insn_ret = 0xD65F03C0;
enum class lock_state { unknown, original, locked };
template <class Read> lock_state setters_state(Read& read, uint64_t lib) {
    int orig = 0, ret = 0;
    for (const auto& s : setters) {
        uint32_t w = 0; if (!read(lib + s.rva, &w, 4)) return lock_state::unknown;
        if (w == s.original) ++orig; else if (w == insn_ret) ++ret; else return lock_state::unknown;
    }
    return ret == 0 ? lock_state::original : orig == 0 ? lock_state::locked : lock_state::unknown;
}
template <class Read> bool setters_known(Read& read, uint64_t lib) {
    for (const auto& s : setters) { uint32_t w = 0; if (!read(lib + s.rva, &w, 4) || (w != s.original && w != insn_ret)) return false; }
    return true;
}
// Mixed state (some patched) is also handled: each word is checked individually.
template <class Read, class CodeWrite> bool lock_setters(Read& read, CodeWrite& cw, uint64_t lib, bool lock) {
    for (const auto& s : setters) {
        uint32_t w = 0; if (!read(lib + s.rva, &w, 4)) return false;
        if (w != s.original && w != insn_ret) return false;
    }
    bool ok = true;
    for (const auto& s : setters) {
        const uint32_t want = lock ? insn_ret : s.original;
        uint32_t w = 0; read(lib + s.rva, &w, 4);
        if (w != want) ok &= cw(lib + s.rva, &want, 4);
    }
    return ok && setters_state(read, lib) == (lock ? lock_state::locked : lock_state::original);
}

struct rgba { float r, g, b, a; };
struct state {                // RenderSettings fog block
    uint8_t enabled = 0; int32_t mode = 0; rgba color{}; float start = 0, end = 0, density = 0;
    bool operator==(const state& o) const {
        return enabled == o.enabled && mode == o.mode && std::memcmp(&color, &o.color, sizeof color) == 0 &&
               start == o.start && end == o.end && density == o.density;
    }
};
struct settings { bool enabled = false; int mode = 1; rgba color{.6f, .6f, .65f, 1.f}; float start = 5, end = 60, density = .02f; bool linear_color = false; };

inline bool plausible(uint64_t p) { return p >= 0x10000 && p < 0x0001000000000000ULL && (p & 7) == 0; }
inline bool finite(float v) { return std::isfinite(v) && std::fabs(v) < 1e7f; }

template <class Read> bool code_ok(Read& read, uint64_t lib) {
    for (const auto& s : signatures) { uint32_t w = 0; if (!read(lib + s.rva, &w, 4) || w != s.insn) return false; }
    return true;
}
template <class Read> bool read_state(Read& read, uint64_t rs, state& s) {
    uint8_t raw[0x24]{};
    if (!read(rs + rs_enabled, raw, sizeof raw)) return false;
    s.enabled = raw[0]; std::memcpy(&s.mode, raw + 4, 4); std::memcpy(&s.color, raw + 8, 16);
    std::memcpy(&s.start, raw + 0x18, 4); std::memcpy(&s.end, raw + 0x1C, 4); std::memcpy(&s.density, raw + 0x20, 4);
    return true;
}
inline bool sane(const state& s) {
    return s.enabled <= 1 && s.mode >= 1 && s.mode <= 3 && finite(s.color.r) && finite(s.color.g) && finite(s.color.b) &&
           finite(s.color.a) && finite(s.start) && finite(s.end) && finite(s.density);
}
template <class Write> bool write_state(Write& write, uint64_t rs, const state& s) {
    uint8_t raw[0x24]{};
    if (!write(rs + rs_enabled, &s.enabled, 1)) return false;
    std::memcpy(raw, &s.mode, 4); std::memcpy(raw + 4, &s.color, 16);
    std::memcpy(raw + 0x14, &s.start, 4); std::memcpy(raw + 0x18, &s.end, 4); std::memcpy(raw + 0x1C, &s.density, 4);
    return write(rs + rs_mode, raw, 0x20);
}
// GammaToLinearSpace exactly as in 0x5EC1098..0x5EC1228
inline float to_linear(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    if (c < 1.f) return std::pow((c + 0.055f) / 1.055f, 2.4f);
    if (c == 1.f) return 1.f;
    return std::pow(c, 2.2f);
}
struct device_values { float start4[4], end4[4], density4[4], unused[4], exp2[2], lin_a, lin_b; rgba color; };
inline device_values derive(const state& s, bool linear_color) {
    device_values d{};
    const int mode = s.enabled ? s.mode : 0;
    float start = 10000.f, end = 20000.f, density = 0, a = 0, b = 0, e0 = 0, e1 = 0;
    if (mode) {
        start = s.start; end = s.end; density = s.density;
        const float diff = mode == 1 ? end - start : 0.f;
        const float scale = std::fabs(diff) > 1e-4f ? 1.f / diff : 0.f;
        a = mode == 1 ? -scale : 0.f; b = mode == 1 ? end * scale : 0.f;
        e0 = 1.2011224f * density; e1 = 1.4426950f * density; // constants at 0x1B466B8
    }
    for (int i = 0; i < 4; ++i) { d.start4[i] = start; d.end4[i] = end; d.density4[i] = density; }
    d.exp2[0] = e0; d.exp2[1] = e1; d.lin_a = a; d.lin_b = b;
    d.color = linear_color ? rgba{to_linear(s.color.r), to_linear(s.color.g), to_linear(s.color.b), s.color.a} : s.color;
    return d;
}
// Mirrors 0x5EC0FA8 stores + keyword toggles.
template <class Read, class Write> bool push_device(Read& read, Write& write, uint64_t device, uint64_t keywords, const state& s, bool linear_color) {
    if (!plausible(device)) return false;
    const auto d = derive(s, linear_color);
    const uint8_t one = 1; bool ok = true;
    ok &= write(device + dev_params + 0x00, d.start4, 16);
    ok &= write(device + dev_params + 0x10, d.end4, 16);
    ok &= write(device + dev_params + 0x20, d.density4, 16);
    ok &= write(device + dev_params + 0x40, d.exp2, 8);
    ok &= write(device + dev_linear_a, &d.lin_a, 4);
    ok &= write(device + dev_linear_b, &d.lin_b, 4);
    ok &= write(device + dev_color, &d.color, 16);
    ok &= write(device + dev_dirty, &one, 1);
    if (plausible(keywords)) {
        uint64_t bits = 0, word = 0;
        if (read(keywords + kw_set + kw_bits, &bits, 8) && plausible(bits) && read(bits, &word, 8)) {
            word &= ~((1ULL << 0x11) | (1ULL << 0x12) | (1ULL << 0x13));
            const int mode = s.enabled ? s.mode : 0;
            if (mode >= 1 && mode <= 3) word |= 1ULL << (0x10 | mode);
            ok &= write(bits, &word, 8);
        } else ok = false;
    } else ok = false;
    return ok;
}
// true if the device already holds exactly what push_device would write (no write needed).
template <class Read> bool device_matches(Read& read, uint64_t device, const state& s, bool linear_color) {
    if (!plausible(device)) return false;
    const auto d = derive(s, linear_color);
    float cur[12]{}; rgba col{}; float a = 0, b = 0;
    if (!read(device + dev_params, cur, sizeof cur) || !read(device + dev_color, &col, 16) ||
        !read(device + dev_linear_a, &a, 4) || !read(device + dev_linear_b, &b, 4)) return false;
    auto eq = [](float x, float y) { return std::fabs(x - y) <= 1e-5f * (1.f + std::fabs(y)); };
    return eq(cur[0], d.start4[0]) && eq(cur[4], d.end4[0]) && eq(cur[8], d.density4[0]) && eq(a, d.lin_a) &&
           eq(b, d.lin_b) && eq(col.r, d.color.r) && eq(col.g, d.color.g) && eq(col.b, d.color.b) && eq(col.a, d.color.a);
}
template <class Read> bool keywords_match(Read& read, uint64_t keywords, const state& s) {
    uint64_t bits = 0, word = 0;
    if (!plausible(keywords) || !read(keywords + kw_set + kw_bits, &bits, 8) || !plausible(bits) || !read(bits, &word, 8)) return false;
    const int mode = s.enabled ? s.mode : 0;
    for (int m = 1; m <= 3; ++m) if (((word >> (0x10 | m)) & 1) != uint64_t(m == mode)) return false;
    return true;
}
// Colour space guess: the device keeps the converted colour, so compare it with RenderSettings.
template <class Read> bool guess_linear(Read& read, uint64_t device, const state& s) {
    rgba c{}; if (!plausible(device) || !read(device + dev_color, &c, 16)) return false;
    auto near = [](float x, float y) { return std::fabs(x - y) < 1e-3f; };
    if (near(c.r, s.color.r) && near(c.g, s.color.g) && near(c.b, s.color.b)) return false;
    return near(c.r, to_linear(s.color.r)) && near(c.g, to_linear(s.color.g)) && near(c.b, to_linear(s.color.b));
}
inline state wanted(const settings& u) {
    state s{}; s.enabled = 1; s.mode = u.mode < 1 ? 1 : u.mode > 3 ? 3 : u.mode; s.color = u.color;
    s.start = u.start; s.end = u.end > u.start + .1f ? u.end : u.start + .1f; s.density = u.density;
    return s;
}
} // namespace fog_core
