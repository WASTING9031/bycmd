#pragma once
// ESP drawing, shared by the overlay and the menu preview (the preview calls the very same player()).
#include "imgui.h"
#include "../ui/cfg.hpp"
#include "../ui/theme/theme.hpp"
#include "../ui/esp_font.hpp"
#include "weapon_icons.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace esp_style {
inline ImU32 color(ImVec4 c, float alpha=1.f) { c.w *= std::clamp(alpha,0.f,1.f); return ImGui::ColorConvertFloat4ToU32(c); }
inline ImFont* font(ImFont* f) { return f ? f : ImGui::GetFont(); }
inline ImVec2 measure(ImFont* f,float size,const char* text) { return font(f)->CalcTextSizeA(size,FLT_MAX,0.f,text); }
// pixel font is crisp only at multiples of its design size (10 px)
inline float pixel_size() { const float s=cfg::esp::font_size; return s<15.f?10.f:s<25.f?20.f:30.f; }
inline float text_size(int font_index) { if(font_index==esp_fonts_ui::f_pixel) return pixel_size(); return std::round(std::clamp(cfg::esp::font_size,10.f,26.f)*esp_fonts_ui::scale(font_index)); }

inline void text(ImDrawList* dl, ImFont* f,float size,ImVec2 p,ImU32 col,const char* s) {
    if (!s || !*s) return;
    f=font(f); p.x=std::round(p.x); p.y=std::round(p.y);
    if (cfg::esp::outline) {
        const ImU32 black=IM_COL32(0,0,0,(col>>IM_COL32_A_SHIFT)&0xFF);
        dl->AddText(f,size,ImVec2(p.x-1,p.y),black,s); dl->AddText(f,size,ImVec2(p.x+1,p.y),black,s);
        dl->AddText(f,size,ImVec2(p.x,p.y-1),black,s); dl->AddText(f,size,ImVec2(p.x,p.y+1),black,s);
    }
    dl->AddText(f,size,p,col,s);
}
inline void label(ImDrawList* dl,ImFont* f,float size,ImVec2 p,ImVec4 c,float a,const char* s) {
    const ImVec2 z=measure(f,size,s);
    if(cfg::esp::label_background) dl->AddRectFilled(ImVec2(p.x-3,p.y-1),ImVec2(p.x+z.x+3,p.y+z.y+1),color(ImVec4(0,0,0,.55f),a),2.f);
    text(dl,f,size,p,color(c,a),s);
}
inline std::string shorten(ImFont* f,float size,const char* s,float max_width) {
    std::string out=s?s:"";
    if(measure(f,size,out.c_str()).x<=max_width) return out;
    while(!out.empty()) {
        size_t n=out.size()-1;
        while(n>0 && (static_cast<unsigned char>(out[n])&0xc0)==0x80) --n;
        out.resize(n);
        if(measure(f,size,(out+"..").c_str()).x<=max_width) break;
    }
    return out+"..";
}
inline void box(ImDrawList* dl,ImVec2 t,ImVec2 b,float a) {
    if(b.x<=t.x || b.y<=t.y) return;
    const float thick=std::clamp(cfg::esp::box_thickness,1.f,3.f);
    if(cfg::esp::box_fill) { ImVec4 c=cfg::esp::box_col; c.w*=std::clamp(cfg::esp::fill_opacity,0.f,.35f); dl->AddRectFilled(t,b,color(c,a)); }
    const ImU32 c=color(cfg::esp::box_col,a), dark=color(ImVec4(0,0,0,.9f),a);
    if(cfg::esp::box_type==0) {
        const float r=std::clamp(cfg::esp::box_rounding,0.f,8.f);
        if(cfg::esp::outline) {
            dl->AddRect(ImVec2(t.x-thick,t.y-thick),ImVec2(b.x+thick,b.y+thick),dark,r,0,1.f);
            dl->AddRect(ImVec2(t.x+thick,t.y+thick),ImVec2(b.x-thick,b.y-thick),dark,r,0,1.f);
        }
        dl->AddRect(t,b,c,r,0,thick);
    } else {
        const float k=std::clamp(std::min(b.x-t.x,b.y-t.y)*cfg::esp::corner_length,3.f,40.f);
        const ImVec2 pts[4]={t,ImVec2(b.x,t.y),b,ImVec2(t.x,b.y)};
        const float dx[4]={1,-1,-1,1},dy[4]={1,1,-1,-1};
        for(int pass=cfg::esp::outline?0:1;pass<2;++pass) for(int i=0;i<4;++i) {
            const ImVec2 p=pts[i]; const float w=pass?thick:thick+2.f, o=pass?0.f:1.f;
            dl->AddLine(ImVec2(p.x-dx[i]*o,p.y),ImVec2(p.x+dx[i]*(k+o),p.y),pass?c:dark,w);
            dl->AddLine(ImVec2(p.x,p.y-dy[i]*o),ImVec2(p.x,p.y+dy[i]*(k+o)),pass?c:dark,w);
        }
    }
}
inline void bomb_carrier(ImDrawList* dl,ImVec2 top,float width=56.f,float alpha=1.f) {
    if (!dl || !std::isfinite(width) || width<=0.f) return;
    weapon_icons::draw(dl,weapon_catalog::lookup(100),top,width,color(cfg::esp::bomb_col,alpha));
}
// HP / armor bars like the reference screenshot: solid bar with a dark outline, sized for the screen
// (at 1080p: 5 px bar + 2 px outline; HP bar 7 px left of the box, armor bar 7 px under the box).
inline float bar_scale() { const float h=g_sh>0.f?g_sh:ImGui::GetIO().DisplaySize.y; return std::isfinite(h)&&h>0?std::clamp(h/1080.f,.5f,2.5f):1.f; }
inline float bar_core() { const float w=std::isfinite(cfg::esp::bar_width)?std::clamp(cfg::esp::bar_width,1.f,10.f):5.f; return std::max(2.f,std::round(w*bar_scale())); }
inline float bar_outline() { return std::max(1.f,std::round(bar_core()*.4f)); }
inline void health(ImDrawList* dl,int hp,int max_hp,ImVec2 t,ImVec2 b,float a) {
    if(max_hp<=0) max_hp=100;
    if(b.y<=t.y) return;
    const float ratio=std::clamp(hp/float(max_hp),0.f,1.f), w=bar_core(), o=bar_outline(), gap=std::round(7.f*bar_scale());
    const float x1=std::round(t.x)-gap, x0=x1-w, y0=std::round(t.y), y1=std::round(b.y), y=std::round(y1-(y1-y0)*ratio);
    dl->AddRectFilled(ImVec2(x0-o,y0-o),ImVec2(x1+o,y1+o),color(ImVec4(0,0,0,.75f),a));
    ImVec4 c=cfg::esp::health_col;
    if(cfg::esp::health_gradient) c=ImVec4(1.f+(c.x-1.f)*ratio,.2f+(c.y-.2f)*ratio,.15f+(c.z-.15f)*ratio,c.w);
    if(ratio>0) dl->AddRectFilled(ImVec2(x0,y),ImVec2(x1,y1),color(c,a));
    if(cfg::esp::health_numbers && hp<max_hp) {   // number left of the bar, at the fill edge
        char s[12]; std::snprintf(s,sizeof s,"%d",hp);
        ImFont* f=esp_fonts_ui::pixel(); const float ps=pixel_size(); const ImVec2 z=measure(f,ps,s);
        text(dl,f,ps,ImVec2(std::round(x0-o-2.f-z.x),std::round(std::clamp(y-z.y*.5f,y0-z.y*.5f,y1-z.y))),color(ImVec4(1,1,1,1),a),s);
    }
}
// Armor bar under the box (same thickness / outline as the HP bar). Returns the y below it.
inline float armor(ImDrawList* dl,int value,ImVec2 t,ImVec2 b,float a) {
    const float w=bar_core(), o=bar_outline(), ratio=std::clamp(value/100.f,0.f,1.f);
    const float x0=std::round(t.x), x1=std::round(b.x), y0=std::round(b.y+7.f*bar_scale()), y1=y0+w;
    dl->AddRectFilled(ImVec2(x0-o,y0-o),ImVec2(x1+o,y1+o),color(ImVec4(0,0,0,.75f),a));
    if(ratio>0) dl->AddRectFilled(ImVec2(x0,y0),ImVec2(std::round(x0+(x1-x0)*ratio),y1),color(cfg::esp::armor_col,a));
    return y1+o;
}
inline void name(ImDrawList* dl,const char* s,ImVec2 t,float cx,float a) {
    ImFont* f=esp_fonts_ui::name_font(); const float size=text_size(cfg::esp::name_font);
    const std::string nm=shorten(f,size,s,240.f); const ImVec2 z=measure(f,size,nm.c_str());
    label(dl,f,size,ImVec2(std::round(cx-z.x*.5f),std::round(t.y-z.y-4.f)),cfg::esp::name_col,a,nm.c_str());
}
inline float icon_size_setting() {
    return std::isfinite(cfg::esp::weapon_icon_size)?std::clamp(cfg::esp::weapon_icon_size,16.f,72.f):34.f;
}
inline float icon_height() { return std::round(std::clamp(icon_size_setting()*.40f,7.f,30.f)); }
inline float loadout_height() { return std::round(std::max(7.f,icon_height()*.78f)); }
// Main weapon icon at a fixed INK HEIGHT, width from the glyph's own aspect ratio.
// (The old width fit squeezed every long gun into a 34 px wide, 6-7 px tall line, so
// AWM / M4 / AKR all looked like the same strip.) Very long guns are capped in width.
inline float weapon_icon(ImDrawList* dl,const weapon_catalog::definition* w,float cx,float y,float a) {
    if(!w||w->kind==weapon_catalog::icon_kind::none) return y;
    float h=icon_height(); const float max_w=icon_size_setting()*2.4f;
    float wd=weapon_icons::width_at_height(w,h);
    if(wd>max_w){h=std::max(6.f,std::floor(h*max_w/wd));wd=weapon_icons::width_at_height(w,h);}
    weapon_icons::draw_at_height(dl,w,ImVec2(std::round(cx-wd*.5f),std::round(y)),h,color(cfg::esp::weapon_icon_col,a));
    return std::round(y)+h+3.f;
}
inline float weapon(ImDrawList* dl,uint8_t raw,float cx,float y,float a) {
    const auto* w=weapon_catalog::lookup(raw);
    if(w && w->kind==weapon_catalog::icon_kind::none) return y;
    if(cfg::esp::weapon_text) {
        char up[48]; const char* nm=w?w->name:"?"; size_t i=0;
        char unknown[8]; if(!w){std::snprintf(unknown,sizeof unknown,"#%u",unsigned(raw));nm=unknown;} // unknown id: show the number
        for(;nm[i]&&i<47;++i) up[i]=(nm[i]>='a'&&nm[i]<='z')?char(nm[i]-32):nm[i]; up[i]=0;
        ImFont* f=esp_fonts_ui::weapon_font(); const float size=cfg::esp::weapon_font==esp_fonts_ui::f_pixel?pixel_size():text_size(cfg::esp::weapon_font)-2.f; const ImVec2 z=measure(f,size,up);
        label(dl,f,size,ImVec2(cx-z.x*.5f,y),cfg::esp::weapon_text_col,a,up); y+=z.y+2.f;
    }
    if(cfg::esp::weapon_icons && w) y=weapon_icon(dl,w,cx,y,a);
    return y;
}
// Loadout row: every OTHER item this player owns (primary, pistol, grenades, bomb, kit),
// each with its own icon, all at the same ink height. Knives, hands and vests are skipped
// (everyone has a knife; armour already has the HK/K flag). Order: primary, pistol,
// grenades by id, bomb, kit. `active` is drawn above, so one copy of it is skipped.
inline int loadout_rank(const weapon_catalog::definition* d) {
    using K=weapon_catalog::icon_kind;
    if(!d||d->raw_id==110||d->raw_id==111) return -1;
    switch(d->kind){
    case K::rifle: case K::sniper: case K::smg: case K::shotgun: case K::machinegun: return 0;
    case K::pistol: return 1; case K::grenade: return 2; case K::bomb: return 3; case K::gear: return 4;
    default: return -1;
    }
}
inline float loadout_row(ImDrawList* dl,const uint8_t* raw,int n,int active,float cx,float y,float h,float a) {
    constexpr int cap=12;
    const weapon_catalog::definition* items[cap]; int m=0; bool active_skipped=false;
    for(int i=0;i<n&&m<cap;++i) {
        const auto* d=weapon_catalog::lookup(raw[i]);
        if(loadout_rank(d)<0) continue;
        if(!active_skipped&&int(raw[i])==active){active_skipped=true;continue;}
        items[m++]=d;
    }
    if(!m||!std::isfinite(h)||h<=0.f) return y;
    std::stable_sort(items,items+m,[](const weapon_catalog::definition* l,const weapon_catalog::definition* r){
        const int x=loadout_rank(l),z=loadout_rank(r); return x!=z?x<z:l->raw_id<r->raw_id;});
    const float gap=std::max(3.f,std::round(h*.35f));
    float widths[cap]; float total=gap*float(m-1);
    for(int i=0;i<m;++i){widths[i]=std::round(weapon_icons::width_at_height(items[i],h));total+=widths[i];}
    float x=std::round(cx-total*.5f); const float top=std::round(y);
    for(int i=0;i<m;++i) {
        const ImU32 c=color(cfg::esp::loadout_col,a*.9f);
        weapon_icons::draw_at_height(dl,items[i],ImVec2(x,top),h,c);
        x+=widths[i]+gap;
    }
    return top+h+2.f;
}
inline void tracer(ImDrawList* dl,ImVec2 origin,ImVec2 foot,float a) {
    const float width=std::clamp(cfg::esp::tracer_thickness,.5f,4.f);
    if(cfg::esp::outline) dl->AddLine(origin,foot,color(ImVec4(0,0,0,.5f),a),width+2.f);
    dl->AddLine(origin,foot,color(cfg::esp::tracer_col,a),width);
}
inline void arrow(ImDrawList* dl,ImFont*,ImVec2 dir,float meters,ImVec2 origin,ImVec2 extent,float a) {
    const float norm=std::sqrt(dir.x*dir.x+dir.y*dir.y);
    if(!std::isfinite(norm)||norm<.001f) return;
    dir.x/=norm; dir.y/=norm;
    const float inset=std::clamp(cfg::esp::offscreen_radius,24.f,std::max(24.f,std::min(extent.x,extent.y)*.45f));
    const float rx=std::max(20.f,extent.x*.5f-inset),ry=std::max(20.f,extent.y*.5f-inset);
    const float r=1.f/std::sqrt(dir.x*dir.x/(rx*rx)+dir.y*dir.y/(ry*ry));
    const float size=std::clamp(cfg::esp::offscreen_size,8.f,48.f);
    const ImVec2 tip(origin.x+dir.x*r,origin.y+dir.y*r);
    const ImVec2 base(tip.x-dir.x*size*1.4f,tip.y-dir.y*size*1.4f);
    const ImVec2 l(base.x-dir.y*size*.6f,base.y+dir.x*size*.6f),rr(base.x+dir.y*size*.6f,base.y-dir.x*size*.6f);
    const ImVec2 notch(tip.x-dir.x*size*1.05f,tip.y-dir.y*size*1.05f);
    dl->AddTriangleFilled(tip,l,notch,color(cfg::esp::offscreen_col,a));
    dl->AddTriangleFilled(tip,notch,rr,color(cfg::esp::offscreen_col,a));
    if(cfg::esp::outline){dl->PathLineTo(tip);dl->PathLineTo(l);dl->PathLineTo(notch);dl->PathLineTo(rr);dl->PathStroke(color(ImVec4(0,0,0,.85f),a),ImDrawFlags_Closed,1.f);}
    const float badge=cfg::esp::offscreen_silhouette?std::clamp(size*.65f,12.f,23.f):0.f;
    const ImVec2 center(base.x-dir.x*(badge+9),base.y-dir.y*(badge+9));
    if(badge>0) {
        dl->AddCircleFilled(center,badge,color(ImVec4(.035f,.035f,.045f,.94f),a),32);
        dl->AddCircle(center,badge,color(cfg::esp::offscreen_col,a*.65f),32,1.2f);
        const ImU32 ink=color(cfg::esp::offscreen_col,a);const float u=badge/14.f;
        dl->AddCircleFilled({center.x,center.y-7*u},3.0f*u,ink,16);
        dl->AddRectFilled({center.x-3.2f*u,center.y-2.5f*u},{center.x+3.2f*u,center.y+4*u},ink,2*u);
        for(float side:{-1.f,1.f}) {
            dl->AddLine({center.x+side*3*u,center.y-1.5f*u},{center.x+side*6*u,center.y+4*u},ink,2.6f*u);
            dl->AddLine({center.x+side*1.6f*u,center.y+3*u},{center.x+side*3.5f*u,center.y+10*u},ink,2.8f*u);
        }
    }
    if(cfg::esp::offscreen_dist) {
        char s[20]; std::snprintf(s,sizeof s,"%.0fM",meters);
        ImFont* f=esp_fonts_ui::pixel(); const float fs=pixel_size(); const ImVec2 z=measure(f,fs,s);
        text(dl,f,fs,ImVec2(center.x-dir.x*(badge+12.f+z.y*.5f)-z.x*.5f,center.y-dir.y*(badge+12.f+z.y*.5f)-z.y*.5f),color(ImVec4(1,1,1,1),a),s);
    }
}

