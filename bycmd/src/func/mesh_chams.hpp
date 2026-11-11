#pragma once
// External player-mesh chams. No injection, no new materials inside Unity.
// Always through walls: this context has only the overlay depth buffer.
// Runtime mesh/bone access is experimental until verified on the target device.
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>
#include "imgui.h"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/mesh_core.hpp"
#include "../ui/cfg.hpp"
#include "skeleton.hpp"
#include "mesh_chams_shaders.hpp"

namespace mesh_chams {
inline char g_status[256] = "Mesh chams off (experimental)";
inline constexpr const char* effect_names[] = {
    "Flat", "Shaded", "Glow rim", "Fresnel glow", "Hologram", "Galaxy", "Metallic", "Pulse",
    "Rainbow", "Iridescent", "Neon", "Glitch", "Liquid Chrome", "Void Energy", "Electric", "Glass",
    "Zenin Water", "Zenin Lava", "Zenin Ice", "Zenin Matrix"
};
inline constexpr int effect_count = sizeof(effect_names) / sizeof(effect_names[0]);
namespace detail {
inline auto reader() { return [](uint64_t a, void* b, size_t n) { return mem_read(a, b, n); }; }
struct mesh_entry { mesh_core::mesh data; long long retry = 0, used = 0; bool ok = false; };
struct bone_ref { uint64_t hierarchy = 0; int index = -1; };
struct bone_entry {
    long long refreshed = 0, used = 0;
    uint64_t array = 0, count = 0;
    std::vector<bone_ref> refs;
};
inline std::unordered_map<uint64_t, mesh_entry> meshes;
inline std::unordered_map<uint64_t, bone_entry> bones;
struct hierarchy_frame { std::vector<mesh_core::mat4> world; std::vector<uint8_t> valid; };
inline std::unordered_map<uint64_t, hierarchy_frame> hierarchy;
struct item { std::vector<mesh_core::gpu_vertex> vertices; std::vector<uint32_t> indices; };
inline std::vector<item> items;
inline size_t item_count = 0;
inline int session_pid = -1; inline uint64_t session_lib = 0, session_world = 0;
inline int failed = 0, matched = 0, expected = 0;
inline const char* error = nullptr;
inline float view[16]{}, projection[16]{};
inline mesh_core::vec3 eye{};
inline bool gl_ready = false, gl_failed = false;
inline GLuint program = 0, vao = 0, vbo = 0, ebo = 0;
struct uniforms {
    GLint view, proj, dim, expand, eye, main, rim, power, time, light, effect, outline, outline_color;
};
inline uniforms u{};
inline void fail(const char* reason) { ++failed; error = reason; }

inline GLuint compile(GLenum type, const char* text) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &text, nullptr); glCompileShader(id);
    GLint ok = 0; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]{}; glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        diag::log("CHAMS", "GL shader compile FAILED: %s", log); glDeleteShader(id); return 0;
    }
    return id;
}
inline bool init_gl() {
    if (gl_ready || gl_failed) return gl_ready;
    GLint depth_bits = 0; glGetIntegerv(GL_DEPTH_BITS, &depth_bits);
    if (depth_bits < 1) { diag::log("CHAMS", "GL FAILED: overlay has no depth buffer"); gl_failed = true; return false; }
    const GLuint vertex = compile(GL_VERTEX_SHADER, shaders::vertex);
    const GLuint fragment = compile(GL_FRAGMENT_SHADER, shaders::fragment);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex); if (fragment) glDeleteShader(fragment);
        gl_failed = true; return false;
    }
    program = glCreateProgram(); glAttachShader(program, vertex); glAttachShader(program, fragment);
    glLinkProgram(program); glDeleteShader(vertex); glDeleteShader(fragment);
    GLint ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]{}; glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        diag::log("CHAMS", "GL program link FAILED: %s", log);
        glDeleteProgram(program); program = 0; gl_failed = true; return false;
    }
    auto loc = [](const char* name) -> GLint { return GLint(glGetUniformLocation(program, name)); };
    u = {loc("u_view"), loc("u_proj"), loc("u_dim"), loc("u_expand_px"), loc("u_eye_world"),
         loc("u_fx_main_v"), loc("u_fx_rim_v"), loc("u_rim_power"), loc("u_time"), loc("u_light_dir"),
         loc("u_fx_mode"), loc("u_outline_pass"), loc("u_outline_color")};
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(mesh_core::gpu_vertex), nullptr);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(mesh_core::gpu_vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(mesh_core::gpu_vertex), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    if (!vao || !vbo || !ebo) { diag::log("CHAMS", "GL FAILED: buffer allocation"); gl_failed = true; return false; }
    gl_ready = true;
    diag::log("CHAMS", "GLES3 ready: %d shader effects, CPU skinning, no map depth / no vischeck", effect_count);
    return true;
}

