#pragma once
// Shared native Unity material-property access for the sky tint (libunity.so build analysed from the dump).
//  Renderer:  +0x140 dynamic_array<PPtr<Material>> data (int32 InstanceIDs), +0x150 size
//             (Renderer::GetMaterialCount = ldr w0,[x0,#0x150]; SetMaterial writes [data+i*4]=id)
//  Object:    +0x08 InstanceID, vt+0x50 GetName -> NamedObject m_Name (const char*) at +0x20 (validated at runtime)
//  Material:  +0x28 m_Shader InstanceID, +0x108 SharedMaterialData, SMD+0x60 ShaderPropertySheet
//  Sheet:     +0x10 int32 nameIDs, +0x30 u64 descriptors, +0x50 value buffer,
//             +0x70+8*t begin index of type t (end = begin of t+1), +0xB0+8*t base offset of type t
//             value = buffer + (((desc & 0xFFFFFFF) + base[t]) & 0x7FFFFFFF)   (t: 0 float, 2 vector/color)
//  Property names: global hash map at libunity+0xD0127C0, 24-byte entries {u32 hash, char* name, int id}
//  Shader:    vtable 0xC28FF30, GetName (vt+0x50 = 0x619EC08) -> vt+0xA8 (0x619EBD4):
//             parsed = [shader+0x378]; name = parsed && len ? core::string at parsed+0x80 : core::string at shader+0x40
//  core::string (32 bytes): tag = byte +0x1F; tag < 0x40 -> inline chars at +0, length 0x1F-tag;
//             else heap: char* at +0, length at +0x10
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace material_core {
inline constexpr uint64_t renderer_materials = 0x140, renderer_material_count = 0x150;
inline constexpr uint64_t object_instance_id = 0x08, object_name = 0x20;
inline constexpr uint64_t material_shader_id = 0x28, material_shared = 0x108, shared_sheet = 0x60;
inline constexpr uint64_t sheet_names = 0x10, sheet_desc = 0x30, sheet_data = 0x50, sheet_begin = 0x70, sheet_base = 0xB0;
inline constexpr uint64_t property_map_global = 0xD0127C0;
inline constexpr uint64_t shader_parsed = 0x378, parsed_name = 0x80, shader_own_name = 0x40;
inline constexpr uint32_t max_materials_per_renderer = 16, max_sheet_props = 4096;

inline bool ptr_ok(uint64_t p) { return p >= 0x10000 && p < 0x0000800000000000ULL; }
template <class T, class Read> T val(Read& read, uint64_t a) { T v{}; if (!read(a, &v, sizeof v)) return T{}; return v; }

template <class Read> bool c_string(Read& read, uint64_t p, std::string& out, size_t max = 96) {
    out.clear(); if (!ptr_ok(p)) return false;
    char buf[128]{};
    if (max > sizeof buf - 1) max = sizeof buf - 1;
    if (!read(p, buf, max)) { // tail of a page: retry shorter
        if (!read(p, buf, 32)) return false; max = 32; }
    for (size_t i = 0; i < max; ++i) {
        const unsigned char c = (unsigned char)buf[i];
        if (!c) return i > 0;
        if (c < 0x20 || c > 0x7E) { out.clear(); return false; }
        out.push_back(char(c));
    }
    return !out.empty();
}
template <class Read> std::string object_name_of(Read& read, uint64_t native) {
    std::string s; c_string(read, val<uint64_t>(read, native + object_name), s); return s;
}

// Unity core::string (SSO) -> std::string (printable ASCII only).
template <class Read> bool core_string(Read& read, uint64_t s, std::string& out) {
    out.clear(); if (!ptr_ok(s)) return false;
    uint8_t raw[32]{};
    if (!read(s, raw, sizeof raw)) return false;
    const uint8_t tag = raw[0x1F];
    if (tag < 0x40) {
        const size_t n = size_t(0x1F - tag);
        for (size_t i = 0; i < n; ++i) { const unsigned char c = raw[i]; if (c < 0x20 || c > 0x7E) { out.clear(); return false; } out.push_back(char(c)); }
        return !out.empty();
    }
    uint64_t p, n; std::memcpy(&p, raw, 8); std::memcpy(&n, raw + 0x10, 8);
    if (!ptr_ok(p) || !n || n > 200) return false;
    return c_string(read, p, out, size_t(n));
}
// Shader display name exactly like Shader::GetName (parsed form name, else the object's own string).
template <class Read> std::string shader_name_of(Read& read, uint64_t shader) {
    std::string s;
    const uint64_t parsed = val<uint64_t>(read, shader + shader_parsed);
    if (ptr_ok(parsed) && core_string(read, parsed + parsed_name, s)) return s;
    core_string(read, shader + shader_own_name, s);
    return s;
}

// Property name -> id from the runtime's FastPropertyName table (ids are assigned at runtime).
template <class Read> int property_id(Read& read, uint64_t lib, const char* name) {
    const uint64_t map = val<uint64_t>(read, lib + property_map_global);
    if (!ptr_ok(map)) return -1;
    const uint64_t buckets = val<uint64_t>(read, map); const uint32_t mask = val<uint32_t>(read, map + 8);
    if (!ptr_ok(buckets) || !mask || mask > (1u << 22)) return -1;
    const uint64_t bytes = uint64_t(mask) * 3 + 24;
    std::vector<uint8_t> raw(static_cast<size_t>(bytes));
    for (uint64_t off = 0; off < bytes; off += 0x10000) {
        const size_t n = size_t(bytes - off < 0x10000 ? bytes - off : 0x10000);
        if (!read(buckets + off, raw.data() + off, n)) return -1;
    }
    std::string s;
    for (uint64_t e = 0; e + 24 <= bytes; e += 24) {
        uint32_t h; uint64_t key; int32_t id;
        std::memcpy(&h, &raw[e], 4); std::memcpy(&key, &raw[e + 8], 8); std::memcpy(&id, &raw[e + 16], 4);
        if (h >= 0xFFFFFFFEu || !ptr_ok(key)) continue;
        if (c_string(read, key, s, 48) && s == name) return id;
    }
    return -1;
}

// Address of a property value inside a material's ShaderPropertySheet; 0 if absent.
template <class Read> uint64_t property_address(Read& read, uint64_t material, int type, int id) {
    if (id < 0 || type < 0 || type > 3) return 0;
    const uint64_t smd = val<uint64_t>(read, material + material_shared);
    if (!ptr_ok(smd)) return 0;
    const uint64_t sheet = smd + shared_sheet;
    const uint64_t begin = val<uint64_t>(read, sheet + sheet_begin + 8 * type);
    const uint64_t end = val<uint64_t>(read, sheet + sheet_begin + 8 * (type + 1));
    if (end < begin || end - begin > max_sheet_props || end > max_sheet_props) return 0;
    const uint64_t names = val<uint64_t>(read, sheet + sheet_names);
    const uint64_t desc = val<uint64_t>(read, sheet + sheet_desc);
    const uint64_t data = val<uint64_t>(read, sheet + sheet_data);
    if (!ptr_ok(names) || !ptr_ok(desc) || !ptr_ok(data)) return 0;
    for (uint64_t i = begin; i < end; ++i) {
        if (val<int32_t>(read, names + i * 4) != id) continue;
        const uint64_t d = val<uint64_t>(read, desc + i * 8);
        const uint64_t base = val<uint64_t>(read, sheet + sheet_base + 8 * type);
        const uint64_t off = (d & 0xFFFFFFFULL) + base;
        if (int32_t(uint32_t(off)) < 0) return 0;
        return data + (off & 0x7FFFFFFFULL);
    }
    return 0;
}

struct material_info { int32_t id = 0; uint64_t native = 0; std::string name, shader; bool has_color = false, has_ztest = false; };

// Collects every live Material from the InstanceID map (objects sharing the reference material's vtable).
template <class Read> size_t collect_materials(Read& read, uint64_t lib, uint64_t object_map_global,
                                               uint64_t reference_material, std::vector<material_info>& out,
                                               size_t limit = 1500) {
    out.clear();
    const uint64_t vt = val<uint64_t>(read, reference_material);
    const uint64_t map = val<uint64_t>(read, lib + object_map_global);
    if (!ptr_ok(vt) || !ptr_ok(map)) return 0;
    const uint64_t buckets = val<uint64_t>(read, map); const uint32_t mask = val<uint32_t>(read, map + 8);
    if (!ptr_ok(buckets) || !mask || mask > (1u << 26)) return 0;
    const uint64_t bytes = uint64_t(mask) * 3 + 24;
    std::vector<uint8_t> chunk(0x30000);
    for (uint64_t off = 0; off < bytes && out.size() < limit; off += chunk.size()) {
        const size_t n = size_t(bytes - off < chunk.size() ? bytes - off : chunk.size()) / 24 * 24;
        if (!n || !read(buckets + off, chunk.data(), n)) continue;
        for (size_t e = 0; e + 24 <= n && out.size() < limit; e += 24) {
            uint32_t tag; int32_t id; uint64_t obj;
            std::memcpy(&tag, &chunk[e], 4); std::memcpy(&id, &chunk[e + 8], 4); std::memcpy(&obj, &chunk[e + 16], 8);
            if (tag >= 0xFFFFFFFEu || !ptr_ok(obj)) continue;
            if (val<uint64_t>(read, obj) != vt || val<int32_t>(read, obj + object_instance_id) != id) continue;
            material_info m; m.id = id; m.native = obj; m.name = object_name_of(read, obj);
            out.push_back(std::move(m));
        }
    }
    return out.size();
}
// Write journal: original bytes are restored only while the game did not overwrite them itself.
struct journal {
    struct entry { uint64_t addr; uint8_t size; uint8_t original[16]; uint8_t written[16]; };
    std::vector<entry> entries;
    template<class Read,class Write,class T>
    bool put(Read& read,Write& write,uint64_t a,const T& v) {
        static_assert(sizeof(T)<=16,"small values only");
        entry* e=nullptr; for(auto& x:entries) if(x.addr==a&&x.size==sizeof(T)) e=&x;
        if(!e){ entry n{a,uint8_t(sizeof(T)),{},{}}; if(!read(a,n.original,sizeof(T))) return false; entries.push_back(n); e=&entries.back(); }
        T cur{}; if(read(a,&cur,sizeof(T))&&std::memcmp(&cur,&v,sizeof(T))==0){ std::memcpy(e->written,&v,sizeof(T)); return true; }
        if(!write(a,&v,sizeof(T))) return false;
        std::memcpy(e->written,&v,sizeof(T)); return true;
    }
    template<class Read,class Write>
    size_t restore(Read& read,Write& write) {
        size_t failed=0;
        for(auto it=entries.rbegin();it!=entries.rend();++it){
            uint8_t cur[16]{};
            if(!read(it->addr,cur,it->size)) { ++failed; continue; }
            if(std::memcmp(cur,it->written,it->size)!=0) continue; // game owns it now
            if(!write(it->addr,it->original,it->size)) ++failed;
        }
        entries.clear(); return failed;
    }
};
} // namespace material_core
