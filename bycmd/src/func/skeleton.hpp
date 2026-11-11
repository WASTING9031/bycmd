#pragma once
// Tight ESP boxes from the real skeleton (character SkinnedMeshRenderer bones).
// Bone transform pointers are resolved once per player (every 2 s); per frame only two bulk reads
// per hierarchy (local TRS array + parent indices) are done and world positions are composed locally.
// Fallback (no bones): caller uses the hip-based box.
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/mesh_core.hpp"
#include "../game/player.hpp"

namespace skeleton {
struct pose {
    bool ok = false;
    std::vector<Vector3> bones;   // world positions
    Vector3 head{}, top{}, bottom{};
};
namespace detail {
struct entry {
    uint32_t id = 0; uint64_t smr = 0, hier = 0; std::vector<int32_t> index; int32_t max_index = -1;
    long long refreshed = 0; bool ok = false;
};
inline std::unordered_map<uint64_t, entry> cache;
inline int pid = -1;
inline auto reader() { return [](uint64_t a, void* b, size_t n) { return mem_read(a, b, n); }; }

// Read-only CharacterBindings discovery used only to locate skeleton bones.
// No material replacement, shader state, GPU pass or render-node writes.
inline void bindings_of(uint64_t object,std::vector<uint64_t>& out) {
    const uint64_t model=player::find_part(object,rpm<uint64_t>(proc::lib+v100::model_typeinfo));
    if(!plausible(model))return;
    const uint64_t a=rpm<uint64_t>(model+0x30),live=rpm<uint64_t>(model+0x38);
    const uint64_t b=plausible(live)?rpm<uint64_t>(live+0x50):0;
    for(uint64_t x:{a,b}){
        if(!plausible(x))continue;
        const uint64_t klass=rpm<uint64_t>(x);std::string name,ns;
        if(plausible(klass)&&il2cpp::klass_identity(klass,name,ns)&&name=="CharacterBindings"&&std::find(out.begin(),out.end(),x)==out.end())out.push_back(x);
    }
}
inline void refresh(uint64_t player, entry& e) {
    e.ok = false; e.index.clear(); e.hier = 0; e.max_index = -1;
    std::vector<uint64_t> b; bindings_of(player, b);
    if (b.empty()) return;
    const uint64_t managed = rpm<uint64_t>(b[0] + 0x38);
    const uint64_t smr = plausible(managed) ? rpm<uint64_t>(managed + v100::unity_cached_ptr) : 0;
    if (!mesh_core::ptr_ok(smr)) return;
    const uint64_t data = rpm<uint64_t>(smr + mesh_core::smr_bones);
    const uint64_t count = rpm<uint64_t>(smr + mesh_core::smr_bones_size);
    if (!mesh_core::ptr_ok(data) || !count || count > 256) return;
    std::vector<int32_t> ids(count);
    if (!mem_read(data, ids.data(), count * 4)) return;
    auto read = reader();
    for (int32_t id : ids) {
        const uint64_t tr = mesh_core::resolve_id(read, proc::lib, id);
        if (!tr) continue;
        const uint64_t h = rpm<uint64_t>(tr + mesh_core::tr_hierarchy);
        const int32_t i = rpm<int32_t>(tr + mesh_core::tr_index);
        if (!mesh_core::ptr_ok(h) || i < 0 || i > 2048) continue;
        if (!e.hier) e.hier = h;
        if (h != e.hier) continue;
        e.index.push_back(i); e.max_index = std::max(e.max_index, i);
    }
    e.smr = smr;
    e.ok = e.index.size() >= 8;
}
}

inline pose read(uint64_t player) {
    using namespace detail;
    pose out;
    if (pid != proc::pid) { cache.clear(); pid = proc::pid; }
    const long long now = diag::now_ms();
    const uint32_t id = rpm<uint32_t>(player + v100::net_object_id);
    entry& e = cache[player];
    if (e.id != id || now - e.refreshed > 2000) { e.id = id; e.refreshed = now; refresh(player, e); }
    if (!e.ok) return out;
    const int n = e.max_index + 1;
    const uint64_t trs = rpm<uint64_t>(e.hier + mesh_core::h_trs), par = rpm<uint64_t>(e.hier + mesh_core::h_parents);
    if (!mesh_core::ptr_ok(trs) || !mesh_core::ptr_ok(par)) { e.ok = false; return out; }
    std::vector<float> t(size_t(n) * 12);
    std::vector<int32_t> parents(n);
    if (!mem_read(trs, t.data(), t.size() * 4) || !mem_read(par, parents.data(), n * 4ull)) { e.ok = false; return out; }
    std::vector<mesh_core::mat4> world(n);
    std::vector<uint8_t> state(n, 0); // 0 todo, 1 ok, 2 bad
    // Unity hierarchies are depth-first: parent index < child index.
    for (int i = 0; i < n; ++i) {
        const float* x = &t[size_t(i) * 12];
        const mesh_core::mat4 local = mesh_core::trs({x[0], x[1], x[2]}, {x[4], x[5], x[6], x[7]}, {x[8], x[9], x[10]});
        bool good = true; for (float f : local.m) good &= std::isfinite(f);
        const int p = parents[i];
        if (!good || p >= i || p < -1) { state[i] = 2; continue; }
        if (p < 0) world[i] = local;
        else { if (state[p] != 1) { state[i] = 2; continue; } world[i] = mesh_core::mul(world[p], local); }
        state[i] = 1;
    }
    float top = -1e9f, bottom = 1e9f;
    for (int32_t i : e.index) {
        if (state[i] != 1) continue;
        const auto& m = world[i].m;
        const Vector3 p(m[12], m[13], m[14]);
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) continue;
        out.bones.push_back(p);
        if (p.y > top) { top = p.y; out.top = p; }
        if (p.y < bottom) { bottom = p.y; out.bottom = p; }
    }
    if (out.bones.size() < 8) return out;
    // sanity: a standing / crouching character is 0.5 .. 2.4 m tall
    const float h = top - bottom;
    if (!(h > .4f && h < 2.6f)) return out;
    out.head = out.top; out.head.y -= .08f;
    out.ok = true;
    return out;
}