// Cache native bone mappings; read TRS/parents in bulk rather than syscall per bone per frame.
inline bool skin_matrices(uint64_t smr, const mesh_core::mesh& mesh, std::vector<mesh_core::mat4>& out) {
    const long long now = diag::now_ms(); auto read = reader();
    const uint64_t array = rpm<uint64_t>(smr + mesh_core::smr_bones);
    const uint64_t count = rpm<uint64_t>(smr + mesh_core::smr_bones_size);
    if (!mesh_core::ptr_ok(array) || !count || count > mesh_core::max_bones) return false;
    auto& entry = bones[smr]; entry.used = now;
    if (entry.refs.size() != mesh.bindposes.size() || entry.array != array || entry.count != count || now - entry.refreshed >= 2000) {
        entry.array = array; entry.count = count; entry.refreshed = now;
        entry.refs.assign(mesh.bindposes.size(), bone_ref{});
        std::vector<int32_t> ids(static_cast<size_t>(count));
        if (!mem_read(array, ids.data(), ids.size() * sizeof(int32_t))) { entry.refs.clear(); return false; }
        for (size_t b = 0; b < entry.refs.size() && b < ids.size(); ++b) {
            const uint64_t transform = mesh_core::resolve_id(read, proc::lib, ids[b]);
            if (!transform) continue;
            const uint64_t h = rpm<uint64_t>(transform + mesh_core::tr_hierarchy);
            const int index = rpm<int32_t>(transform + mesh_core::tr_index);
            if (mesh_core::ptr_ok(h) && index >= 0 && index <= 2048) entry.refs[b] = {h, index};
        }
    }
    // Each shared hierarchy is read once, up to the largest index any cached renderer uses.
    for (const auto& ref : entry.refs) {
        if (!ref.hierarchy) continue;
        auto found = hierarchy.find(ref.hierarchy);
        if (found != hierarchy.end() && int(found->second.world.size()) > ref.index) continue;
        int max_index = ref.index;
        for (const auto& pair : bones) for (const auto& r : pair.second.refs)
            if (r.hierarchy == ref.hierarchy) max_index = std::max(max_index, r.index);
        const size_t n = size_t(max_index + 1);
        auto& frame = hierarchy[ref.hierarchy]; frame.world.assign(n, mesh_core::mat4{}); frame.valid.assign(n, 0);
        const uint64_t trs = rpm<uint64_t>(ref.hierarchy + mesh_core::h_trs), parents_ptr = rpm<uint64_t>(ref.hierarchy + mesh_core::h_parents);
        if (!mesh_core::ptr_ok(trs) || !mesh_core::ptr_ok(parents_ptr)) continue;
        std::vector<float> data(n * 12); std::vector<int32_t> parents(n);
        if (!mem_read(trs, data.data(), data.size() * sizeof(float)) || !mem_read(parents_ptr, parents.data(), n * sizeof(int32_t))) continue;
        for (size_t i = 0; i < n; ++i) {
            const float* t = &data[i * 12]; const int parent = parents[i];
            bool finite = true; for (int k : {0,1,2,4,5,6,7,8,9,10}) finite &= std::isfinite(t[k]);
            if (!finite || parent < -1 || parent >= int(i)) continue;
            const auto local = mesh_core::trs({t[0],t[1],t[2]}, {t[4],t[5],t[6],t[7]}, {t[8],t[9],t[10]});
            if (parent < 0) frame.world[i] = local;
            else { if (!frame.valid[parent]) continue; frame.world[i] = mesh_core::mul(frame.world[parent], local); }
            bool good = true; for (float f : frame.world[i].m) good &= std::isfinite(f);
            frame.valid[i] = good ? 1 : 0;
        }
    }
    out.assign(mesh.bindposes.size(), mesh_core::mat4{});
    int good = 0;
    for (size_t b = 0; b < entry.refs.size(); ++b) {
        const auto& r = entry.refs[b]; const auto f = hierarchy.find(r.hierarchy);
        if (f == hierarchy.end() || r.index < 0 || size_t(r.index) >= f->second.valid.size() || !f->second.valid[r.index]) continue;
        out[b] = mesh_core::mul(f->second.world[r.index], mesh.bindposes[b]); ++good;
    }
    matched += good; expected += int(out.size());
    return good > 0 && good * 10 >= int(out.size()) * 9;
}

