#pragma once
#include "esp_style.hpp"
#include "weapon_icons.hpp"
#include "../ui/esp_font.hpp"
#include "../game/math.hpp"
#include "../game/grenade_visual_data.hpp"
#include <limits>
namespace world_visuals {
inline bool grenades_enabled() {return cfg::world_esp::grenade_tracking||cfg::world_esp::grenade_prediction||cfg::world_esp::grenade_trail||cfg::world_esp::grenade_times||cfg::world_esp::grenade_effects||cfg::world_esp::effect_times;}
inline float size(){return std::clamp(cfg::esp::font_size,12.f,22.f);}
inline void caption(ImDrawList* d,ImVec2 p,const char* text,ImVec4 color) {
    const auto z=esp_style::measure(esp_fonts_ui::current(),size(),text);
    esp_style::label(d,esp_fonts_ui::current(),size(),{p.x-z.x*.5f,p.y},color,1.f,text);
}
// These are pickup objects, not flying projectiles or active effects.
inline bool drops_enabled() {
    return (cfg::world_esp::dropped_icons||cfg::world_esp::dropped_names) &&
      (cfg::world_esp::bomb_dropped||cfg::world_esp::weapon_dropped||cfg::world_esp::dropped_smoke||
       cfg::world_esp::dropped_molotov||cfg::world_esp::dropped_flash||cfg::world_esp::dropped_he);
}
inline bool marker_enabled(const bomb_visual_data::marker& m) {
    if(m.kind!=bomb_visual_data::marker_kind::dropped)return false;
    switch(m.raw){
    case 100:return cfg::world_esp::bomb_dropped;
    case 91:return cfg::world_esp::dropped_he;
    case 92:return cfg::world_esp::dropped_smoke;
    case 93:return cfg::world_esp::dropped_flash;
    case 94:case 95:case 97:return cfg::world_esp::dropped_molotov;
    default:break;
    }
    const auto* w=weapon_catalog::lookup(m.raw);if(!w)return false;
    using K=weapon_catalog::icon_kind;
    switch(w->kind){case K::pistol:case K::smg:case K::rifle:case K::sniper:
    case K::shotgun:case K::machinegun:case K::knife:return cfg::world_esp::weapon_dropped;default:return false;}
}
inline void item(ImDrawList* d,ImVec2 at,uint8_t raw) {
    const auto* weapon=weapon_catalog::lookup(raw);if(!weapon)return;
    const float width=raw==100?48.f:64.f,height=width*.375f;
    if(cfg::world_esp::dropped_icons)weapon_icons::draw(d,weapon,{at.x-width*.5f,at.y-height*.5f},width,raw==100?IM_COL32(255,184,82,255):IM_COL32(240,244,250,255));
    if(cfg::world_esp::dropped_names)caption(d,{at.x,at.y+(cfg::world_esp::dropped_icons?height*.5f+4.f:-size()*.5f)},weapon->name,{1,1,1,1});
}
inline void markers(ImDrawList* d,const bomb_visual_data::snapshot& frame,const matrix& view,const matrix& projection) {
    if(!drops_enabled())return;
    for(const auto& m:frame.markers)if(marker_enabled(m)){
        ImVec2 at{};if(!world_to_screen(m.position,view,projection,at))continue;
        item(d,at,m.raw);
    }
}
inline void polyline(ImDrawList* d,const std::vector<Vector3>& points,const matrix& v,const matrix& p,ImU32 color,bool dashed,float scale=1.f) {
    float phase=0;
    for(size_t i=1;i<points.size();++i){ImVec2 a{},b{};if(!world_to_screen(points[i-1],v,p,a)||!world_to_screen(points[i],v,p,b))continue;
        if(!dashed){d->AddLine(a,b,color,2.f*scale);continue;}
        const float dx=b.x-a.x,dy=b.y-a.y,len=std::sqrt(dx*dx+dy*dy);if(len<.1f||len>10000)continue;
        for(float t=0;t<len;){const float in=std::fmod(phase+t,12.f);const bool ink=in<6.f;
            const float end=std::min(len,t+std::max(.01f,(ink?6.f:12.f)-in));
            if(ink)d->AddLine({a.x+dx*t/len,a.y+dy*t/len},{a.x+dx*end/len,a.y+dy*end/len},color,1.5f*scale);t=end;}
        phase=std::fmod(phase+len,12.f);
    }
}
inline const char* grenade_name(uint8_t raw) {
    switch(raw){case 91:return "HE";case 92:return "SMOKE";case 93:return "FLASH";case 94:return "MOLOTOV";case 95:return "INCENDIARY";case 97:return "MOLOTOV";default:return "GRENADE";}
}
inline ImU32 grenade_color(uint8_t raw,float a=1.f) {
    const int al=int(255*std::clamp(a,0.f,1.f));
    switch(raw){case 91:return IM_COL32(255,92,78,al);case 92:return IM_COL32(168,196,232,al);case 93:return IM_COL32(255,226,110,al);default:return IM_COL32(255,150,60,al);}
}
inline float grenade_scale(){return std::isfinite(cfg::world_esp::grenade_scale)?std::clamp(cfg::world_esp::grenade_scale,.5f,2.f):1.f;}
inline float grenade_text_size(){const float base=std::isfinite(cfg::esp::font_size)?std::clamp(cfg::esp::font_size,12.f,22.f):16.f;return base*grenade_scale();}
inline void grenade_caption(ImDrawList* d,ImVec2 at,const char* text,ImVec4 col){
    const float scale=grenade_scale(),sz=grenade_text_size();auto* font=esp_fonts_ui::current();
    const auto z=esp_style::measure(font,sz,text);const ImVec2 p(at.x-z.x*.5f,at.y);
    // Grenade background is independent of the player/dropped-item label background.
    if(cfg::world_esp::grenade_background)d->AddRectFilled({p.x-3*scale,p.y-scale},{p.x+z.x+3*scale,p.y+z.y+scale},IM_COL32(17,19,26,200),2*scale);
    esp_style::text(d,font,sz,p,ImGui::ColorConvertFloat4ToU32(col),text);
}
// Scalable round badge. Tracking controls the badge, times controls only the countdown.
inline void timer_badge(ImDrawList* d,ImVec2 at,uint8_t raw,grenade_math::timer clock,bool limit,const char* name=nullptr,float elapsed=-1.f,bool show_time=true,float meters=std::numeric_limits<float>::quiet_NaN()) {
    const float sc=grenade_scale(),r=19.f*sc;const ImVec2 c(std::round(at.x),std::round(at.y-r-4*sc));
    if(cfg::world_esp::grenade_background){
        d->AddCircleFilled({c.x,c.y+1.5f*sc},r+2*sc,IM_COL32(0,0,0,90),40);
        d->AddCircleFilled(c,r,IM_COL32(17,19,26,228),40);
        d->AddCircle(c,r,IM_COL32(255,255,255,30),40,sc);
    }
    if(show_time){
        const float frac=clock.valid&&std::isfinite(clock.total)&&std::isfinite(clock.remaining)&&clock.total>0?std::clamp(clock.remaining/clock.total,0.f,1.f):1.f;
        const float a0=-1.5707963f,a1=a0+6.2831853f*frac;
        d->PathArcTo(c,r+.5f*sc,a0,a0+6.2831853f,48);d->PathStroke(IM_COL32(60,64,76,200),0,2.5f*sc);
        if(frac>.001f){d->PathArcTo(c,r+.5f*sc,a0,a1,48);d->PathStroke(grenade_color(raw),0,2.5f*sc);}
    }
    if(const auto* w=weapon_catalog::lookup(raw)){const float iw=r*3.3f,ih=iw*.375f;
        weapon_icons::draw(d,w,{c.x-iw*.5f,c.y-ih*.5f},iw,IM_COL32(236,240,248,255));}
    float y=c.y+r+5*sc;const float line=grenade_text_size()+2*sc;
    if(cfg::world_esp::grenade_name){grenade_caption(d,{c.x,y},name?name:grenade_name(raw),{1,1,1,1});y+=line;}
    if(show_time){
        if(clock.valid&&std::isfinite(clock.remaining)){char t[24];std::snprintf(t,sizeof t,"%s%.1fs",limit?"<":"",std::max(0.f,clock.remaining));grenade_caption(d,{c.x,y},t,ImGui::ColorConvertU32ToFloat4(grenade_color(raw)));y+=line;}
        else if(std::isfinite(elapsed)&&elapsed>=0){char t[32];std::snprintf(t,sizeof t,"in air %.1fs",elapsed);grenade_caption(d,{c.x,y},t,{.8f,.85f,.9f,1});y+=line;}
    }
    if(cfg::world_esp::grenade_distance&&std::isfinite(meters)&&meters>=0){char t[32];std::snprintf(t,sizeof t,"%.0f m",meters);grenade_caption(d,{c.x,y},t,{.85f,.89f,.95f,1});}
}
// kept for callers that used the old signature
inline void timer_label(ImDrawList* d,ImVec2 at,uint8_t raw,grenade_math::timer clock,bool limit,const char*) {timer_badge(d,at,raw,clock,limit);}
inline void grenades(ImDrawList* d,const grenade_visual_data::snapshot& frame,const matrix& v,const matrix& p,Vector3 local) {
    const float scale=grenade_scale();
    for(const auto& f:frame.projectiles){
        if(cfg::world_esp::grenade_trail)polyline(d,f.trail,v,p,IM_COL32(105,202,255,235),false,scale);
        if(cfg::world_esp::grenade_prediction){polyline(d,f.path,v,p,IM_COL32(255,201,113,235),true,scale);
            if(!f.path.empty()){ImVec2 end{};if(world_to_screen(f.path.back(),v,p,end))grenade_caption(d,{end.x,end.y+8*scale},"~ trajectory",{1,.8f,.44f,1});}}
        ImVec2 at{};if(!world_to_screen(f.position,v,p,at))continue;
        // shown while the grenade is still flying: type + fuse countdown (or time in air when the fuse is unknown)
        if(cfg::world_esp::grenade_times||cfg::world_esp::grenade_tracking)
            timer_badge(d,at,f.raw,f.fuse,f.fuse_is_limit,nullptr,f.elapsed,cfg::world_esp::grenade_times,calculate_distance(f.position,local));
    }
    for(const auto& e:frame.effects){
        std::vector<Vector3> ring;
        if(cfg::world_esp::grenade_effects&&e.reference_radius>0&&std::isfinite(e.reference_radius)&&e.reference_radius<=100){
            for(int i=0;i<=64;++i){const float a=i*6.28318530718f/64;
                ring.push_back(e.position+Vector3(std::cos(a)*e.reference_radius,.04f,std::sin(a)*e.reference_radius));}
            const ImU32 color=e.raw==92?IM_COL32(145,191,239,210):e.raw==93?IM_COL32(240,221,155,210):IM_COL32(255,146,99,210);
            polyline(d,ring,v,p,color,true);
        }
        if(cfg::world_esp::grenade_effects&&!e.flames.empty()){
            const auto h=grenade_math::hull(e.flames);
            // Envelope of real flame centers, not the exact damage/collision boundary.
            if(h.size()>=3){ImVec2 a{};if(world_to_screen(h[0],v,p,a))for(size_t i=1;i+1<h.size();++i){ImVec2 b{},c{};
                if(world_to_screen(h[i],v,p,b)&&world_to_screen(h[i+1],v,p,c))d->AddTriangleFilled(a,b,c,IM_COL32(255,124,43,42));}}
            if(h.size()>1){auto closed=h;closed.push_back(h.front());polyline(d,closed,v,p,IM_COL32(255,155,63,220),false);}
            for(auto flame:e.flames){ImVec2 at{};if(world_to_screen(flame,v,p,at))d->AddCircleFilled(at,2.5f,IM_COL32(255,190,96,240));}
        }
        ImVec2 at{};if(!world_to_screen(e.position,v,p,at))continue;
        if(cfg::world_esp::effect_times){
            float top=at.y;for(auto flame:e.flames){ImVec2 point{};if(world_to_screen(flame,v,p,point))top=std::min(top,point.y);}
            for(auto edge:ring){ImVec2 point{};if(world_to_screen(edge,v,p,point))top=std::min(top,point.y);}
            at.y=std::max(58.f*scale,top-(ring.empty()?grenade_text_size()+30.f*scale:2.f*grenade_text_size()+48.f*scale));
            const char* title=e.raw==91?"HE":e.raw==93?"FLASH":e.raw==92?"SMOKE":"FIRE";
            timer_badge(d,{at.x,at.y+44.f*scale},e.raw,e.clock,false,title,-1.f,true,calculate_distance(e.position,local));
        }
        else if(cfg::world_esp::grenade_effects)timer_badge(d,at,e.raw,{},false,nullptr,-1.f,false,calculate_distance(e.position,local));
    }
}
}
