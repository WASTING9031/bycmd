#pragma once
// External SkinnedMeshRenderer reader + CPU skinning (Standoff 2 1.0.0 ARM64, libunity.so).
// Native layout and transform helpers; every layout below was re-verified
// in THIS Android libunity.so (addresses in comments). No game writes, no Unity calls.
//
//   SkinnedMeshRenderer::get_sharedMesh  0x5F72664 : ldr w0,[smr,#0x1E0]  (mesh InstanceID)
//   InstanceID -> Object* hash map       global 0xCFB60E0 (hash*0x9E3779B1, 24-byte entries)
//   SkinnedMeshRenderer::get_bones       0x57C6AAC : dynamic_array<int> at smr+0x1F8 (size +0x10)
//   Mesh::get_vertexCount                0x57C7C6C : [[mesh+0x28]+0x78]
//   Mesh::GetVertexAttribute*            0x5F5C660 : channels md+0x08 (4 bytes each)
//   stream descriptors                   0x5F5A568 : md+0x40, 12 bytes: offset +4, stride +8
//   CPU vertex data                      0x5F5B52C : md+0x98
//   Mesh::GetIndices                     0x626531C : buffer md+0xA0, size md+0xB0, fmt md+0xC0
//   submeshes                            0x5F5EB08 : md+0xC8 (count +0xD8), 0x38 each: firstByte +0x20, count +0x24, baseVertex +0x2C
//   Mesh::get_bindposes                  0x57C7AB0 : dynamic_array<Matrix4x4> md+0xE8 (size +0xF8)
//   Transform::get_position              0x59A4598 : hierarchy [t+0x28], index [t+0x30]; TRS array h+0x18 (0x30 each), parents h+0x20
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace mesh_core {
inline constexpr uint64_t object_map_global = 0xCFB60E0;
inline constexpr uint32_t hash_mul = 0x9E3779B1u;
inline constexpr uint64_t smr_mesh_id = 0x1E0, smr_bones = 0x1F8, smr_bones_size = 0x208;
inline constexpr uint64_t mesh_data = 0x28;
inline constexpr uint64_t md_channels = 0x08, md_streams = 0x40, md_vertex_count = 0x78;
inline constexpr uint64_t md_vertex_data = 0x98;
inline constexpr uint64_t md_index_data = 0xA0, md_index_size = 0xB0, md_index_format = 0xC0;
inline constexpr uint64_t md_submeshes = 0xC8, md_submesh_count = 0xD8, submesh_stride = 0x38;
inline constexpr uint64_t sm_first_byte = 0x20, sm_index_count = 0x24, sm_base_vertex = 0x2C;
inline constexpr uint64_t md_bindposes = 0xE8, md_bindpose_count = 0xF8;
inline constexpr uint64_t tr_hierarchy = 0x28, tr_index = 0x30, h_trs = 0x18, h_parents = 0x20, trs_stride = 0x30;
inline constexpr uint32_t max_vertices = 0x30000, max_indices = 0x90000, max_bones = 512, max_depth = 128;
enum attr { a_position = 0, a_normal = 1, a_uv0 = 4, a_weight = 12, a_index = 13 };

struct mat4 { float m[16]; }; // column-major, Unity Matrix4x4 memory order
struct vec2 { float x, y; };
struct vec3 { float x, y, z; };
struct quat { float x, y, z, w; };

inline bool ptr_ok(uint64_t p) { return p >= 0x10000 && p < 0x0000800000000000ULL; }
inline bool finite3(const vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

inline mat4 identity() { mat4 r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f; return r; }
inline mat4 mul(const mat4& a, const mat4& b) {
    mat4 r{};
    for (int c = 0; c < 4; ++c) for (int rr = 0; rr < 4; ++rr) {
        float s = 0.f; for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
        r.m[c * 4 + rr] = s;
    }
    return r;
}
inline mat4 trs(const vec3& t, quat q, const vec3& s) {
    const float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!(l > 1e-6f) || !std::isfinite(l)) q = {0, 0, 0, 1}; else { q.x /= l; q.y /= l; q.z /= l; q.w /= l; }
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    mat4 r{};
    r.m[0] = (1 - 2 * (yy + zz)) * s.x; r.m[1] = 2 * (xy + wz) * s.x;       r.m[2] = 2 * (xz - wy) * s.x;
    r.m[4] = 2 * (xy - wz) * s.y;       r.m[5] = (1 - 2 * (xx + zz)) * s.y; r.m[6] = 2 * (yz + wx) * s.y;
    r.m[8] = 2 * (xz + wy) * s.z;       r.m[9] = 2 * (yz - wx) * s.z;       r.m[10] = (1 - 2 * (xx + yy)) * s.z;
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; r.m[15] = 1.f;
    return r;
}
inline vec3 xform_point(const mat4& m, const vec3& v) {
    return {m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12],
            m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13],
            m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14]};
}
inline vec3 xform_dir(const mat4& m, const vec3& v) {
    return {m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z,
            m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z,
            m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z};
}