// Everything known about one player for one frame.
struct player_info {
    ImVec2 min{}, max{};
    const char* name = "";
    int hp = 100, max_hp = 100;
    int armor = -1; bool helmet = false;
    bool has_money = false; unsigned money = 0;
    bool has_ping = false; unsigned ping = 0;
    bool has_weapon = false; uint8_t weapon = 0;
    bool has_ammo = false; unsigned ammo = 0;
    uint8_t loadout[12]{}; int loadout_count = 0;   // every item this player owns (own uyg part)
    float distance = -1.f;
    bool bomb = false;
};
inline void player(ImDrawList* dl,const player_info& p,float a) {
    const ImVec2 t=p.min,b=p.max; const float cx=std::round((t.x+b.x)*.5f);
    if(cfg::esp::box) box(dl,t,b,a);
    if(cfg::esp::health) health(dl,p.hp,p.max_hp,t,b,a);
    if(cfg::esp::name) name(dl,p.name&&*p.name?p.name:"Enemy",t,cx,a);
    float y=b.y+3.f;
    if(cfg::esp::ammo && p.has_ammo) {
        const float scale=bar_scale(), o=bar_outline(), h=bar_core();
        const float x=std::round(t.x), y0=std::round(b.y+7.f*scale), len=std::max(12.f,std::round((b.x-t.x)*.55f));
        dl->AddRectFilled({x-o,y0-o},{x+len+o,y0+h+o},color(ImVec4(0,0,0,.85f),a));
        dl->AddRectFilled({x,y0},{x+len,y0+h},color(cfg::esp::armor_col,a));
        char number[12]; std::snprintf(number,sizeof number,"%u",p.ammo);
        ImFont* f=esp_fonts_ui::pixel(); const float fs=pixel_size(); const ImVec2 z=measure(f,fs,number);
        text(dl,f,fs,{std::round(x+len+3.f),std::round(y0+(h-z.y)*.5f)},color(ImVec4(1,1,1,1),a),number);
        y=std::max(y0+h+o,y0+(h+z.y)*.5f)+3.f;
    }
    // flags right of the box: always pixel font
    ImFont* pf=esp_fonts_ui::pixel(); const float ps=pixel_size(); float fy=t.y-1.f; const float fx=b.x+4.f;
    char s[32];
    auto flag=[&](const char* str,ImVec4 c){text(dl,pf,ps,ImVec2(fx,fy),color(c,a),str);fy+=measure(pf,ps,str).y;};
    if(cfg::esp::money&&p.has_money){std::snprintf(s,sizeof s,"$%u",p.money);flag(s,cfg::esp::money_col);}
    if(cfg::esp::armor&&p.armor>0)flag(p.helmet?"HK":"K",ImVec4(1,1,1,1));
    if(cfg::esp::ping&&p.has_ping){std::snprintf(s,sizeof s,"%uMS",p.ping);flag(s,cfg::esp::ping_col);}
    if(cfg::esp::distance&&p.distance>=0){std::snprintf(s,sizeof s,"%.0fM",p.distance);flag(s,cfg::esp::distance_col);}
    if(cfg::esp::bomb&&p.bomb)flag("C4",cfg::esp::bomb_col);
    if((cfg::esp::weapon_text||cfg::esp::weapon_icons)&&p.has_weapon) y=weapon(dl,p.weapon,cx,y,a);
    if(cfg::esp::loadout&&p.loadout_count>0) loadout_row(dl,p.loadout,p.loadout_count,p.has_weapon?int(p.weapon):-1,cx,y,loadout_height(),a);
}
}
