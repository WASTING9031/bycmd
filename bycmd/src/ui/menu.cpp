#include <cstring>
#include "menu.hpp"
#include "bar.hpp"
#include "cfg.hpp"
#include "theme/theme.hpp"
#include "../func/esp_style.hpp"
#include "../func/mesh_chams.hpp"
#include "../func/fog.hpp"
#include "../func/sky.hpp"
#include "../func/viewmodel.hpp"
#include "../func/camera_overrides.hpp"
#include "../func/scope.hpp"
#include "../func/particles.hpp"
#include "../func/world_mode.hpp"
#include "../func/target_hud.hpp"
#include "../func/world_visuals.hpp"
#include "../func/touch_aim.hpp"
#include "../func/silent_aim.hpp"
#include "../func/triggerbot.hpp"
#include "../func/anti_effects.hpp"
#include "../func/norecoil.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "preview_assets.hpp"
#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <initializer_list>

extern ImFont *fontBold,*fontMedium,*fontDesc,*espFont;
namespace ui::menu {
namespace {
const ImVec4 pink(197.f/255.f,143.f/255.f,200.f/255.f,1),white(.94f,.94f,.95f,1),dim(.64f,.64f,.67f,1);
constexpr float row_h=44.f;
int rows_left=0;
int page=0;
int sections[6]={0,0,0,0,0,0};
const char* preset_name="Default";
ImU32 color(ImVec4 c) {return ImGui::GetColorU32(c);}
ImDrawList* dl(){return ImGui::GetWindowDrawList();}
float animate(const char* name,float target,float speed=16.f) {
    auto* storage=ImGui::GetStateStorage();const ImGuiID id=ImGui::GetID(name);
    float value=storage->GetFloat(id,target);
    const float dt=std::clamp(ImGui::GetIO().DeltaTime,0.f,.1f);
    value+=(target-value)*(1.f-std::exp(-speed*dt));
    if(std::fabs(target-value)<.0001f)value=target;
    storage->SetFloat(id,value);return value;
}
ImVec4 blend(ImVec4 a,ImVec4 b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};}

void label(ImVec2 p,const char* s,ImVec4 c=white,float size=16.f){dl()->AddText(espFont?espFont:ImGui::GetFont(),size,p,color(c),s);}
void glass_decor(ImDrawList* d,ImVec2 p,ImVec2 sz,float r,float alpha);
void card(const char* title,int rows) {
    ImGui::PushStyleColor(ImGuiCol_Text,dim);
    if(fontDesc) ImGui::PushFont(fontDesc);
    ImGui::TextUnformatted(title);
    if(fontDesc) ImGui::PopFont(); ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0,8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16,5));
    ImGui::BeginChild(title,ImVec2(0,rows*row_h+12),true,ImGuiWindowFlags_AlwaysUseWindowPadding|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    rows_left=rows;
    glass_decor(dl(),ImGui::GetWindowPos(),ImGui::GetWindowSize(),ImGui::GetStyle().ChildRounding,ImGui::GetStyle().Alpha);
}
void glass_decor(ImDrawList* d,ImVec2 p,ImVec2 sz,float r,float alpha) {
    const ImVec2 q(p.x+sz.x,p.y+sz.y);
    auto tint=[&](float red,float green,float blue,float a){return IM_COL32(int(red*255),int(green*255),int(blue*255),int(std::clamp(a*alpha,0.f,1.f)*255));};
    d->AddRectFilled(p,q,tint(.041f,.040f,.047f,.84f),r);
    const int strips=48;
    for(int i=0;i<strips;++i){
        float y0=sz.y*i/strips,y1=sz.y*(i+1)/strips,ym=(y0+y1)*.5f;
        float edge=ym<r?r-ym:(ym>sz.y-r?ym-(sz.y-r):0.f);
        float inset=edge>0?r-std::sqrt(std::max(0.f,r*r-edge*edge)):0.f;
        float a=.045f*std::exp(-ym/std::max(24.f,sz.y*.38f))+.008f;
        d->AddRectFilled({p.x+inset+1,p.y+y0+1},{q.x-inset-1,std::min(q.y-1,p.y+y1+1)},tint(.92f,.91f,1.f,a));
    }
    const ImVec2 glow(p.x+sz.x*.64f,p.y+sz.y*.57f);
    const float rx=std::min(76.f,sz.x*.23f),ry=std::min(88.f,sz.y*.43f);
    for(int i=18;i>=1;--i){float t=i/18.f;
        d->AddEllipseFilled(glow,rx*t,ry*t,tint(.64f,.38f,.69f,.020f*(1.f-t)+.006f),0,48);
    }
    d->AddRect({p.x+.5f,p.y+.5f},{q.x-.5f,q.y-.5f},tint(.70f,.65f,.77f,.085f),r,0,1.f);
    d->AddRect({p.x+3,p.y+3},{q.x-3,q.y-3},tint(0,0,0,.32f),std::max(1.f,r-2),0,1.f);
    d->AddLine({p.x+r,p.y+1.5f},{q.x-r,p.y+1.5f},tint(.88f,.86f,.95f,.095f),1.f);
    d->AddLine({p.x+r,q.y-1.5f},{q.x-r,q.y-1.5f},tint(0,0,0,.5f),1.f);
}