inline float half_to_float(uint16_t h) {
    const uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, f = h & 1023;
    float v;
    if (e == 0) v = std::ldexp(float(f), -24);
    else if (e == 31) v = f ? NAN : INFINITY;
    else v = std::ldexp(float(f | 1024), int(e) - 25);
    return s ? -v : v;
}
// Unity VertexAttributeFormat; sizes match table libunity+0x1BA9C14 {4,2,1,1,2,2,1,1,2,2,4,4}.
inline int format_size(uint8_t f) { static const uint8_t s[12] = {4,2,1,1,2,2,1,1,2,2,4,4}; return f < 12 ? s[f] : 0; }
inline float decode(const uint8_t* p, uint8_t f) {
    switch (f) {
    case 0: { float v; std::memcpy(&v, p, 4); return v; }
    case 1: { uint16_t v; std::memcpy(&v, p, 2); return half_to_float(v); }
    case 2: return p[0] / 255.f;
    case 3: return std::fmax(int8_t(p[0]) / 127.f, -1.f);
    case 4: { uint16_t v; std::memcpy(&v, p, 2); return v / 65535.f; }
    case 5: { int16_t v; std::memcpy(&v, p, 2); return std::fmax(v / 32767.f, -1.f); }
    case 6: return float(p[0]);
    case 7: return float(int8_t(p[0]));
    case 8: { uint16_t v; std::memcpy(&v, p, 2); return float(v); }
    case 9: { int16_t v; std::memcpy(&v, p, 2); return float(v); }
    case 10: { uint32_t v; std::memcpy(&v, p, 4); return float(v); }
    case 11: { int32_t v; std::memcpy(&v, p, 4); return float(v); }
    }
    return NAN;
}
inline uint32_t decode_uint(const uint8_t* p, uint8_t f) {
    switch (f) {
    case 6: case 2: return p[0];
    case 7: return uint32_t(int8_t(p[0]) < 0 ? 0 : p[0]);
    case 8: case 4: { uint16_t v; std::memcpy(&v, p, 2); return v; }
    case 9: { int16_t v; std::memcpy(&v, p, 2); return v < 0 ? 0u : uint32_t(v); }
    case 10: { uint32_t v; std::memcpy(&v, p, 4); return v; }
    case 11: { int32_t v; std::memcpy(&v, p, 4); return v < 0 ? 0u : uint32_t(v); }
    case 0: { float v; std::memcpy(&v, p, 4); return v >= 0 && v < 65536 ? uint32_t(v) : 0u; }
    }
    return 0;
}

struct channel { uint8_t stream = 0, offset = 0, format = 0, dim = 0; };
struct stream { uint32_t offset = 0; uint8_t stride = 0; };