inline void add_renderer(uint64_t managed, const Vector3& hip) {
    if (!plausible(managed)) { fail("CharacterRenderer missing"); return; }
    const uint64_t native = rpm<uint64_t>(managed + v100::unity_cached_ptr);
    if (!mesh_core::ptr_ok(native)) { fail("native SkinnedMeshRenderer invalid"); return; }
    auto read = reader();
    const uint64_t mesh_ptr = mesh_core::resolve_id(read, proc::lib, rpm<int32_t>(native + mesh_core::smr_mesh_id));
    if (!mesh_ptr) { fail("mesh InstanceID not resolved"); return; }
    const long long now = diag::now_ms();
    if (!meshes.count(mesh_ptr) && meshes.size() >= 8) {
        auto oldest = meshes.begin();
        for (auto it = meshes.begin(); it != meshes.end(); ++it) if (it->second.used < oldest->second.used) oldest = it;
        meshes.erase(oldest);
    }
    auto& cache = meshes[mesh_ptr]; cache.used = now;
    if (!cache.ok && now >= cache.retry) {
        cache.ok = mesh_core::read_mesh(read, mesh_ptr, cache.data); cache.retry = now + 5000;
        diag::log("CHAMS", "mesh=0x%llx %s verts=%zu tris=%zu bones=%zu skin=%d UV=%zu err=%s",
                  (unsigned long long)mesh_ptr, cache.ok ? "read OK" : "read FAILED", cache.data.pos.size(),
                  cache.data.indices.size()/3, cache.data.bindposes.size(), int(cache.data.has_skin), cache.data.uv.size(),
                  cache.data.error ? cache.data.error : "-");
    }
    if (!cache.ok) { fail(cache.data.error ? cache.data.error : "mesh read failed"); return; }
    if (!cache.data.has_skin) { fail("mesh has no readable weights/bindposes"); return; }
    std::vector<mesh_core::mat4> matrices;
    if (!skin_matrices(native, cache.data, matrices)) { fail("bone mapping failed (<90% resolved)"); return; }
    if (item_count >= 16) { fail("per-frame renderer limit (16)"); return; }
    if (item_count == items.size()) items.emplace_back();
    auto& item = items[item_count]; mesh_core::vec3 center{};
    if (!mesh_core::skin(cache.data, matrices, item.vertices, center)) { fail("skinning: no finite vertices"); return; }
    const float dx = center.x-hip.x, dy = center.y-hip.y, dz = center.z-hip.z;
    if (!mesh_core::finite3(center) || dx*dx+dy*dy+dz*dz > 16.f) { fail("mesh far from player (bad mapping)"); return; }
    mesh_core::face_normals(cache.data, item.vertices);
    item.indices.clear(); item.indices.reserve(cache.data.indices.size());
    auto valid = [&](uint32_t index) {
        const auto& p = item.vertices[index];
        const float x=p.px-hip.x, y=p.py-hip.y, z=p.pz-hip.z;
        return std::isfinite(x+y+z+p.nx+p.ny+p.nz) && x*x+y*y+z*z < 36.f;
    };
    for (size_t k = 0; k + 2 < cache.data.indices.size(); k += 3) {
        const uint32_t a=cache.data.indices[k], b=cache.data.indices[k+1], c=cache.data.indices[k+2];
        if (valid(a) && valid(b) && valid(c)) item.indices.insert(item.indices.end(), {a,b,c});
    }
    if (item.indices.empty() || item.indices.size()*10 < cache.data.indices.size()*9) { fail("mesh contains invalid/exploded triangles"); return; }
    ++item_count;
}