void end_card(){ImGui::EndChild();ImGui::PopStyleVar();ImGui::Dummy(ImVec2(0,14));}
void divider(ImVec2 p,float w) {
    if(--rows_left>0) dl()->AddLine(ImVec2(p.x,p.y+row_h),ImVec2(p.x+w,p.y+row_h),color(ImVec4(1,1,1,.065f)));
}
void focus(ImVec2 p,float w) {
    if(ImGui::IsItemFocused()) dl()->AddRect(ImVec2(p.x,p.y+2),ImVec2(p.x+w,p.y+row_h-2),color(pink),5);
}
void row_label(ImVec2 p,const char* s,float maxx) {
    dl()->PushClipRect(p,ImVec2(maxx,p.y+row_h),true);
    label(ImVec2(p.x,p.y+14),s);dl()->PopClipRect();
}
void picker(const char* s,ImVec4* c) {
    if(ImGui::IsPopupOpen("##picker")) {
        const auto size=ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize({std::min(340.f,size.x-32),0});
        if(size.x<720.f||size.y<520.f)ImGui::SetNextWindowPos({size.x*.5f,size.y*.5f},ImGuiCond_Always,{.5f,.5f});
    }
    if(ImGui::BeginPopup("##picker")) {
        ImGui::TextUnformatted(s);ImGui::Dummy({0,8});ImGui::SetNextItemWidth(-1);
        ImGui::ColorPicker4("##value",&c->x,ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoSidePreview|ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_PickerHueBar);
        ImGui::Dummy({0,8});ImGui::SetNextItemWidth(-1);
        ImGui::ColorEdit4("##hex",&c->x,ImGuiColorEditFlags_NoPicker|ImGuiColorEditFlags_NoSmallPreview|ImGuiColorEditFlags_DisplayHex|ImGuiColorEditFlags_NoOptions);
        ImGui::Dummy({0,8});if(ImGui::Button("Done",{-1,44}))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
}
void swatch(const char* s,ImVec4* c,ImVec2 p) {
    ImGui::SetCursorScreenPos(p);
    if(ImGui::InvisibleButton("##swatch",{44,44}))ImGui::OpenPopup("##picker");
    focus(p,44);
    const ImVec2 a(p.x+9,p.y+9),b(p.x+35,p.y+35);
    dl()->AddRectFilled(a,b,color({.32f,.32f,.34f,1}),6);
    dl()->AddRectFilled(a,b,color(*c),6);
    dl()->AddRect(a,b,color({1,1,1,.22f}),6);
    picker(s,c);
}
void toggle(const char* s,bool* v,ImVec4* tint=nullptr) {
    ImGui::PushID(s);const ImVec2 p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x;
    const float switch_x=p.x+w-54.f,label_w=w-(tint?102.f:58.f);
    if(ImGui::InvisibleButton("##label",{label_w,row_h}))*v=!*v;
    bool hovered=ImGui::IsItemHovered();focus(p,label_w);
    row_label(p,s,p.x+label_w-6.f);
    if(tint)swatch(s,tint,{switch_x-48.f,p.y});
    ImGui::SetCursorScreenPos({switch_x,p.y});
    if(ImGui::InvisibleButton("##switch",{54,row_h}))*v=!*v;
    hovered|=ImGui::IsItemHovered();focus({switch_x,p.y},54);
    const float t=animate("##anim",*v?1.f:0.f),h=animate("##hover",hovered?1.f:0.f,20.f);
    const ImVec2 a(switch_x+4.f,p.y+9.f),b(a.x+46.f,a.y+26.f);
    const ImVec4 track=blend({.18f,.18f,.20f,1},pink,t);
    dl()->AddRectFilled(a,b,color(track),13);
    dl()->AddRect({a.x+.5f,a.y+.5f},{b.x-.5f,b.y-.5f},color({1,1,1,.13f+.08f*h}),13);
    dl()->AddCircleFilled({a.x+13.f+20.f*t,a.y+13.f},10.f,color({1,1,1,1}),32);
    ImGui::SetCursorScreenPos({p.x,p.y+row_h});divider(p,w);ImGui::PopID();
}
bool slider(const char* s,float* v,float lo,float hi,const char* fmt="%.1f") {
    ImGui::PushID(s);ImVec2 p=ImGui::GetCursorScreenPos();float w=ImGui::GetContentRegionAvail().x;
    float value_x=p.x+w*.43f,track_x=value_x+72,track_w=p.x+w-track_x;
    row_label(p,s,value_x-6);
    dl()->AddRectFilled(ImVec2(value_x,p.y+8),ImVec2(value_x+64,p.y+36),color(ImVec4(.10f,.10f,.105f,1)),7);
    dl()->AddRect(ImVec2(value_x,p.y+8),ImVec2(value_x+64,p.y+36),color(ImVec4(1,1,1,.06f)),7);
    char text[32];std::snprintf(text,sizeof(text),fmt,*v);
    ImFont* f=fontDesc?fontDesc:ImGui::GetFont();ImVec2 z=f->CalcTextSizeA(16,FLT_MAX,0,text);
    dl()->AddText(f,16,ImVec2(value_x+32-z.x*.5f,p.y+14),color(white),text);
    ImGui::SetCursorScreenPos(ImVec2(track_x,p.y));ImGui::SetNextItemWidth(track_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0,0,0,0));ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(0,0,0,0));ImGui::PushStyleColor(ImGuiCol_FrameBgActive,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(0,0,0,0));ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    float prev=*v;
    ImGui::SliderFloat("##slider",v,lo,hi,"",ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput);
    bool changed=(*v!=prev);
    ImGui::PopStyleVar();ImGui::PopStyleColor(5);
    float a=track_x+8,b=track_x+track_w-8,x=a+(b-a)*animate("##slider_motion",std::clamp((*v-lo)/(hi-lo),0.f,1.f),24.f);
    dl()->AddLine(ImVec2(a,p.y+22),ImVec2(b,p.y+22),color(ImVec4(.13f,.13f,.14f,1)),5);
    dl()->AddLine(ImVec2(a,p.y+22),ImVec2(x,p.y+22),color(pink),5);
    dl()->AddCircleFilled(ImVec2(x,p.y+22),7,color(white),24);
    focus(ImVec2(track_x,p.y),track_w);
    ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+row_h));divider(p,w);ImGui::PopID();
    return changed;
}
bool slider(const char* s,int* v,int lo,int hi,const char* fmt="%d") {
    float fv=float(*v);
    slider(s,&fv,float(lo),float(hi),fmt);
    int prev=*v; *v=int(fv);
    return *v!=prev;
}
void combo(const char* s,int* v,const char* const* values,int n) {
    ImGui::PushID(s);ImVec2 p=ImGui::GetCursorScreenPos();float w=ImGui::GetContentRegionAvail().x;
    float split=w*.44f;row_label(p,s,p.x+split-8);
    ImGui::SetCursorScreenPos(ImVec2(p.x+split,p.y));ImGui::SetNextItemWidth(w-split);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(10,13));
    if(ImGui::BeginCombo("##select",values[std::clamp(*v,0,n-1)],ImGuiComboFlags_HeightLarge)) {
        for(int i=0;i<n;++i) { bool selected=*v==i; if(ImGui::Selectable(values[i],selected,0,ImVec2(0,44))) *v=i; if(selected)ImGui::SetItemDefaultFocus(); }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+row_h));divider(p,w);ImGui::PopID();
}
void preset(int p) {
    preset_name=p==0?"Clean":p==1?"Contrast":"Minimal";
    cfg::esp::box=true;cfg::esp::box_type=p==1?1:0;cfg::esp::box_thickness=1.f;
    cfg::esp::box_rounding=0;cfg::esp::corner_length=.25f;cfg::esp::box_fill=false;cfg::esp::fill_opacity=.08f;
    cfg::esp::outline=true;cfg::esp::label_background=false;cfg::esp::font_size=p==1?22.f:16.f;
    cfg::esp::health=true;cfg::esp::health_gradient=p==1;cfg::esp::health_numbers=p!=2;cfg::esp::armor=p!=2;
    cfg::esp::name=true;cfg::esp::distance=p==1;cfg::esp::weapon_text=true;cfg::esp::weapon_icons=p!=2;cfg::esp::loadout=p!=2;
    cfg::esp::weapon_icon_size=34;cfg::esp::tracer=false;cfg::esp::offscreen=p==1;
}
ImU32 rgba(float r,float g,float b,float al){return ImGui::GetColorU32(ImVec4(r,g,b,al));}
ImU32 mul(ImVec4 c,float k,float al){return ImGui::GetColorU32(ImVec4(c.x*k,c.y*k,c.z*k,c.w*al));}
void ptext(ImDrawList* d,ImFont* f,float sz,ImVec2 p,ImU32 c,const char* s,bool shadow=true){
    p=ImVec2(std::round(p.x),std::round(p.y));
    if(shadow){ImU32 sh=IM_COL32(0,0,0,int(((c>>IM_COL32_A_SHIFT)&0xFF)*.75f));d->AddText(f,sz,ImVec2(p.x+1,p.y+1),sh,s);}
    d->AddText(f,sz,p,c,s);
}
ImVec2 tsize(ImFont* f,float sz,const char* s){return f->CalcTextSizeA(sz,FLT_MAX,0,s);}
void gbar(ImDrawList* d,ImVec2 a,ImVec2 b,float frac,bool vertical,ImVec4 hi,ImVec4 lo,float al){
    d->AddRectFilled(ImVec2(a.x-1,a.y-1),ImVec2(b.x+1,b.y+1),rgba(0,0,0,.55f*al));
    ImVec2 fa=a,fb=b; if(vertical)fa.y=b.y-(b.y-a.y)*frac; else fb.x=a.x+(b.x-a.x)*frac;
    const ImU32 ch=mul(hi,1,al),cl=mul(lo,1,al);
    if(vertical)d->AddRectFilledMultiColor(fa,fb,ch,ch,cl,cl);else d->AddRectFilledMultiColor(fa,fb,cl,ch,ch,cl);
}
void sample(int raw,float H=420.f) {
    ImVec2 p=ImGui::GetCursorScreenPos();float w=ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##sample",ImVec2(w,H));
    ImDrawList* d=dl();const float a=ImGui::GetStyle().Alpha;
    const ImVec2 q(p.x+w,p.y+H);
    ImTextureID tex=ImGui::GetIO().Fonts->TexID;
    d->PushClipRect(ImVec2(p.x+1,p.y+1),ImVec2(q.x-1,q.y-1),true);
    const float cx=std::round(p.x+w*.46f);
    const float bh=std::min(300.f,H-150.f),bw=std::round(bh*float(preview_art::girl_w)/float(preview_art::girl_h))+14.f;
    const ImVec2 tl(std::round(cx-bw*.5f),std::round(p.y+56.f)),br(std::round(cx+bw*.5f),std::round(p.y+56.f+bh));
    ImVec2 uv0,uv1;
    // art
    if(preview_assets::uv(preview_assets::girl_rect,uv0,uv1)){
        const float ih=bh-10.f,iw=ih*float(preview_art::girl_w)/float(preview_art::girl_h);
        const ImVec2 i0(cx-iw*.5f,tl.y+6),i1(cx+iw*.5f,tl.y+6+ih);
        d->AddImage(tex,i0,i1,uv0,uv1,rgba(1,1,1,a));
    }
    esp_style::player_info info{};
    info.min=tl; info.max=br; info.name="Alice"; info.hp=100; info.armor=64; info.helmet=true;
    info.has_money=true; info.money=0; info.has_ping=true; info.ping=48;
    info.has_weapon=true; info.weapon=uint8_t(raw); info.has_ammo=true; info.ammo=26;
    info.loadout[0]=91; info.loadout[1]=92; info.loadout_count=2; info.distance=24;
    esp_style::player(d,info,a);
    if(cfg::esp::tracer) esp_style::tracer(d,ImVec2(cx,q.y-2),ImVec2(cx,br.y),a);
    if(cfg::esp::offscreen) esp_style::arrow(d,nullptr,ImVec2(1.f,-.35f),41,ImVec2(cx,p.y+H*.5f),ImVec2(w,H),a);
    d->PopClipRect();
}
int preview_weapon=[]{for(size_t i=0;i<weapon_catalog::definitions.size();++i)if(weapon_catalog::definitions[i].raw_id==44)return int(i);return 16;}();
// Target HUD sample: the same panel() the overlay draws, with sample values.
void target_sample(){
    const ImVec2 p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x;
    const float u=std::clamp((w-4.f)/280.f,.6f,1.f);const ImVec2 z=target_hud::panel_size(u);
    ImGui::Dummy({w,z.y+16});
    target_hud::info t;t.name="Alice";t.hp=72;t.armor=64;t.helmet=true;t.has_weapon=true;
    t.weapon=weapon_catalog::definitions[size_t(preview_weapon)].raw_id;t.has_money=true;t.money=16000;t.distance=24;
    target_hud::panel(dl(),{std::round(p.x+2),std::round(p.y+2)},u,t,72,86,ImGui::GetStyle().Alpha);
}
// Custom scope sample: the same reticle() the overlay draws, over a light test backdrop.
void scope_sample(){
    const ImVec2 p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x,h=124.f;
    ImGui::Dummy({w,h+14});const float al=ImGui::GetStyle().Alpha;
    const ImVec2 a(p.x,p.y),b(p.x+w,p.y+h);
    dl()->AddRectFilledMultiColor(a,b,rgba(.80f,.84f,.90f,al),rgba(.80f,.84f,.90f,al),rgba(.55f,.56f,.58f,al),rgba(.55f,.56f,.58f,al));
    dl()->AddRect(a,b,rgba(1,1,1,.10f*al),0,0,1.f);
    dl()->PushClipRect(a,b,true);scope::reticle(dl(),{(a.x+b.x)*.5f,(a.y+b.y)*.5f},a,b,al);dl()->PopClipRect();
}
// These pages expose existing readers/renderers only. No placeholder features.
void player_page(int section) {
    if(section==0){
        card("ENEMY",2);toggle("Player ESP",&cfg::esp::enabled);toggle("Offscreen arrows",&cfg::esp::offscreen,&cfg::esp::offscreen_col);end_card();
        card("MESH CHAMS (EXPERIMENTAL)",6);
        toggle("Mesh chams",&cfg::mesh_chams::enabled,&cfg::mesh_chams::color);
        combo("Effect",&cfg::mesh_chams::effect,mesh_chams::effect_names,mesh_chams::effect_count);
        slider("Opacity",&cfg::mesh_chams::opacity,.05f,1.f,"%.2f");
        toggle("Outline",&cfg::mesh_chams::outline,&cfg::mesh_chams::outline_color);
        slider("Outline width",&cfg::mesh_chams::outline_px,1.f,6.f,"%.0f px");
        slider("Rim power",&cfg::mesh_chams::rim_power,.5f,6.f,"%.1f");
        end_card();
        
        ImGui::Dummy({0,12});
        card("ELEMENTS",13);
        toggle("Box",&cfg::esp::box,&cfg::esp::box_col);toggle("Nickname",&cfg::esp::name,&cfg::esp::name_col);toggle("Health",&cfg::esp::health,&cfg::esp::health_col);
        toggle("Armor",&cfg::esp::armor,&cfg::esp::armor_col);toggle("Distance",&cfg::esp::distance,&cfg::esp::distance_col);toggle("Weapon text",&cfg::esp::weapon_text,&cfg::esp::weapon_text_col);toggle("Weapon icon",&cfg::esp::weapon_icons,&cfg::esp::weapon_icon_col);
        toggle("Loadout icons",&cfg::esp::loadout,&cfg::esp::loadout_col);toggle("Ammo (experimental)",&cfg::esp::ammo);toggle("Bomb carrier",&cfg::esp::bomb,&cfg::esp::bomb_col);toggle("Money",&cfg::esp::money,&cfg::esp::money_col);toggle("Ping (experimental)",&cfg::esp::ping,&cfg::esp::ping_col);toggle("Tracers",&cfg::esp::tracer,&cfg::esp::tracer_col);end_card();
    }else if(section==1){
        card("BOX",3);const char* types[]={"Full","Corner"};combo("Style",&cfg::esp::box_type,types,2);
        slider("Thickness",&cfg::esp::box_thickness,1,3,"%.0f");slider(cfg::esp::box_type?"Corner size":"Rounding",cfg::esp::box_type?&cfg::esp::corner_length:&cfg::esp::box_rounding,cfg::esp::box_type?.12f:0,cfg::esp::box_type?.45f:8,cfg::esp::box_type?"%.2f":"%.0f");end_card();
        card("FONTS",5);combo("Nickname font",&cfg::esp::name_font,esp_fonts_ui::names,esp_fonts_ui::f_count);combo("Weapon font",&cfg::esp::weapon_font,esp_fonts_ui::names,esp_fonts_ui::f_count);
        slider("Text size",&cfg::esp::font_size,10,26,"%.0f");slider("Weapon icon size",&cfg::esp::weapon_icon_size,16,72,"%.0f");toggle("Label background",&cfg::esp::label_background);end_card();
        card("DETAILS",6);toggle("Outline",&cfg::esp::outline);toggle("Box fill",&cfg::esp::box_fill);slider("Fill opacity",&cfg::esp::fill_opacity,0,.3f,"%.2f");toggle("Health gradient",&cfg::esp::health_gradient);toggle("Health numbers",&cfg::esp::health_numbers);slider("Bar width",&cfg::esp::bar_width,2,8,"%.0f px");end_card();
        
    }else if(section==2){
        card("TARGET HUD",2);toggle("Target HUD",&cfg::misc::target_hud,&cfg::misc::target_hud_col);const char* spots[]={"Crosshair","Left","Top"};combo("Position",&cfg::misc::target_hud_pos,spots,3);end_card();
        if(cfg::misc::target_hud){target_sample();}
        ImGui::Dummy({0,12});
        const bool sized=cfg::scope::style==1||cfg::scope::style==2;
        card("CUSTOM SCOPE",sized?5:4);toggle("Custom scope",&cfg::scope::enabled,&cfg::scope::color);const char* looks[]={"Lines","Cross","Circle","Dot"};combo("Style",&cfg::scope::style,looks,4);
        if(sized){slider("Size",&cfg::scope::size,16,400,"%.0f px");}
        slider("Thickness",&cfg::scope::thickness,.5f,6,"%.1f px");toggle("No zoom",&cfg::scope::no_zoom);end_card();
        if(cfg::scope::enabled)scope_sample();
        ImGui::Dummy({0,12});
        card("DAMAGE MARKER (ESTIMATE)",2);toggle("Damage Marker",&cfg::misc::damage_marker,&cfg::misc::damage_color);slider("Fade time",&cfg::misc::damage_time,.2f,2.f,"%.1f s");end_card();ImGui::Dummy({0,12});
        card("OFFSCREEN ARROWS",5);toggle("Enabled",&cfg::esp::offscreen,&cfg::esp::offscreen_col);slider("Edge inset",&cfg::esp::offscreen_radius,24,240,"%.0f");slider("Arrow size",&cfg::esp::offscreen_size,8,48,"%.0f");toggle("Distance label",&cfg::esp::offscreen_dist);toggle("Player silhouette",&cfg::esp::offscreen_silhouette);end_card();
        card("TRACERS",3);toggle("Enabled",&cfg::esp::tracer,&cfg::esp::tracer_col);const char* origins[]={"Bottom","Center","Top"};combo("Origin",&cfg::esp::tracer_origin,origins,3);slider("Line width",&cfg::esp::tracer_thickness,.5f,4);end_card();
        card("DEATH EFFECT",1);toggle("Death animation",&cfg::misc::death_animation);end_card();
        ImGui::Dummy({0,12});
        card("COMBAT INFO",5);toggle("Hit log",&cfg::misc::hit_log);toggle("Bullet tracers",&cfg::misc::bullet_tracer,&cfg::misc::bullet_col);slider("Tracer time",&cfg::misc::bullet_time,.2f,5.f,"%.1f s");
        toggle("Footsteps",&cfg::misc::footsteps,&cfg::misc::footstep_col);toggle("Scope indicator",&cfg::misc::scope_indicator);end_card();
        
    }
}