// Static (bind-pose) mesh data, read once per native mesh and cached.
struct mesh {
    uint64_t native = 0;
    std::vector<vec3> pos, nrm;
    std::vector<vec2> uv;
    std::vector<float> weight;     // 4 per vertex
    std::vector<uint16_t> bone;    // 4 per vertex
    std::vector<uint32_t> indices; // triangle list, already validated (< vertex count)
    std::vector<mat4> bindposes;
    bool has_normals = false, has_skin = false;
    const char* error = nullptr;
};

template <class Read> bool get(Read& read, uint64_t a, void* out, size_t n) { return a && read(a, out, n); }
template <class T, class Read> T val(Read& read, uint64_t a, bool* ok = nullptr) {
    T v{}; const bool r = get(read, a, &v, sizeof(T)); if (ok) *ok = r; if (!r) std::memset(&v, 0, sizeof v); return v;
}

// Unity InstanceID -> native Object* using the runtime's own hash map (0x5F72664 logic).
template <class Read> uint64_t resolve_id(Read& read, uint64_t lib, int32_t id) {
    if (!id) return 0;
    const uint64_t map = val<uint64_t>(read, lib + object_map_global);
    if (!ptr_ok(map)) return 0;
    const uint64_t buckets = val<uint64_t>(read, map);
    const uint32_t mask = val<uint32_t>(read, map + 8);
    if (!ptr_ok(buckets) || !mask) return 0;
    const uint32_t h = uint32_t(id) * hash_mul, tag = h & ~3u;
    uint32_t slot = h & mask, step = 8;
    for (int i = 0; i < 2048; ++i) {
        struct { uint32_t tag, pad; int32_t id, pad2; uint64_t object; } e{};
        if (!get(read, buckets + uint64_t(slot) * 3, &e, sizeof e)) return 0;
        if (e.tag == tag && e.id == id) return ptr_ok(e.object) ? e.object : 0;
        if (e.tag == 0xFFFFFFFFu) return 0;
        slot = (slot + step) & mask; step += 8;
    }
    return 0;
}