struct rect { bool ok = false; ImVec2 min{}, max{}; };
// Screen bounds of the projected skeleton, padded for head top / limbs. All bones must be in front.
inline rect screen_box(const pose& p, const matrix& v, const matrix& pr) {
    rect r;
    if (!p.ok) return r;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (const auto& b : p.bones) {
        ImVec2 s; if (!world_to_screen(b, v, pr, s)) return r;
        x0 = std::min(x0, s.x); y0 = std::min(y0, s.y); x1 = std::max(x1, s.x); y1 = std::max(y1, s.y);
    }
    // head-top bone sits ~10 cm below the top of the head, feet bones ~5 cm above the ground
    Vector3 hat = p.top; hat.y += .14f; Vector3 sole = p.bottom; sole.y -= .06f;
    ImVec2 a, b;
    if (world_to_screen(hat, v, pr, a)) y0 = std::min(y0, a.y);
    if (world_to_screen(sole, v, pr, b)) y1 = std::max(y1, b.y);
    const float h = y1 - y0;
    if (!(h > 2.f) || !std::isfinite(h)) return r;
    const float pad = std::max(2.f, h * .06f);
    x0 -= pad; x1 += pad;
    const float minw = h * .30f;                         // arms down: keep a reasonable width
    if (x1 - x0 < minw) { const float c = (x0 + x1) * .5f; x0 = c - minw * .5f; x1 = c + minw * .5f; }
    r.min = ImVec2(std::round(x0), std::round(y0)); r.max = ImVec2(std::round(x1), std::round(y1)); r.ok = true;
    return r;
}
inline void forget_missing(const std::vector<uint64_t>& alive) {
    for (auto it = detail::cache.begin(); it != detail::cache.end();)
        if (std::find(alive.begin(), alive.end(), it->first) == alive.end()) it = detail::cache.erase(it); else ++it;
}
}