void world_page(int,int col){
    if(col==0){
        const char* modes[]={"Linear","Exponential","Exp squared"};
        card("FOG",cfg::fog::mode==0?4:3);
        toggle("Fog",&cfg::fog::enabled,&cfg::fog::color);
        combo("Mode",&cfg::fog::mode,modes,3);
        if(cfg::fog::mode==0){
            slider("Start",&cfg::fog::start,0,250,"%.0f m");
            slider("End",&cfg::fog::end,1,500,"%.0f m");
            cfg::fog::end=std::max(cfg::fog::end,cfg::fog::start+1.f);
        }else slider("Density",&cfg::fog::density,.001f,.25f,"%.3f");
        end_card();
        ImGui::Dummy({0,18});
        card("PARTICLES",3);
        toggle("Soul",&cfg::particles::soul,&cfg::particles::soul_col);
        toggle("Snow",&cfg::particles::snow,&cfg::particles::snow_col);
        toggle("Rain",&cfg::particles::rain,&cfg::particles::rain_col);
        end_card();
        ImGui::Dummy({0,18});
        card("GRENADES",5);
        toggle("Grenade markers",&cfg::world_esp::grenade_tracking);
        toggle("Grenade timer",&cfg::world_esp::grenade_times);
        toggle("Grenade areas",&cfg::world_esp::grenade_effects);
        toggle("Flight trail",&cfg::world_esp::grenade_trail);
        toggle("Flight prediction",&cfg::world_esp::grenade_prediction);
        end_card();
        ImGui::Dummy({0,12});
        card("GRENADE DISPLAY",5);
        toggle("Grenade name",&cfg::world_esp::grenade_name);
        toggle("Badge background",&cfg::world_esp::grenade_background);
        toggle("Grenade distance",&cfg::world_esp::grenade_distance);
        slider("Marker scale",&cfg::world_esp::grenade_scale,.5f,2.f,"%.1f");
        slider("Forecast length",&cfg::world_esp::prediction_seconds,.25f,3.f,"%.2f s");
        end_card();
        
        ImGui::Dummy({0,12});
        const ImVec2 gp=ImGui::GetCursorScreenPos();const float gw=ImGui::GetContentRegionAvail().x;
        ImGui::Dummy({gw,110});
        world_visuals::timer_badge(dl(),{gp.x+gw*.5f,gp.y+45},91,{true,1.2f,2.f},false);

    }else{
        card("BOMB",1);
        toggle("Bomb timer",&cfg::world_esp::bomb_timer);
        end_card();
        
        ImGui::Dummy({0,18});
        card("DROPPED ITEMS",6);
        toggle("Dropped bomb",&cfg::world_esp::bomb_dropped);
        toggle("Dropped weapon",&cfg::world_esp::weapon_dropped);
        toggle("Dropped smoke",&cfg::world_esp::dropped_smoke);
        toggle("Dropped molotov",&cfg::world_esp::dropped_molotov);
        toggle("Dropped flash",&cfg::world_esp::dropped_flash);
        toggle("Dropped HE",&cfg::world_esp::dropped_he);
        end_card();
        card("DROPPED DISPLAY",2);
        toggle("Icons",&cfg::world_esp::dropped_icons);
        toggle("Name",&cfg::world_esp::dropped_names);
        end_card();
        
        ImGui::Dummy({0,18});
        card("ANTI EFFECTS (EXPERIMENTAL)",2);
        toggle("Anti flash (screen)",&cfg::anti_effects::flash);
        toggle("Anti smoke screen (partial)",&cfg::anti_effects::smoke_screen);
        end_card();
        
        
        
        ImGui::Dummy({0,18});
        card("SKY COLOR",1);toggle("Sky color",&cfg::world::sky,&cfg::world::sky_col);end_card();
        ImGui::Dummy({0,18});
        card("WORLD MODE",2);
        combo("Mode",&cfg::world_mode::mode,world_mode::names,4);
        slider("Strength",&cfg::world_mode::strength,0,1,"%.2f");
        end_card();
        
        ImGui::Dummy({0,18});
        card("NIGHTMODE",3);
        toggle("Night overlay",&cfg::misc::nightmode,&cfg::misc::nightmode_color);
        slider("Darkness",&cfg::misc::nightmode_opacity,0.f,0.88f,"%.2f");
        slider("Edge shade",&cfg::misc::nightmode_edges,0.f,0.5f,"%.2f");
        end_card();
        
    }
}
void settings_page(int section,int col){
    if(section==0){
        if(col==0){card("INTERFACE",4);toggle("Features / game overlay",&cfg::style::overlay_enabled);toggle("Glass tint",&cfg::style::liquid_glass);slider("Glass opacity",&cfg::style::glass_opacity,.3f,.95f,"%.2f");toggle("FPS / CPU / RAM",&cfg::style::stats_bar);end_card();}
        if(col==1){card("PRESETS",3);for(int i=0;i<3;++i){const char* names[]={"Clean","Contrast","Minimal"};if(ImGui::Button(names[i],{-1,44}))preset(i);}end_card();ImGui::Dummy({0,12});if(ImGui::Button("Exit program",{-1,44}))ImGui::OpenPopup("##exit");
            if(ImGui::BeginPopup("##exit")){ImGui::TextUnformatted("Restore visual changes and exit?");ImGui::Dummy({0,8});if(ImGui::Button("Exit",{120,44}))g_exit_requested=true;ImGui::SameLine(0,8);if(ImGui::Button("Cancel",{120,44}))ImGui::CloseCurrentPopup();ImGui::EndPopup();}}
    }else if(section==1){
        if(col==0){card("SCREEN CAPTURE",1);toggle("Show in screenshots",&cfg::style::capture_overlay);end_card();}
    }else if(section==3){
        if(col==0){card("VIEWMODEL POSITION",5);toggle("Viewmodel",&cfg::viewmodel::enabled);toggle("Override Position",&cfg::viewmodel::position);slider("Offset X",&cfg::viewmodel::x,-.4f,.4f,"%+.2f m");slider("Offset Y",&cfg::viewmodel::y,-.4f,.4f,"%+.2f m");slider("Offset Z",&cfg::viewmodel::z,-.4f,.4f,"%+.2f m");end_card();
            card("VIEWMODEL FOV",2);toggle("Override FOV",&cfg::viewmodel::fov_override);slider("Vertical FOV",&cfg::viewmodel::fov,30,140,"%.0f deg");end_card();}
        if(col==1){card("VIEWMODEL ANGLES",4);toggle("Override Angles",&cfg::viewmodel::angles);slider("Pitch",&cfg::viewmodel::pitch,-30,30,"%+.0f deg");slider("Yaw",&cfg::viewmodel::yaw,-30,30,"%+.0f deg");slider("Roll",&cfg::viewmodel::roll,-30,30,"%+.0f deg");end_card();
            if(ImGui::Button("Reset viewmodel",{-1,44})){cfg::viewmodel::enabled=cfg::viewmodel::fov_override=false;cfg::viewmodel::position=cfg::viewmodel::angles=true;cfg::viewmodel::x=cfg::viewmodel::y=cfg::viewmodel::z=cfg::viewmodel::roll=cfg::viewmodel::pitch=cfg::viewmodel::yaw=0;cfg::viewmodel::fov=60;}ImGui::Dummy({0,12});
            ImGui::Dummy({0,12});}
    }else if(section==4){
        if(col==0){card("CAMERA (EXPERIMENTAL)",4);toggle("Override Fov",&cfg::camera_override::fov);slider("Vertical FOV",&cfg::camera_override::fov_value,30,140,"%.0f deg");toggle("Override Aspect Ratio",&cfg::camera_override::aspect);slider("Aspect ratio",&cfg::camera_override::aspect_value,.75f,3.f,"%.2f");end_card();
            if(ImGui::Button("Reset camera",{-1,44})){cfg::camera_override::fov=cfg::camera_override::aspect=false;cfg::camera_override::fov_value=75;cfg::camera_override::aspect_value=1.77778f;}}

    }else{
        if(col==0){card("ESP PREVIEW",1);toggle("Separate preview",&cfg::style::preview_window);end_card();}
        if(col==1){card("WEAPON SAMPLE",1);const char* names[weapon_catalog::definitions.size()];for(size_t i=0;i<weapon_catalog::definitions.size();++i)names[i]=weapon_catalog::definitions[i].name;combo("Sample",&preview_weapon,names,int(weapon_catalog::definitions.size()));end_card();const auto& d=weapon_catalog::definitions[size_t(preview_weapon)];ImVec2 p=ImGui::GetCursorScreenPos();ImGui::Dummy({120,70});weapon_icons::draw(dl(),&d,{p.x+8,p.y+16},100,color(white));}
    }
}
struct SkinEntry {const char* weapon;const char* name;const char* symbol;int id;};
// Skin-definition IDs from the supplied dump; not weapon raw IDs.
constexpr SkinEntry skin_entries[]={{"FabM","Vein","FabM_Vein",240083},{"Karambit","White Rose","KnifeKarambit_WhiteRose",240275}};
auto& selected_skins=cfg::skins::selected;
void skin_image(int index,ImVec2 p,float w){
    ImVec2 a,b;if(preview_assets::uv(preview_assets::skin_rects[index],a,b))dl()->AddImage(ImGui::GetIO().Fonts->TexID,p,{p.x+w,p.y+w*.5f},a,b,color({1,1,1,1}));
}
void skins_page(int& section,bool compact){
    const ImVec2 origin=ImGui::GetCursorScreenPos();const float available=ImGui::GetContentRegionAvail().x;
    const float list_w=compact?available:238.f;const float gap=24.f;
    label(origin,"REPLACE FABM WITH",dim,14);const float tile_h=compact?94.f:126.f;
    for(int i=0;i<2;++i){
        ImVec2 q(origin.x,origin.y+28+i*(tile_h+12));
        glass_decor(dl(),q,{list_w,tile_h},12,ImGui::GetStyle().Alpha);
        if(section==i)dl()->AddRect(q,{q.x+list_w,q.y+tile_h},color({.62f,.42f,.68f,.42f}),12,0,1.f);
        label({q.x+16,q.y+14},skin_entries[i].name,section==i?pink:white);
        const float image_w=compact?145.f:192.f;
        skin_image(i,{q.x+list_w-image_w-10,q.y+26},image_w);
        ImGui::SetCursorScreenPos(q);ImGui::PushID(910+i);
        if(ImGui::InvisibleButton("##weapon",{list_w,tile_h}))section=i;ImGui::PopID();
    }
    ImVec2 q(compact?origin.x:origin.x+list_w+gap,compact?origin.y+28+2*(tile_h+12)+12:origin.y);
    float rw=compact?available:available-list_w-gap;
    label(q,"REPLACEMENT PREVIEW",dim,14);q.y+=28;
    const auto& skin=skin_entries[section];const float art_w=std::min(rw-32,480.f);
    const float pane_h=compact?art_w*.5f+156.f:370.f;
    glass_decor(dl(),q,{rw,pane_h},14,ImGui::GetStyle().Alpha);
    char binding[80];std::snprintf(binding,sizeof binding,"FabM -> %s",skin.weapon);
    label({q.x+24,q.y+20},binding,dim,15);
    label({q.x+24,q.y+48},skin.name,white,28);
    char id[40];std::snprintf(id,sizeof id,"ID %d",skin.id);
    const float idw=ImGui::CalcTextSize(id).x+24;
    dl()->AddRectFilled({q.x+rw-idw-20,q.y+18},{q.x+rw-20,q.y+48},color({.10f,.075f,.115f,1}),7);
    label({q.x+rw-idw-8,q.y+25},id,pink,14);
    skin_image(section,{q.x+(rw-art_w)*.5f,q.y+86},art_w);
    ImVec2 button(q.x+20,q.y+pane_h-60);ImGui::SetCursorScreenPos(button);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,14.f);ImGui::PushStyleColor(ImGuiCol_Button,{.16f,.105f,.18f,1});
    if(ImGui::Button(selected_skins[0]==skin.id?"Selected":"Select skin",{rw-40,44}))
        {selected_skins[0]=selected_skins[0]==skin.id?0:skin.id;selected_skins[1]=0;}
    ImGui::PopStyleColor();ImGui::PopStyleVar(2);
    ImGui::SetCursorScreenPos({origin.x,q.y+pane_h+12});ImGui::Dummy({available,1});
}
const char* section_name(int p,int s){static const char* names[6][5]={{"Players","Layout","Indicators",nullptr,nullptr},{"World",nullptr,nullptr,nullptr,nullptr},{"Interface","Capture","Preview","Viewmodel","Camera"},{"Vein","White Rose",nullptr,nullptr,nullptr},{"Touch aim",nullptr,nullptr,nullptr,nullptr},{"No recoil",nullptr,nullptr,nullptr,nullptr}};return names[p][s];}
int section_count(int p){return p==0?3:p==1?1:p==2?5:p==3?2:1;}
void preview_window(ImGuiIO& io){
    const float w=std::min(360.f,io.DisplaySize.x-32),h=std::min(560.f,io.DisplaySize.y-32);
    ImGui::SetNextWindowSize({w,h});ImGui::SetNextWindowPos({16,16},ImGuiCond_FirstUseEver);
    if(ImGui::Begin("ESP preview",&cfg::style::preview_window,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings)){
        sample(weapon_catalog::definitions[size_t(preview_weapon)].raw_id,std::max(180.f,ImGui::GetContentRegionAvail().y));
    }ImGui::End();
}
void nav_icon(ImDrawList* d,ImVec2 p,int kind,ImU32 c){
    if(kind==5){d->AddCircle(p,6,c,24,1.5f);d->AddLine({p.x-9,p.y},{p.x-3,p.y},c,1.5f);d->AddLine({p.x+3,p.y},{p.x+9,p.y},c,1.5f);d->AddLine({p.x,p.y-9},{p.x,p.y-3},c,1.5f);d->AddLine({p.x,p.y+3},{p.x,p.y+9},c,1.5f);}
    else if(kind==4){d->AddCircle(p,7,c,24,1.4f);d->AddCircle(p,3,c,20,1.2f);d->AddCircleFilled(p,1,c,8);}
    else if(kind==0){d->AddEllipse(p,7,4,c,0,20,1.3f);d->AddCircleFilled(p,2,c,12);}
    else if(kind==1){d->AddCircle(p,6,c,20,1.3f);d->AddEllipse(p,3,6,c,0,20,1);d->AddLine({p.x-6,p.y},{p.x+6,p.y},c);}
    else if(kind==3){
        d->AddRectFilled({p.x-8,p.y-5},{p.x+8,p.y-1},c,1);
        d->AddQuadFilled({p.x-7,p.y-1},{p.x,p.y-1},{p.x-2,p.y+7},{p.x-7,p.y+7},c);
        d->AddRect({p.x,p.y-1},{p.x+4,p.y+3},c,1,0,1.5f);
    }
    else {d->AddCircle(p,5,c,16,1.3f);for(int k=0;k<8;++k){float a=k*3.14159265f/4;d->AddLine({p.x+6*std::cos(a),p.y+6*std::sin(a)},{p.x+8*std::cos(a),p.y+8*std::sin(a)},c,1.3f);}}
}
// Reference-style solid glyphs, drawn as vectors (no external icon font needed).
void preview_icon(ImVec2 p,int kind,ImU32 c){
    auto* d=dl();
    if(kind<=1){
        d->AddCircleFilled({p.x,p.y-6},4.2f,c,20);
        d->AddRectFilled({p.x-6.8f,p.y+1},{p.x+6.8f,p.y+10},c,3.f,ImDrawFlags_RoundCornersTop);
        if(kind==1)d->AddRectFilled({p.x+7,p.y-5},{p.x+13,p.y-3},c,1);
    }else if(kind==2){
        for(int i=0;i<3;++i){float y=p.y-7+i*7;d->AddCircleFilled({p.x-8,y},1.5f,c,8);d->AddRectFilled({p.x-3,y-1.2f},{p.x+9,y+1.2f},c,.6f);}
    }else if(kind==3){
        for(int i=0;i<8;++i){float a=i*IM_PI/4;ImVec2 u(std::cos(a),std::sin(a)),v(-u.y,u.x);
            d->AddQuadFilled({p.x+u.x*5+v.x*2.5f,p.y+u.y*5+v.y*2.5f},{p.x+u.x*10+v.x*2.5f,p.y+u.y*10+v.y*2.5f},{p.x+u.x*10-v.x*2.5f,p.y+u.y*10-v.y*2.5f},{p.x+u.x*5-v.x*2.5f,p.y+u.y*5-v.y*2.5f},c);}
        d->AddCircleFilled(p,8,c,24);d->AddCircleFilled(p,3,color({.025f,.025f,.028f,1}),16);
    }else if(kind==4){
        d->PathLineTo({p.x,p.y-12});d->PathBezierCubicCurveTo({p.x-3,p.y-5},{p.x-8,p.y+1},{p.x-8,p.y+5});
        d->PathBezierCubicCurveTo({p.x-8,p.y+15},{p.x+8,p.y+15},{p.x+8,p.y+5});
        d->PathBezierCubicCurveTo({p.x+8,p.y+1},{p.x+3,p.y-5},{p.x,p.y-12});d->PathFillConvex(c);
    }else{
        d->AddRectFilled({p.x-7,p.y-11},{p.x-5,p.y+12},c,1);
        d->AddQuadFilled({p.x-5,p.y-10},{p.x+9,p.y-8},{p.x+9,p.y+2},{p.x-5,p.y+1},c);
    }
}
void preview_toolbar(){
    const ImVec2 p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x;
    const float badge=112.f,step=44.f;
    dl()->AddRectFilled(p,{p.x+badge,p.y+40},color({.075f,.058f,.083f,1}),9);
    preview_icon({p.x+18,p.y+20},0,color(pink));label({p.x+35,p.y+12},"Enemies",pink);
    ImGui::InvisibleButton("Enemies##players",{badge,44});if(ImGui::IsItemClicked())sections[0]=0;
    const char* ids[]={"Player ESP##preview","Layout##preview","Indicators##preview"};
    for(int i=0;i<3;++i){const ImVec2 q(p.x+badge+12+i*step,p.y);ImGui::SetCursorScreenPos(q);
        if(ImGui::InvisibleButton(ids[i],{step,44})){if(i==0)cfg::esp::enabled=!cfg::esp::enabled;else sections[0]=i;}
        preview_icon({q.x+step*.5f,q.y+20},i==2?5:i+1,color(ImGui::IsItemHovered()?white:ImVec4(.43f,.43f,.45f,1)));
    }
    ImGui::SetCursorScreenPos({p.x,p.y+52});
}
} // namespace
void render(){
    bar::render();if(bar::g_alpha<.001f||bar::game_alpha()<.001f)return;
    auto& io=ImGui::GetIO();const ImGuiStyle saved=ImGui::GetStyle();auto& st=ImGui::GetStyle();
    st.Alpha=bar::g_alpha*bar::game_alpha();st.WindowPadding={16,12};st.WindowRounding=20;st.WindowBorderSize=1;
    st.ChildRounding=12;st.ChildBorderSize=1;st.FrameRounding=10;st.FrameBorderSize=1;st.FramePadding={10,13};st.ItemSpacing={0,0};st.ItemInnerSpacing={8,6};st.CellPadding={10,0};st.ScrollbarSize=6;st.PopupRounding=10;
    auto* c=st.Colors;c[ImGuiCol_TitleBg]={.045f,.045f,.05f,1};c[ImGuiCol_TitleBgActive]={.09f,.065f,.095f,1};c[ImGuiCol_TitleBgCollapsed]={.045f,.045f,.05f,1};c[ImGuiCol_Text]=white;c[ImGuiCol_TextDisabled]=dim;c[ImGuiCol_WindowBg]={.025f,.025f,.028f,.99f};c[ImGuiCol_ChildBg]={.064f,.064f,.069f,1};c[ImGuiCol_PopupBg]={.035f,.035f,.04f,.99f};c[ImGuiCol_Border]={1,1,1,.065f};
    c[ImGuiCol_FrameBg]={.08f,.08f,.085f,1};c[ImGuiCol_FrameBgHovered]={.12f,.11f,.13f,1};c[ImGuiCol_FrameBgActive]={.17f,.12f,.18f,1};c[ImGuiCol_Button]={.07f,.07f,.075f,1};c[ImGuiCol_ButtonHovered]={.14f,.11f,.15f,1};c[ImGuiCol_ButtonActive]={.19f,.13f,.20f,1};
    c[ImGuiCol_Header]={.19f,.13f,.20f,1};c[ImGuiCol_HeaderHovered]={.23f,.16f,.24f,1};c[ImGuiCol_HeaderActive]={.25f,.17f,.26f,1};c[ImGuiCol_CheckMark]=pink;c[ImGuiCol_SliderGrab]=pink;c[ImGuiCol_NavHighlight]=pink;c[ImGuiCol_ScrollbarBg]={0,0,0,0};c[ImGuiCol_ScrollbarGrab]={.20f,.19f,.22f,1};
    if(cfg::style::liquid_glass)c[ImGuiCol_WindowBg]={.03f,.025f,.035f,std::clamp(cfg::style::glass_opacity,.3f,.95f)};
    ImGui::PushFont(fontDesc?fontDesc:ImGui::GetFont());
    float w=std::min(1000.f,std::max(280.f,io.DisplaySize.x-32)),h=std::min(600.f,std::max(260.f,io.DisplaySize.y-96));
    bool compact=w<720;ImGui::SetNextWindowSize({w,h});ImGui::SetNextWindowPos({(io.DisplaySize.x-w)*.5f,(io.DisplaySize.y-h)*.5f},ImGuiCond_FirstUseEver);
    if(ImGui::Begin("##reference_menu",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings)){
        ImVec2 wp=ImGui::GetWindowPos();wp={std::clamp(wp.x,8.f,std::max(8.f,io.DisplaySize.x-w-8)),std::clamp(wp.y,8.f,std::max(8.f,io.DisplaySize.y-h-8))};ImGui::SetWindowPos(wp);
        dl()->AddRectFilled({wp.x+1,wp.y+1},{wp.x+w-1,wp.y+56},color({.045f,.045f,.05f,1}),20,ImDrawFlags_RoundCornersTop);
        const ImVec2 p=ImGui::GetCursorScreenPos();float avail=w-32;
        ImVec2 logo_a,logo_b;
        if(preview_assets::uv(preview_assets::logo_rect,logo_a,logo_b))
            dl()->AddImage(io.Fonts->TexID,{p.x-2,p.y+4},{p.x+38,p.y+44},logo_a,logo_b,color(white));
        else nav_icon(dl(),{p.x+16,p.y+22},0,color(white));
        glass_decor(dl(),{p.x+44,p.y},{112,44},9,st.Alpha);
        ImGui::SetCursorScreenPos({p.x+44,p.y});ImGui::PushStyleColor(ImGuiCol_Button,{0,0,0,0});ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
        if(ImGui::Button(preset_name,{112,44}))ImGui::OpenPopup("##profiles");ImGui::PopStyleVar();ImGui::PopStyleColor();
        if(ImGui::BeginPopup("##profiles")){const char* names[]={"Clean","Contrast","Minimal"};for(int i=0;i<3;++i)if(ImGui::Selectable(names[i],false,0,{160,44})){preset(i);ImGui::CloseCurrentPopup();}ImGui::EndPopup();}
        // No close button in the header: the floating logo button opens / closes the menu.
        int& section=sections[page];section=std::clamp(section,0,section_count(page)-1);
        if(!compact){float total=0;for(int i=0;i<section_count(page);++i)total+=ImGui::CalcTextSize(section_name(page,i)).x+24;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0,0,0,0});ImGui::PushStyleColor(ImGuiCol_ButtonActive,{0,0,0,0});
            float x=p.x+avail-total;
            for(int i=0;i<section_count(page);++i){
                ImGui::SetCursorScreenPos({x,p.y});const char* text=section_name(page,i);
                float tw=ImGui::CalcTextSize(text).x+24;ImGui::PushID(100+page*10+i);
                float selected=animate("##tab_selected",section==i?1.f:0.f);
                if(ImGui::InvisibleButton(text,{tw,44}))section=i;
                ImFont* face=section==i&&fontBold?fontBold:(fontDesc?fontDesc:ImGui::GetFont());
                float text_w=face->CalcTextSizeA(16.f,1000.f,0.f,text).x;
                dl()->AddText(face,16.f,{x+(tw-text_w)*.5f,p.y+14},color(blend(dim,white,selected)),text);
                ImGui::PopID();x+=tw;
            }
            ImGui::PopStyleColor(2);ImGui::PopStyleVar();
            ImGui::SetCursorScreenPos({p.x+168,p.y});ImGui::InvisibleButton("##drag",{std::max(4.f,p.x+avail-total-(p.x+168)-8),44});
        }else{ImGui::SetCursorScreenPos(p);ImGui::InvisibleButton("##drag",{36,44});}
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0)){ImGui::SetWindowPos({wp.x+io.MouseDelta.x,wp.y+io.MouseDelta.y});}
        float body_y=p.y+60;
        if(compact){ImGui::SetCursorScreenPos({p.x,body_y});ImGui::SetNextItemWidth(avail);if(ImGui::BeginCombo("##section",section_name(page,section),ImGuiComboFlags_HeightLarge)){for(int i=0;i<section_count(page);++i)if(ImGui::Selectable(section_name(page,i),section==i,0,{0,44}))section=i;ImGui::EndCombo();}body_y+=56;}
        float body_h=wp.y+h-72-body_y;
        ImGui::SetCursorScreenPos({p.x,body_y});
        float left_w=page==0&&!compact?avail*.57f:avail;
        ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0});ImGui::BeginChild("##settings_body",{left_w,body_h},false);ImGui::PopStyleColor();
        static int last=-1;static float transition=1.f;int key=page*10+section;
        if(last!=key){ImGui::SetScrollY(0);last=key;transition=0.f;}
        transition+=(1.f-transition)*(1.f-std::exp(-18.f*std::clamp(io.DeltaTime,0.f,.1f)));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,st.Alpha*(.25f+.75f*transition));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+8.f*(1.f-transition));
        ImGui::PushID(key);
        if(page==0)player_page(section);
        else if(page==4){
            card("TOUCH AIM (EXPERIMENTAL)",4);
            toggle("Touch aim",&cfg::legit::enabled);
            toggle("Visible check (blocks aim)",&cfg::legit::visible_check);
            slider("Smooth",&cfg::legit::smooth,.025f,1.f,"%.3f");
            const char* bones[]={"Head estimate","Chest estimate","Pelvis estimate"};combo("Target point",&cfg::legit::bone,bones,3);
            end_card();

            ImGui::Dummy({0,14});
            card("SILENT AIM (EXPERIMENTAL)",4);
            toggle("Silent aim",&cfg::silent::enabled);
            slider("FOV radius (px)",&cfg::silent::fov,10.f,500.f,"%.0f px");
            const char* sa_bones[]={"Head","Chest","Pelvis"};combo("Target point",&cfg::silent::bone,sa_bones,3);
            slider("Max distance (m)",&cfg::silent::max_dist,10.f,200.f,"%.0f m");
            end_card();

            ImGui::TextWrapped("%s",silent_aim::status);

            ImGui::Dummy({0,14});
            card("FOV",2);
            toggle("Draw FOV",&cfg::legit::draw_fov,&cfg::legit::fov_color);
            slider("FOV radius (px)",&cfg::legit::fov,10.f,360.f,"%.0f px");
            end_card();
            card("TOUCH POINT",2);
            toggle("Move touch point (+)",&cfg::legit::edit_position);
            slider("Touch gain",&cfg::legit::gain,.01f,.5f,"%.2f");
            end_card();
            
        }
        else if(page==5){
            card("NO RECOIL (EXPERIMENTAL)",1);toggle("No recoil",&cfg::rage::norecoil);end_card();
            ImGui::Dummy({0,14});
            card("TRIGGERBOT (EXPERIMENTAL)",3);
            toggle("Triggerbot",&cfg::trigger::enabled);
            int cd_ms=int(cfg::trigger::cooldown_ms);if(slider("Cooldown (ms)",&cd_ms,100,1000,"%d ms"))cfg::trigger::cooldown_ms=cd_ms;
            int jt_ms=int(cfg::trigger::jitter_ms);if(slider("Jitter (ms)",&jt_ms,0,200,"%d ms"))cfg::trigger::jitter_ms=jt_ms;
            end_card();
            ImGui::TextWrapped("%s",triggerbot::status);
        }
        else if(page==3)skins_page(section,compact);
        else {
            if(ImGui::BeginTable("##columns",compact?1:2,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoSavedSettings)){
                for(int col=0;col<2;++col){ImGui::TableNextColumn();if(compact&&col>0)ImGui::Dummy({0,16});ImGui::PushID(col);if(page==1)world_page(section,col);else settings_page(section,col);ImGui::PopID();}ImGui::EndTable();}
        }
        ImGui::PopID();ImGui::PopStyleVar();
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)&&ImGui::IsMouseDragging(0,6)&&!ImGui::IsAnyItemActive())ImGui::SetScrollY(ImGui::GetScrollY()-io.MouseDelta.y);
        ImGui::EndChild();
        if(page==0&&!compact){ImGui::SetCursorScreenPos({p.x+left_w+24,body_y});ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0});ImGui::BeginChild("##integrated_preview",{avail-left_w-24,body_h},false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);ImGui::PopStyleColor();
            preview_toolbar();
            sample(weapon_catalog::definitions[size_t(preview_weapon)].raw_id,std::max(180.f,body_h-52));ImGui::EndChild();}
        const float dock_w=std::min(600.f,avail),cell=(dock_w-8)/6;ImVec2 dock(p.x+(avail-dock_w)*.5f,wp.y+h-60);
        glass_decor(dl(),dock,{dock_w,44},22,st.Alpha);
        const int order[]={4,5,0,1,3,2};int active=page==4?0:page==5?1:page==0?2:page==1?3:page==3?4:5;
        const float dock_position=animate("##dock_position",float(active),16.f);
        ImVec2 pill(dock.x+4+dock_position*cell,dock.y);
        dl()->AddRectFilled(pill,{pill.x+cell,pill.y+44},color({.10f,.075f,.11f,1}),20);
        const char* names[]={"Legit","Rage","Visuals","World","Skins","Settings"};
        for(int i=0;i<6;++i){ImVec2 q(dock.x+4+i*cell,dock.y);ImGui::SetCursorScreenPos(q);ImGui::PushID(800+i);ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,20);ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));if(ImGui::Button("##nav",{cell,44}))page=order[i];ImGui::PopStyleColor();ImGui::PopStyleVar(2);
            float selected=animate("##nav_selected",page==order[i]?1.f:0.f);float tx=compact?q.x+cell*.5f-7:q.x+(cell-ImGui::CalcTextSize(names[i]).x-24)*.5f;nav_icon(dl(),{tx+7,q.y+22},order[i],color(blend(dim,pink,selected)));if(!compact)label({tx+22,q.y+14},names[i],blend(dim,white,selected));ImGui::PopID();}
    }
    ImGui::End();if(cfg::style::preview_window)preview_window(io);ImGui::PopFont();ImGui::GetStyle()=saved;
}
}