template <class Read> bool read_mesh(Read& read, uint64_t native_mesh, mesh& out) {
    out = mesh{}; out.native = native_mesh;
    const uint64_t md = val<uint64_t>(read, native_mesh + mesh_data);
    if (!ptr_ok(md)) { out.error = "MeshData pointer invalid"; return false; }
    const uint32_t vc = val<uint32_t>(read, md + md_vertex_count);
    if (!vc || vc > max_vertices) { out.error = "vertex count out of range"; return false; }
    channel ch[14]; stream st[4];
    if (!get(read, md + md_channels, ch, sizeof ch)) { out.error = "channels unreadable"; return false; }
    for (int i = 0; i < 4; ++i) {
        uint8_t raw[12]; if (!get(read, md + md_streams + i * 12, raw, 12)) { out.error = "streams unreadable"; return false; }
        std::memcpy(&st[i].offset, raw + 4, 4); st[i].stride = raw[8];
    }
    const uint64_t vdata = val<uint64_t>(read, md + md_vertex_data);
    if (!ptr_ok(vdata)) { out.error = "mesh has no CPU copy (not readable)"; return false; }
    auto usable = [&](int a, int min_dim) {
        const channel& c = ch[a];
        if (c.dim == 0 || (c.dim & 15) < min_dim || c.stream > 3 || !format_size(c.format)) return false;
        const stream& s = st[c.stream];
        return s.stride && c.offset + format_size(c.format) * (c.dim & 15) <= s.stride;
    };
    if (!usable(a_position, 3)) { out.error = "no position channel"; return false; }
    // Read each used stream completely once.
    std::vector<uint8_t> buf[4];
    for (int a : {a_position, a_normal, a_uv0, a_weight, a_index}) {
        if (!usable(a, 1)) continue;
        const int si = ch[a].stream; if (!buf[si].empty()) continue;
        const size_t bytes = size_t(st[si].stride) * vc;
        if (bytes > 64u * 1024u * 1024u) { out.error = "stream too large"; return false; }
        buf[si].resize(bytes);
        if (!get(read, vdata + st[si].offset, buf[si].data(), bytes)) { out.error = "vertex stream unreadable"; return false; }
    }
    auto at = [&](int a, uint32_t v, int comp) {
        const channel& c = ch[a]; const stream& s = st[c.stream];
        return buf[c.stream].data() + size_t(v) * s.stride + c.offset + size_t(comp) * format_size(c.format);
    };
    out.pos.resize(vc);
    for (uint32_t v = 0; v < vc; ++v)
        out.pos[v] = {decode(at(a_position, v, 0), ch[0].format), decode(at(a_position, v, 1), ch[0].format), decode(at(a_position, v, 2), ch[0].format)};
    out.has_normals = usable(a_normal, 3);
    if (out.has_normals) {
        out.nrm.resize(vc);
        for (uint32_t v = 0; v < vc; ++v)
            out.nrm[v] = {decode(at(a_normal, v, 0), ch[1].format), decode(at(a_normal, v, 1), ch[1].format), decode(at(a_normal, v, 2), ch[1].format)};
    }
    if (usable(a_uv0, 2)) {
        out.uv.resize(vc);
        for (uint32_t v = 0; v < vc; ++v) {
            vec2 u{decode(at(a_uv0, v, 0), ch[a_uv0].format), decode(at(a_uv0, v, 1), ch[a_uv0].format)};
            out.uv[v] = std::isfinite(u.x) && std::isfinite(u.y) ? u : vec2{0, 0};
        }
    }
    // Bind poses + skin.
    const uint64_t bp = val<uint64_t>(read, md + md_bindposes);
    const uint64_t bpc = val<uint64_t>(read, md + md_bindpose_count);
    if (ptr_ok(bp) && bpc > 0 && bpc <= max_bones && usable(a_weight, 1) && usable(a_index, 1)) {
        out.bindposes.resize(size_t(bpc));
        if (get(read, bp, out.bindposes.data(), size_t(bpc) * sizeof(mat4))) {
            const int wd = ch[a_weight].dim & 15, id = ch[a_index].dim & 15;
            out.weight.assign(size_t(vc) * 4, 0.f); out.bone.assign(size_t(vc) * 4, 0);
            for (uint32_t v = 0; v < vc; ++v) {
                float sum = 0.f;
                for (int k = 0; k < 4; ++k) {
                    float w = k < wd ? decode(at(a_weight, v, k), ch[a_weight].format) : 0.f;
                    uint32_t b = k < id ? decode_uint(at(a_index, v, k), ch[a_index].format) : 0;
                    if (!std::isfinite(w) || w < 0.f || b >= bpc) w = 0.f, b = 0;
                    // single weight channel => remaining weight belongs to the first bone
                    out.weight[v * 4 + k] = w; out.bone[v * 4 + k] = uint16_t(b); sum += w;
                }
                if (wd == 1 && id >= 1) { out.weight[v * 4] = 1.f; sum = 1.f; }
                if (sum > 1e-5f) for (int k = 0; k < 4; ++k) out.weight[v * 4 + k] /= sum;
            }
            out.has_skin = true;
        } else out.bindposes.clear();
    }
    // Indices (only triangle submeshes; index range validated against buffer size).
    const uint64_t ib = val<uint64_t>(read, md + md_index_data);
    const uint64_t ib_size = val<uint64_t>(read, md + md_index_size);
    const bool u32 = val<uint32_t>(read, md + md_index_format) == 1;
    const uint64_t subs = val<uint64_t>(read, md + md_submeshes);
    const uint64_t sub_count = val<uint64_t>(read, md + md_submesh_count);
    if (!ptr_ok(ib) || !ib_size || ib_size > 64u * 1024u * 1024u || !ptr_ok(subs) || !sub_count || sub_count > 64) {
        out.error = "no CPU index buffer"; return false;
    }
    std::vector<uint8_t> raw(static_cast<size_t>(ib_size));
    if (!get(read, ib, raw.data(), raw.size())) { out.error = "index buffer unreadable"; return false; }
    const uint32_t isz = u32 ? 4 : 2;
    for (uint64_t s = 0; s < sub_count; ++s) {
        uint8_t sm[submesh_stride];
        if (!get(read, subs + s * submesh_stride, sm, sizeof sm)) continue;
        uint32_t first = 0, count = 0, topo = 0; int32_t base = 0;
        std::memcpy(&first, sm + sm_first_byte, 4); std::memcpy(&count, sm + sm_index_count, 4);
        std::memcpy(&topo, sm + 0x28, 4); std::memcpy(&base, sm + sm_base_vertex, 4);
        if (topo != 0) continue; // MeshTopology.Triangles only
        count -= count % 3;
        if (uint64_t(first) + uint64_t(count) * isz > ib_size || out.indices.size() + count > max_indices) continue;
        for (uint32_t t = 0; t < count; t += 3) {
            uint32_t tri[3]; bool good = true;
            for (int k = 0; k < 3; ++k) {
                const uint8_t* p = raw.data() + first + size_t(t + k) * isz;
                uint32_t i = 0; if (u32) std::memcpy(&i, p, 4); else { uint16_t x; std::memcpy(&x, p, 2); i = x; }
                const int64_t v = int64_t(i) + base;
                if (v < 0 || v >= int64_t(vc)) { good = false; break; }
                tri[k] = uint32_t(v);
            }
            if (good) out.indices.insert(out.indices.end(), tri, tri + 3); // whole triangle or nothing
        }
    }
    if (out.indices.empty()) { out.error = "no triangles"; return false; }
    return true;
}

