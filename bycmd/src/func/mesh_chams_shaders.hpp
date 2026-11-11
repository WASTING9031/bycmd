#pragma once
// Fragment effects adapted from the shader text supplied in skeet_strings.txt.
// CPU skinning feeds world-space position/normal and the mesh UV0 attribute.
namespace mesh_chams::shaders {
inline constexpr const char* vertex = R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
layout(location=2) in vec2 a_uv;
uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec2 u_dim;
uniform float u_expand_px;
out vec2 v_uv;
out vec3 v_normal_world;
out vec3 v_normal_view;
out vec3 v_world_pos;
void main() {
    vec4 view_pos = u_view * vec4(a_position, 1.0);
    vec4 clip = u_proj * view_pos;
    float distance = -view_pos.z;
    v_uv = a_uv;
    v_world_pos = a_position;
    v_normal_world = a_normal;
    v_normal_view = mat3(u_view) * a_normal;
    // Canonical OpenGL depth independent of Unity's graphics-backend Z convention.
    // Preserve clip.w (perspective interpolation) and let GL clip near-plane triangles.
    float near_z = 0.05, far_z = 600.0;
    float ratio = abs(distance) > 0.00001 ? clip.w / distance : 1.0;
    clip.z = ((far_z + near_z) / (far_z - near_z) * distance -
              (2.0 * far_z * near_z) / (far_z - near_z)) * ratio;
    if (u_expand_px > 0.0) {
        vec2 direction = v_normal_view.xy;
        float len = length(direction);
        if (len > 0.0001) clip.xy += direction / len * (2.0 * u_expand_px / max(u_dim, vec2(1.0))) * clip.w;
    }
    gl_Position = clip;
}
)GLSL";
inline constexpr const char* fragment = R"GLSL(#version 300 es
precision highp float;
precision highp int;
uniform int        u_fx_mode;
uniform vec4       u_fx_main_v;
uniform vec4       u_fx_rim_v;
uniform float      u_rim_power;
uniform float      u_time;
uniform vec3       u_light_dir;
uniform vec3       u_eye_world;
uniform int u_outline_pass;
uniform vec4 u_outline_color;
in vec2 v_uv;
in vec3 v_normal_world;
in vec3 v_normal_view;
in vec3 v_world_pos;
out vec4 frag_color;
vec3 hsv2rgb(vec3 c){vec3 p=abs(fract(c.xxx+vec3(1.0,2.0/3.0,1.0/3.0))*6.0-3.0);return c.z*mix(vec3(1.0),clamp(p-1.0,0.0,1.0),c.y);}
float wrap_dist(float a,float b){float d=abs(a-b);return min(d,1.0-d);}
// Adapted from CharacterMultipass.shader in the supplied Zenin archive.
float zen_hash(vec2 p) {
    p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32);
    return fract(p.x*p.y);
}
float zen_noise(vec2 p) {
    vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(zen_hash(i),zen_hash(i+vec2(1.0,0.0)),f.x),
               mix(zen_hash(i+vec2(0.0,1.0)),zen_hash(i+vec2(1.0)),f.x),f.y);
}