inline void upload(const item& item) {
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(item.vertices.size()*sizeof(mesh_core::gpu_vertex)), item.vertices.data(), GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(item.indices.size()*sizeof(uint32_t)), item.indices.data(), GL_STREAM_DRAW);
}
inline void draw(const item& item) { glDrawElements(GL_TRIANGLES, GLsizei(item.indices.size()), GL_UNSIGNED_INT, nullptr); }
inline void status(int submitted) {
    std::snprintf(g_status, sizeof g_status, "mesh ready=%zu skipped=%d bones=%d/%d GL=%s submitted=%d%s%s",
        item_count, failed, matched, expected, gl_failed ? "FAILED" : gl_ready ? "ready" : "pending", submitted,
        error ? " | " : "", error ? error : "");
}
inline void render_cb(const ImDrawList*, const ImDrawCmd*) {
    if (!item_count || !cfg::mesh_chams::enabled) return;
    if (!init_gl()) { status(0); return; }
    // ResetRenderState restores the ImGui pipeline; preserve depth/color state it does not set.
    GLboolean old_mask[4]{}, old_depth_mask{}; GLint old_func = 0;
    GLfloat old_range[2]{}, old_clear = 1;
    glGetBooleanv(GL_COLOR_WRITEMASK, old_mask); glGetBooleanv(GL_DEPTH_WRITEMASK, &old_depth_mask);
    glGetIntegerv(GL_DEPTH_FUNC, &old_func); glGetFloatv(GL_DEPTH_RANGE, old_range); glGetFloatv(GL_DEPTH_CLEAR_VALUE, &old_clear);
    const auto& io=ImGui::GetIO(); const float sx=io.DisplayFramebufferScale.x, sy=io.DisplayFramebufferScale.y;
    glViewport(0, int((io.DisplaySize.y-g_sh)*sy), int(g_sw*sx), int(g_sh*sy));
    glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE); glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST); glDepthRangef(0,1); glDepthMask(GL_TRUE); glClearDepthf(1); glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(program); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glUniformMatrix4fv(u.view,1,GL_FALSE,view); glUniformMatrix4fv(u.proj,1,GL_FALSE,projection);
    glUniform2f(u.dim,g_sw,g_sh); glUniform3f(u.eye,eye.x,eye.y,eye.z);
    glUniform3f(u.light,.4f,.8f,.5f); glUniform1f(u.time,float(diag::now_ms()%1000000)/1000.f);
    const auto& col=cfg::mesh_chams::color; const auto& rim=cfg::mesh_chams::rim_color;
    glUniform4f(u.main,col.x,col.y,col.z,col.w*std::clamp(cfg::mesh_chams::opacity,0.f,1.f));
    glUniform4f(u.rim,rim.x,rim.y,rim.z,rim.w); glUniform1f(u.power,cfg::mesh_chams::rim_power);
    glUniform1i(u.effect,std::clamp(cfg::mesh_chams::effect,0,effect_count-1));
    glUniform1i(u.outline,0); glUniform1f(u.expand,0);
    // Global depth prepass first: correct nearest surface across ALL player meshes.
    glDisable(GL_BLEND); glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE); glDepthFunc(GL_LESS);
    for (size_t i=0;i<item_count;++i) { upload(items[i]); draw(items[i]); }
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glDepthMask(GL_FALSE); glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    for (size_t i=0;i<item_count;++i) { upload(items[i]); draw(items[i]); }
    if (cfg::mesh_chams::outline && cfg::mesh_chams::outline_px>0) {
        const auto& c=cfg::mesh_chams::outline_color;
        glUniform1i(u.outline,1); glUniform4f(u.outline_color,c.x,c.y,c.z,c.w);
        glUniform1f(u.expand,std::clamp(cfg::mesh_chams::outline_px,1.f,6.f));
        // Expanded fragments only outside the original global depth silhouette.
        glDepthFunc(GL_LESS);
        for (size_t i=0;i<item_count;++i) { upload(items[i]); draw(items[i]); }
    }
    glBindVertexArray(0); glUseProgram(0); glDisable(GL_DEPTH_TEST);
    glColorMask(old_mask[0],old_mask[1],old_mask[2],old_mask[3]); glDepthMask(old_depth_mask);
    glDepthFunc(GLenum(old_func)); glDepthRangef(old_range[0],old_range[1]); glClearDepthf(old_clear);
    const GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        error = "OpenGL draw error (see CHAMS log)";
        static long long gl_last=0;
        if (diag::every(gl_last,5000)) diag::log("CHAMS","GL draw error=0x%x",unsigned(gl_error));
    }
    status(gl_error == GL_NO_ERROR ? int(item_count) : 0);
    static long long last=0;
    if (diag::every(last,5000)) diag::log("CHAMS","%s (submitted=GL draw calls, not verified pixels)",g_status);
}
} // namespace detail