// Per-frame world matrices of the renderer's bones, from the native transform hierarchy.
struct hierarchy_cache {
    struct node { bool done = false, ok = false; mat4 world{}; };
    std::unordered_map<uint64_t, std::unordered_map<int32_t, node>> h;
    void clear() { h.clear(); }
};
template <class Read> bool world_of(Read& read, uint64_t hier, int32_t index, hierarchy_cache& cache, mat4& out, int depth = 0) {
    if (index < 0 || depth > int(max_depth)) return false;
    auto& n = cache.h[hier][index];
    if (n.done) { out = n.world; return n.ok; }
    n.done = true;
    const uint64_t trs_arr = val<uint64_t>(read, hier + h_trs), parents = val<uint64_t>(read, hier + h_parents);
    if (!ptr_ok(trs_arr) || !ptr_ok(parents)) return false;
    float t[12];
    if (!get(read, trs_arr + uint64_t(index) * trs_stride, t, sizeof t)) return false;
    const int32_t parent = val<int32_t>(read, parents + uint64_t(index) * 4);
    const mat4 local = trs({t[0], t[1], t[2]}, {t[4], t[5], t[6], t[7]}, {t[8], t[9], t[10]});
    for (float f : local.m) if (!std::isfinite(f)) return false;
    mat4 w = local;
    if (parent >= 0) { mat4 p; if (parent == index || !world_of(read, hier, parent, cache, p, depth + 1)) return false; w = mul(p, local); }
    n.world = w; n.ok = true; out = w;
    return true;
}
template <class Read> bool transform_world(Read& read, uint64_t native_transform, hierarchy_cache& cache, mat4& out) {
    const uint64_t hier = val<uint64_t>(read, native_transform + tr_hierarchy);
    const int32_t index = val<int32_t>(read, native_transform + tr_index);
    if (!ptr_ok(hier) || index < 0 || index > 1 << 20) return false;
    return world_of(read, hier, index, cache, out);
}