void main() {
    if (u_outline_pass != 0) { frag_color = u_outline_color; return; }
    vec4 u_fx_color = u_fx_main_v;
    vec4 u_fx_rim_color = u_fx_rim_v;
    vec3  n  = normalize(v_normal_world);
    vec3  nv = normalize(v_normal_view);
    vec3  l  = normalize(u_light_dir);
    vec3  v  = normalize(u_eye_world - v_world_pos);
    float t  = u_time;
    float ndl = dot(n, l) * 0.5 + 0.5;
    float ndv = clamp(abs(dot(n, v)), 0.0, 1.0);
    if (u_fx_mode == 0) {
        frag_color = u_fx_color; return;
    } else if (u_fx_mode == 1) {
        vec3 col = u_fx_color.rgb * (0.3 + 0.7 * ndl);
        float rim = pow(1.0 - ndv, 3.0);
        col += rim * 0.15;
        frag_color = vec4(col, u_fx_color.a); return;
    } else if (u_fx_mode == 2) {
        vec3 body = u_fx_color.rgb * (0.25 + 0.75 * ndl);
        float fresnel = pow(1.0 - ndv, max(0.01, u_rim_power));
        vec3 rim = u_fx_rim_color.rgb * fresnel * u_fx_rim_color.a;
        vec3 h = normalize(l + v);
        float spec = pow(max(0.0, dot(n, h)), 24.0) * 0.2;
        frag_color = vec4(body + rim + spec, u_fx_color.a); return;
    } else if (u_fx_mode == 3) {
        float fresnel = pow(clamp(1.0 - ndv, 0.0, 1.0), 2.5);
        float glow    = clamp(fresnel * max(0.01, u_rim_power), 0.0, 1.0);
        frag_color = vec4(u_fx_color.rgb, u_fx_color.a * glow); return;
    } else if (u_fx_mode == 4) {
        float scan = sin(gl_FragCoord.y * 1.5 + t * 8.0) * 0.5 + 0.5;
        scan = smoothstep(0.25, 0.75, scan);
        float scan_fine = sin(gl_FragCoord.y * 4.0 - t * 12.0) * 0.5 + 0.5;
        scan = scan * 0.8 + scan_fine * 0.2;
        float flicker = 0.92 + 0.08 * sin(t * 37.0) * sin(t * 7.3 + 2.0);
        float fresnel = pow(1.0 - ndv, 2.0);
        vec3 holo = u_fx_color.rgb;
        holo.r += fresnel * 0.25;
        holo.b += (1.0 - fresnel) * 0.2;
        vec3 col = holo * (0.4 + 0.6 * scan) * flicker;
        col += fresnel * vec3(0.6, 0.8, 1.0) * 0.4;
        float glitch = step(0.97, sin(t * 53.0 + gl_FragCoord.y * 0.1));
        col += glitch * 0.3;
        float alpha = (0.25 + 0.75 * fresnel) * scan * flicker * u_fx_color.a;
        frag_color = vec4(col, alpha); return;
    } else if (u_fx_mode == 5) {
        float tt = t * 0.3;
        vec2 p = v_uv * 4.0 + n.xy * 0.5;
        float n1 = sin(p.x * 3.0 + tt) * cos(p.y * 2.5 - tt * 0.7);
        float n2 = sin(p.x * 5.0 - tt * 1.3 + 2.0) * cos(p.y * 4.0 + tt * 0.5);
        float n3 = sin((p.x + p.y) * 7.0 + tt * 2.0) * 0.5;
        float n4 = sin(p.x * 11.0 + p.y * 9.0 - tt * 1.7) * 0.3;
        float nebula = pow((n1 + n2 + n3 + n4) * 0.125 + 0.5, 1.3);
        float stars = pow(max(0.0, sin(p.x * 23.0 + tt * 0.5) * sin(p.y * 19.0 - tt * 0.3)), 20.0);
        stars += pow(max(0.0, sin(p.x * 31.0 - tt * 0.7) * sin(p.y * 29.0 + tt * 0.4)), 24.0) * 0.7;
        vec3 space = mix(u_fx_color.rgb, u_fx_rim_color.rgb, nebula);
        space += stars * vec3(1.0, 0.95, 0.8) * 0.6;
        float fresnel = pow(1.0 - ndv, 2.5);
        space += fresnel * u_fx_rim_color.rgb * 0.4;
        space *= 0.55 + 0.45 * ndl;
        frag_color = vec4(space, u_fx_color.a); return;
    } else if (u_fx_mode == 6) {
        vec2 matcap = nv.xy * 0.5 + 0.5;
        float chrome  = sin(matcap.x * 6.283) * cos(matcap.y * 6.283) * 0.5 + 0.5;
        float chrome2 = sin(matcap.x * 12.566 + 1.0) * cos(matcap.y * 9.42) * 0.3 + 0.5;
        vec3 h = normalize(l + v);
        float sp_sh = pow(max(0.0, dot(n, h)), 80.0) * 2.0;
        float sp_br = pow(max(0.0, dot(n, h)), 12.0) * 0.4;
        vec3 h2 = normalize(vec3(-l.x, l.y, -l.z) + v);
        float sp_fl = pow(max(0.0, dot(n, h2)), 32.0) * 0.3;
        float fresnel = pow(1.0 - ndv, 3.5);
        vec3 col = u_fx_color.rgb * (0.15 + 0.85 * ndl);
        col = mix(col, u_fx_color.rgb * 1.3, chrome * 0.35 + chrome2 * 0.15);
        col += sp_sh + sp_br + sp_fl + fresnel * u_fx_rim_color.rgb * 0.7;
        frag_color = vec4(col, u_fx_color.a); return;
    } else if (u_fx_mode == 7) {
        float pulse  = sin(t * 3.0) * 0.5 + 0.5;
        float pulse2 = sin(t * 3.0 + 1.5) * 0.5 + 0.5;
        vec3 col = mix(u_fx_color.rgb, u_fx_rim_color.rgb, pulse);
        col *= 0.3 + 0.7 * ndl;
        float fresnel = pow(1.0 - ndv, 2.5);
        col += fresnel * u_fx_rim_color.rgb * pulse2 * 0.7;
        float wave = sin(v_uv.y * 12.0 - t * 4.0) * 0.5 + 0.5;
        col += wave * u_fx_rim_color.rgb * 0.1;
        col *= 0.82 + 0.18 * sin(t * 3.0 + 1.0);
        frag_color = vec4(col, u_fx_color.a); return;
    } else if (u_fx_mode == 8) {
        float hue = fract(t * 0.15 + v_uv.y * 0.4 + v_uv.x * 0.2 + dot(n, v) * 0.3);
        vec3 rgb = hsv2rgb(vec3(hue, 0.9, 1.0)) * (0.35 + 0.65 * ndl);
        float fresnel = pow(1.0 - ndv, 2.5);
        rgb += fresnel * hsv2rgb(vec3(fract(hue + 0.33), 0.7, 1.0)) * 0.4;
        frag_color = vec4(rgb, u_fx_color.a); return;
    } else if (u_fx_mode == 9) {
        float tf = clamp(abs(dot(n, v)), 0.0, 1.0);
        float hue = fract(tf * 2.5 + 0.6);
        vec3 film = hsv2rgb(vec3(hue, 0.6 + 0.4 * (1.0 - tf), 1.0));
        vec3 col = mix(u_fx_color.rgb * (0.2 + 0.8 * ndl), film, 0.6 + 0.4 * (1.0 - tf));
        float fresnel = pow(1.0 - ndv, 4.0);
        col += fresnel * u_fx_rim_color.rgb * 0.6;
        vec3 h = normalize(l + v);
        col += pow(max(0.0, dot(n, h)), 48.0) * 0.8 * hsv2rgb(vec3(fract(hue + 0.15), 0.5, 1.0));
        frag_color = vec4(col, u_fx_color.a); return;
    } else if (u_fx_mode == 10) {
        float fresnel = pow(1.0 - ndv, 2.0);
        vec3 core = u_fx_color.rgb * 1.5 * pow(ndv, 3.0);
        vec3 edge = u_fx_rim_color.rgb * 2.0 * fresnel;
        float flicker = 0.95 + 0.05 * sin(t * 20.0 + v_uv.y * 10.0);
        vec3 col = (core + edge) * (0.5 + 0.5 * ndl) * flicker;
        vec3 h = normalize(l + v);
        col += pow(max(0.0, dot(n, h)), 32.0) * mix(u_fx_color.rgb, u_fx_rim_color.rgb, 0.5) * 0.5;
        frag_color = vec4(col, (0.5 + 0.5 * fresnel) * u_fx_color.a); return;
    } else if (u_fx_mode == 11) {
        vec3 base = u_fx_color.rgb * (0.3 + 0.7 * ndl);
        float band = floor(gl_FragCoord.y / 3.0);
        float seed = fract(sin(band * 43.7 + floor(t * 8.0) * 17.3) * 999.5);
        float glitch_on = step(0.92, seed);
        float strength = (seed - 0.92) * 12.5;
        vec3 col;
        col.r = base.r + glitch_on * strength * 0.6;
        col.g = base.g - glitch_on * strength * 0.2;
        col.b = base.b + glitch_on * strength * 0.4;
        col *= fract(gl_FragCoord.y * 0.5) > 0.5 ? 0.92 : 1.0;
        col += step(0.97, fract(sin(floor(t * 15.0) * 73.1) * 431.5)) * u_fx_color.rgb * 0.6;
        float bs = fract(sin(floor(gl_FragCoord.y / 8.0) * 13.0 + floor(t * 5.0) * 7.0) * 777.0);
        col = mix(col, col.gbr, clamp(step(0.96, bs) * (bs - 0.96) * 400.0, 0.0, 1.0));
        float fresnel = pow(1.0 - ndv, 2.0);
        col.r += fresnel * 0.25;
        col.b += fresnel * 0.15;
        col = floor(col * 24.0 + 0.5) / 24.0;
        frag_color = vec4(col, u_fx_color.a); return;
    } else if (u_fx_mode == 12) {
        vec2 flow = vec2(
            sin(nv.x * 4.0 + t * 1.5) * cos(nv.y * 3.0 - t),
            cos(nv.x * 3.0 - t * 1.2) * sin(nv.y * 4.0 + t * 0.8)
        ) * 0.12;
        vec2 matcap = nv.xy * 0.5 + 0.5 + flow;
        float c1 = sin(matcap.x * 12.566) * cos(matcap.y * 9.42) * 0.5 + 0.5;
        float c2 = sin(matcap.x * 6.28 + 1.5) * cos(matcap.y * 12.566 - 0.7) * 0.5 + 0.5;
        float c3 = sin((matcap.x + matcap.y) * 8.0 + t * 0.5) * 0.5 + 0.5;
        float c4 = sin(matcap.x * 18.0 - matcap.y * 14.0 + t * 0.3) * 0.5 + 0.5;
        float chrome = c1 * 0.3 + c2 * 0.3 + c3 * 0.2 + c4 * 0.2;
        vec3 h = normalize(l + v);
        float sp1 = pow(max(0.0, dot(n, h)), 128.0) * 3.0;
        float sp2 = pow(max(0.0, dot(n, h)), 16.0) * 0.4;
        vec3 h2 = normalize(vec3(-l.x, l.y, -l.z) + v);
        float sp_fl = pow(max(0.0, dot(n, h2)), 48.0) * 0.5;
        float fresnel = pow(1.0 - ndv, 3.5);
        vec3 col = mix(u_fx_color.rgb, u_fx_rim_color.rgb * 1.3, chrome) * (0.2 + 0.8 * ndl);
        col += sp1 + sp2 + sp_fl + fresnel * u_fx_rim_color.rgb * 0.9;
        frag_color = vec4(col, u_fx_color.a); return;
    } else if (u_fx_mode == 13) {
        float fresnel = pow(1.0 - ndv, 2.5);
        vec3 void_col = u_fx_color.rgb * 0.04 * pow(ndv, 2.5);
        vec2 p = v_uv * 8.0 + n.xy * 2.0;
        float s1 = sin(p.x * 2.0 + t * 3.0 + sin(p.y * 3.0 + t) * 2.0) * 0.5 + 0.5;
        float s2 = sin(p.y * 2.5 - t * 2.5 + cos(p.x * 2.0 - t * 1.5) * 2.0) * 0.5 + 0.5;
        float energy = pow((s1 * s2 + (sin((p.x - p.y) * 3.0 + t * 4.0) * 0.5 + 0.5) * 0.3) * fresnel, 0.6);
        vec3 edge = u_fx_rim_color.rgb * energy * 2.5 + fresnel * fresnel * u_fx_rim_color.rgb * 2.0;
        float sp = pow(max(0.0, sin(p.x * 17.0 + t * 7.0) * sin(p.y * 13.0 - t * 5.0)), 20.0);
        edge += sp * fresnel * vec3(1.0, 0.95, 0.9) * 3.0;
        edge *= sin(t * 2.0) * 0.15 + 0.85;
        frag_color = vec4(void_col + edge, max(0.08, max(fresnel * u_fx_color.a, energy * 0.9))); return;
    } else if (u_fx_mode == 14) {
        vec3 base = u_fx_color.rgb * (0.12 + 0.28 * ndl);
        vec2 p = v_uv * 10.0 + n.xy * 3.0;
        float a1 = pow(1.0 - abs(sin(p.x * 8.0 + t * 7.0 + sin(p.y * 5.0 + t * 3.0) * 3.0)), 10.0);
        float a2 = pow(1.0 - abs(sin(p.y * 7.0 - t * 6.0 + cos(p.x * 4.0 - t * 2.5) * 2.5)), 12.0);
        float a3 = pow(1.0 - abs(sin((p.x + p.y) * 5.0 + t * 9.0 + sin(t * 4.0) * 5.0)), 14.0);
        float a4 = pow(1.0 - abs(sin((p.x - p.y * 0.7) * 9.0 - t * 8.0 + cos(t * 2.0 + p.y) * 3.0)), 16.0);
        float arcs = max(max(a1, a2), max(a3, a4));
        float burst = step(0.93, fract(sin(floor(t * 8.0) * 91.7) * 437.3)) * (0.5 + 0.5 * sin(t * 50.0));
        arcs = max(arcs, burst * 0.4);
        vec3 arc_col = u_fx_rim_color.rgb * arcs * 3.0 + arcs * arcs * 0.5;
        float fresnel = pow(1.0 - ndv, 2.0);
        arc_col += fresnel * u_fx_rim_color.rgb * 0.5;
        arc_col += pow(max(0.0, sin(p.x * 30.0 + t * 20.0) * sin(p.y * 25.0 - t * 15.0)), 8.0) * fresnel * u_fx_rim_color.rgb * 1.5;
        frag_color = vec4(base + arc_col, u_fx_color.a); return;
    } else if (u_fx_mode == 15) {
        float fresnel = pow(1.0 - ndv, 4.0);
        vec3 col = mix(u_fx_color.rgb * 0.25 * ndv, u_fx_rim_color.rgb, fresnel);
        vec3 h = normalize(l + v);
        float sp1 = pow(max(0.0, dot(n, h)), 256.0) * 3.5;
        float sp2 = pow(max(0.0, dot(n, h)), 32.0) * 0.25;
        vec3 l2 = normalize(vec3(-0.5, 0.8, -0.3));
        vec3 h2 = normalize(l2 + v);
        float sp3 = pow(max(0.0, dot(n, h2)), 160.0) * 1.2;
        col += (sp1 + sp2 + sp3) * vec3(1.0, 0.97, 0.93);
        col += (sin(nv.x * 8.0 + t * 1.2) * sin(nv.y * 6.0 - t * 0.8) * 0.5 + 0.5) * u_fx_rim_color.rgb * 0.06;
        col += (nv.y * 0.5 + 0.5) * u_fx_rim_color.rgb * fresnel * 0.25;
        float alpha = (0.06 + fresnel * 0.75 + (sp1 + sp3) * 0.25) * u_fx_color.a;
        frag_color = vec4(col, alpha); return;
    }
    else if (u_fx_mode == 16) {
        float noise=zen_noise(v_uv*9.0+vec2(t,-t*.7));
        float bands=pow(clamp(.5+.5*sin((v_uv.x+v_uv.y)*30.0-t*3.0),0.0,1.0),5.0);
        float foam=clamp(noise*.55+bands,0.0,1.0);
        vec3 col=mix(u_fx_color.rgb*.12,u_fx_color.rgb*(.55+foam),.85)+foam*.35;
        col+=pow(max(0.0,dot(n,normalize(l+v))),80.0)*.7;
        frag_color=vec4(col,u_fx_color.a); return;
    } else if (u_fx_mode == 17) {
        float noise=zen_noise(v_uv*20.0+vec2(t*.25,-t*.18));
        float vein=1.0-smoothstep(.025,.15,abs(noise-.5));
        float heat=clamp(.8*(vein*.9+noise*.25),0.0,1.0);
        vec3 hot=u_fx_rim_color.rgb;
        vec3 col=mix(u_fx_color.rgb*.1,hot*2.2,heat)+vein*hot;
        frag_color=vec4(col,u_fx_color.a); return;
    } else if (u_fx_mode == 18) {
        float noise=zen_noise(v_uv*20.0);
        float crack=(1.0-smoothstep(.02,.09,abs(noise-.5)))*.5;
        float fresnel=pow(1.0-ndv,3.0);
        float frost=clamp(.6*(.4+fresnel+noise*.35),0.0,1.0);
        vec3 col=mix(u_fx_color.rgb*.3,vec3(.72,.9,1.0),frost);
        col=mix(col,vec3(.95,.99,1.0),crack*.8);
        col+=pow(max(0.0,dot(n,normalize(l+v))),80.0)*.45;
        frag_color=vec4(col,u_fx_color.a); return;
    } else if (u_fx_mode == 19) {
        vec2 grid=v_uv*24.0, cell=floor(grid), local=fract(grid);
        float trail=fract(t+zen_hash(vec2(cell.x,0.0))*4.0-cell.y*.08);
        float glyph=step(.56,zen_hash(floor(local*vec2(4.0,6.0))+cell+floor(t*2.0)));
        float rain=clamp((1.0-trail)*2.5,0.0,1.0)*glyph;
        vec3 col=u_fx_color.rgb*(.05+rain*1.7)+vec3(.15,1.0,.3)*rain*.8;
        frag_color=vec4(col,clamp(u_fx_color.a*(.06+rain),0.0,1.0)); return;
    }
    frag_color = u_fx_color;
}
)GLSL";
}