inline void waiting() {
    std::snprintf(g_status,sizeof g_status,cfg::mesh_chams::enabled ? "Mesh chams: waiting for players / camera" : "Mesh chams off (experimental)");
    detail::item_count=0;
    if(cfg::mesh_chams::enabled) { static long long last=0; if(diag::every(last,5000)) diag::log("CHAMS", "%s", g_status); }
}
inline void begin_frame(const matrix& v,const matrix& p,uint64_t world=0) {
    using namespace detail;
    if (session_pid!=proc::pid || session_lib!=proc::lib || session_world!=world) {
        meshes.clear(); bones.clear(); items.clear(); session_pid=proc::pid; session_lib=proc::lib; session_world=world;
    }
    item_count=0; failed=matched=expected=0; error=nullptr; hierarchy.clear();
    std::memcpy(view,&v,sizeof view); std::memcpy(projection,&p,sizeof projection);
    eye={-(view[0]*view[12]+view[1]*view[13]+view[2]*view[14]),
         -(view[4]*view[12]+view[5]*view[13]+view[6]*view[14]),
         -(view[8]*view[12]+view[9]*view[13]+view[10]*view[14])};
}
inline void submit_enemy(uint64_t player,const Vector3& hip) {
    if (!cfg::mesh_chams::enabled) return;
    std::vector<uint64_t> bindings; skeleton::detail::bindings_of(player,bindings);
    if (bindings.empty()) { detail::fail("CharacterBindings not found"); return; }
    std::vector<uint64_t> seen;
    for (uint64_t b:bindings) {
        const uint64_t r=rpm<uint64_t>(b+0x38);
        if (std::find(seen.begin(),seen.end(),r)!=seen.end()) continue;
        seen.push_back(r); detail::add_renderer(r,hip);
    }
}
inline void end_frame() {
    using namespace detail;
    status(0);
    for (auto it=bones.begin();it!=bones.end();) {
        if (diag::now_ms()-it->second.used>10000) it=bones.erase(it); else ++it;
    }
    if (item_count) {
        auto* dl=ImGui::GetBackgroundDrawList();
        dl->AddCallback(render_cb,nullptr); dl->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
    } else {
        static long long last=0; if (diag::every(last,5000)) diag::log("CHAMS","%s",g_status);
    }
}
inline void shutdown() {
    using namespace detail;
    if (program) glDeleteProgram(program);
    if (vbo) glDeleteBuffers(1,&vbo); if (ebo) glDeleteBuffers(1,&ebo); if (vao) glDeleteVertexArrays(1,&vao);
    program=vao=vbo=ebo=0; gl_ready=gl_failed=false; items.clear(); meshes.clear(); bones.clear(); hierarchy.clear(); item_count=0;
}
} // namespace mesh_chams