template <class Read> bool bone_matrices(Read& read, uint64_t lib, uint64_t native_smr, const mesh& m,
                                         hierarchy_cache& cache, std::vector<mat4>& skin) {
    if (!m.has_skin) return false;
    const uint64_t data = val<uint64_t>(read, native_smr + smr_bones);
    const uint64_t count = val<uint64_t>(read, native_smr + smr_bones_size);
    if (!ptr_ok(data) || !count || count > max_bones) return false;
    std::vector<int32_t> ids(static_cast<size_t>(count));
    if (!get(read, data, ids.data(), ids.size() * 4)) return false;
    skin.assign(m.bindposes.size(), mat4{});
    int good = 0;
    for (size_t b = 0; b < skin.size(); ++b) {
        if (b >= ids.size()) break;
        const uint64_t tr = resolve_id(read, lib, ids[b]);
        mat4 w; if (!tr || !transform_world(read, tr, cache, w)) continue;
        skin[b] = mul(w, m.bindposes[b]); ++good;
    }
    return good > 0 && good * 10 >= int(skin.size()) * 9; // >=90% bones resolved, else skip (avoid exploded mesh)
}

struct gpu_vertex { float px, py, pz, nx, ny, nz, u, v; };
// Skins into world space. Vertices whose bones are missing collapse to NaN and are culled by the shader-side check.
inline size_t skin(const mesh& m, const std::vector<mat4>& skin, std::vector<gpu_vertex>& out, vec3& center) {
    const size_t vc = m.pos.size(); out.resize(vc);
    double cx = 0, cy = 0, cz = 0; size_t n = 0;
    for (size_t v = 0; v < vc; ++v) {
        mat4 s{}; float wsum = 0.f;
        for (int k = 0; k < 4; ++k) {
            const float w = m.weight[v * 4 + k]; if (w <= 0.f) continue;
            const mat4& b = skin[m.bone[v * 4 + k]];
            if (b.m[15] == 0.f) continue; // unresolved bone
            for (int i = 0; i < 16; ++i) s.m[i] += b.m[i] * w;
            wsum += w;
        }
        gpu_vertex& g = out[v];
        if (wsum < .5f) { g = {NAN, NAN, NAN, 0, 0, 0}; continue; }
        if (wsum < .999f) for (float& f : s.m) f /= wsum;
        const vec3 p = xform_point(s, m.pos[v]);
        vec3 nn = m.has_normals ? xform_dir(s, m.nrm[v]) : vec3{0, 1, 0};
        const float l = std::sqrt(nn.x * nn.x + nn.y * nn.y + nn.z * nn.z);
        if (l > 1e-6f) { nn.x /= l; nn.y /= l; nn.z /= l; }
        const vec2 uv = v < m.uv.size() ? m.uv[v] : vec2{m.pos[v].x, m.pos[v].y};
        g = {p.x, p.y, p.z, nn.x, nn.y, nn.z, uv.x, uv.y};
        if (finite3(p)) { cx += p.x; cy += p.y; cz += p.z; ++n; }
    }
    if (n) center = {float(cx / n), float(cy / n), float(cz / n)};
    return n;
}
// Faces normals when the mesh has none.
inline void face_normals(const mesh& m, std::vector<gpu_vertex>& v) {
    if (m.has_normals) return;
    for (auto& g : v) g.nx = g.ny = g.nz = 0.f;
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        gpu_vertex &a = v[m.indices[i]], &b = v[m.indices[i + 1]], &c = v[m.indices[i + 2]];
        const float ux = b.px - a.px, uy = b.py - a.py, uz = b.pz - a.pz, wx = c.px - a.px, wy = c.py - a.py, wz = c.pz - a.pz;
        const float nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
        if (!std::isfinite(nx + ny + nz)) continue;
        for (gpu_vertex* g : {&a, &b, &c}) { g->nx += nx; g->ny += ny; g->nz += nz; }
    }
    for (auto& g : v) { const float l = std::sqrt(g.nx * g.nx + g.ny * g.ny + g.nz * g.nz); if (l > 1e-8f) { g.nx /= l; g.ny /= l; g.nz /= l; } }
}
} // namespace mesh_core
