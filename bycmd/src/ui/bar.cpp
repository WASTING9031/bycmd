#include "bar.hpp"
#include "cfg.hpp"
#include "theme/theme.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "preview_assets.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>

extern ImFont *fontBold,*fontMedium;

namespace ui::bar {
namespace {
// ── Real process stats from /proc (no fake numbers) ─────────────────────────
struct stats_t { float cpu=-1.f; float ram_mb=-1.f; };
stats_t g_stats;
int g_last_pid=-2; unsigned long long g_last_ticks=0; double g_last_wall=0;

bool read_ticks(int pid,unsigned long long& ticks) {
    char path[64];std::snprintf(path,sizeof path,pid>0?"/proc/%d/stat":"/proc/self/stat",pid);
    FILE* f=std::fopen(path,"r");if(!f)return false;
    char buf[1024];size_t n=std::fread(buf,1,sizeof buf-1,f);std::fclose(f);buf[n]=0;
    const char* p=std::strrchr(buf,')');if(!p)return false;   // comm may contain spaces
    unsigned long long ut=0,stt=0;
    // after ") ": state(3) ... utime is field 14, stime field 15
    if(std::sscanf(p+2,"%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu",&ut,&stt)!=2)return false;
    ticks=ut+stt;return true;
}
bool read_rss(int pid,float& mb) {
    char path[64];std::snprintf(path,sizeof path,pid>0?"/proc/%d/statm":"/proc/self/statm",pid);
    FILE* f=std::fopen(path,"r");if(!f)return false;
    unsigned long long size=0,res=0;int ok=std::fscanf(f,"%llu %llu",&size,&res);std::fclose(f);
    if(ok!=2)return false;
    mb=float(double(res)*double(sysconf(_SC_PAGESIZE))/(1024.0*1024.0));return true;
}
void update_stats() {
    const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const int pid=g_stats_pid>0?g_stats_pid:0;
    if(pid!=g_last_pid){g_last_pid=pid;g_last_wall=0;g_stats={};}
    if(g_last_wall>0&&now-g_last_wall<1.0)return;      // refresh once per second
    unsigned long long t=0;
    if(read_ticks(pid,t)){
        if(g_last_wall>0&&t>=g_last_ticks){
            const double hz=double(sysconf(_SC_CLK_TCK));
            g_stats.cpu=float(double(t-g_last_ticks)/hz/(now-g_last_wall)*100.0); // >100% = several cores
        }
        g_last_ticks=t;
    } else g_stats.cpu=-1.f;
    float mb=0;g_stats.ram_mb=read_rss(pid,mb)?mb:-1.f;
    g_last_wall=now;
}

// ── Icons (vector, so they work with any font) ──────────────────────────────
void icon_flag(ImDrawList* d,ImVec2 c,float s,ImU32 col) {
    const float h=s*.5f;
    d->AddRectFilled(ImVec2(c.x-h,c.y-h),ImVec2(c.x-h+s*.14f,c.y+h),col,1);          // pole
    d->AddRectFilled(ImVec2(c.x-h+s*.14f,c.y-h+s*.04f),ImVec2(c.x+h*.25f,c.y-h+s*.30f),col);
    d->AddRectFilled(ImVec2(c.x-h+s*.30f,c.y-h+s*.30f),ImVec2(c.x+h,c.y+h*.32f),col,1); // flag
}
void icon_feather(ImDrawList* d,ImVec2 c,float s,ImU32 col) {
    // leaf body leaning right, with a quill line through it
    const float h=s*.5f;
    ImVec2 a(c.x-h*.80f,c.y+h),tip(c.x+h*.85f,c.y-h);
    d->PathClear();
    d->PathLineTo(ImVec2(c.x-h*.35f,c.y+h*.35f));
    d->PathBezierCubicCurveTo(ImVec2(c.x-h*.75f,c.y-h*.20f),ImVec2(c.x-h*.10f,c.y-h*.95f),tip,12);
    d->PathBezierCubicCurveTo(ImVec2(c.x+h*.75f,c.y-h*.15f),ImVec2(c.x+h*.25f,c.y+h*.45f),ImVec2(c.x-h*.35f,c.y+h*.35f),12);
    d->PathFillConvex(col);
    d->AddLine(a,ImVec2(c.x+h*.25f,c.y-h*.30f),IM_COL32(0,0,0,(col>>IM_COL32_A_SHIFT)&0xFF),s*.08f);
    d->AddLine(a,ImVec2(c.x-h*.30f,c.y+h*.40f),col,s*.10f);
}
void icon_ram(ImDrawList* d,ImVec2 c,float s,ImU32 col) {
    const float w=s*.62f,h=s*.24f;
    d->AddRectFilled(ImVec2(c.x-w,c.y-h),ImVec2(c.x+w,c.y+h),col,1.5f);
    const ImU32 hole=IM_COL32(0,0,0,(col>>IM_COL32_A_SHIFT)&0xFF);
    for(int i=0;i<4;++i){float x=c.x-w+s*.16f+i*(2*w-s*.32f)/3.f;d->AddRectFilled(ImVec2(x-s*.06f,c.y-h*.45f),ImVec2(x+s*.06f,c.y+h*.45f),hole);}
    for(int i=0;i<6;++i){float x=c.x-w+s*.10f+i*(2*w-s*.20f)/5.f;d->AddRectFilled(ImVec2(x-s*.035f,c.y+h),ImVec2(x+s*.035f,c.y+h+s*.16f),col);}
}

// A single floating launcher. This uses the user's existing atlas logo, not a
// sample-player watermark. Position is session-local (no new settings file).
struct launcher_t {
    ImVec2 pos{}, viewport{}, press_mouse{}, press_pos{};
    float diameter=76.f,hover=0.f;
    bool initialized=false,user_moved=false,armed=false,dragged=false;
};
launcher_t launcher;

ImVec2 clamp_launcher(ImVec2 p,ImVec2 viewport,float size){
    return {std::clamp(p.x,8.f,std::max(8.f,viewport.x-size-8.f)),
            std::clamp(p.y,4.f,std::max(4.f,viewport.y-size-4.f))};
}
void fit_launcher(ImVec2 viewport){
    // A 76px circle on wide landscape screens. When the menu nearly fills the
    // width, use the free top margin (minimum 44px touch target), not its X button.
    const bool side_space=(viewport.x-1000.f)*.5f>=100.f;
    const float menu_h=std::min(600.f,std::max(260.f,viewport.y-96.f));
    const float top_gap=std::max(0.f,(viewport.y-menu_h)*.5f);
    const float size=std::min(std::min(viewport.x,viewport.y)-8.f,
                             side_space?76.f:std::clamp(top_gap-8.f,44.f,76.f));
    const bool changed=!launcher.initialized||viewport.x!=launcher.viewport.x||viewport.y!=launcher.viewport.y;
    if(changed){
        ImVec2 next;
        if(launcher.initialized&&launcher.user_moved&&launcher.viewport.x>0&&launcher.viewport.y>0){
            next={(launcher.pos.x+launcher.diameter*.5f)/launcher.viewport.x*viewport.x-size*.5f,
                  (launcher.pos.y+launcher.diameter*.5f)/launcher.viewport.y*viewport.y-size*.5f};
        }else next={viewport.x-size-16.f,side_space?24.f:std::max(4.f,top_gap-size-4.f)};
        launcher.diameter=size;launcher.pos=clamp_launcher(next,viewport,size);
        launcher.viewport=viewport;launcher.initialized=true;
        // A rotation while held must never become a tap on release.
        launcher.armed=launcher.dragged=false;
    }else launcher.pos=clamp_launcher(launcher.pos,viewport,launcher.diameter);
}
bool launcher_contains(ImVec2 p){
    const float r=launcher.diameter*.5f,dx=p.x-launcher.pos.x-r,dy=p.y-launcher.pos.y-r;
    return dx*dx+dy*dy<=r*r;
}
} // namespace

void set_game_alpha(float a) { g_game_alpha=std::clamp(a,0.f,1.f); }
float game_alpha() { return g_game_alpha; }

// Stats pill: [flag] FPS 120   [feather] CPU 118%   [ram] RAM 2417MB  (bottom-centre)
void render_stats() {
    if(!cfg::style::overlay_enabled||!cfg::style::stats_bar||g_game_alpha<.001f)return;
    update_stats();
    ImGuiIO& io=ImGui::GetIO();
    ImDrawList* d=ImGui::GetForegroundDrawList();
    ImFont* lf=fontMedium?fontMedium:ImGui::GetFont();
    ImFont* vf=fontBold?fontBold:lf;
    const float fs=16.f,isz=14.f,gap_icon=6.f,gap_lv=6.f,gap_grp=14.f,pad_x=14.f,h=34.f;
    char v[3][24];
    std::snprintf(v[0],24,"%.0f",io.Framerate);
    if(g_stats.cpu>=0)std::snprintf(v[1],24,"%.0f%%",g_stats.cpu);else std::snprintf(v[1],24,"--");
    if(g_stats.ram_mb>=0)std::snprintf(v[2],24,"%.0fMB",g_stats.ram_mb);else std::snprintf(v[2],24,"--");
    const char* lab[3]={"FPS","CPU","RAM"};
    float lw[3],vw[3],total=pad_x*2;
    for(int i=0;i<3;++i){lw[i]=lf->CalcTextSizeA(fs,FLT_MAX,0,lab[i]).x;vw[i]=vf->CalcTextSizeA(fs,FLT_MAX,0,v[i]).x;
        total+=isz+gap_icon+lw[i]+gap_lv+vw[i]+(i<2?gap_grp:0);}
    const float a=g_game_alpha;
    const float x0=std::floor(io.DisplaySize.x*.5f-total*.5f),y0=std::max(8.f,io.DisplaySize.y-h-12.f),r=h*.5f;
    // soft shadow
    for(int i=4;i>=1;--i){float e=i*3.f;d->AddRectFilled(ImVec2(x0-e,y0-e+3),ImVec2(x0+total+e,y0+h+e+3),IM_COL32(0,0,0,int(16*a)),r+e);}
    if(cfg::style::liquid_glass){
        d->AddRectFilled(ImVec2(x0,y0),ImVec2(x0+total,y0+h),IM_COL32(22,18,22,int(170*a)),r);
        d->AddRectFilled(ImVec2(x0+2,y0+1),ImVec2(x0+total-2,y0+h*.48f),IM_COL32(255,255,255,int(16*a)),r,ImDrawFlags_RoundCornersTop);
        d->AddRect(ImVec2(x0,y0),ImVec2(x0+total,y0+h),IM_COL32(255,255,255,int(40*a)),r,0,1.f);
        d->AddLine(ImVec2(x0+r,y0+1),ImVec2(x0+total-r,y0+1),IM_COL32(255,255,255,int(60*a)),1.f);
    } else {
        d->AddRectFilled(ImVec2(x0,y0),ImVec2(x0+total,y0+h),IM_COL32(16,13,15,int(235*a)),r);
        d->AddRect(ImVec2(x0,y0),ImVec2(x0+total,y0+h),IM_COL32(255,255,255,int(14*a)),r);
    }
    const ImU32 lc=IM_COL32(205,203,208,int(255*a)),vc=IM_COL32(250,250,252,int(255*a));
    float x=x0+pad_x;const float cy=y0+h*.5f,ty=cy-fs*.5f-1.f;
    for(int i=0;i<3;++i){
        ImVec2 ic(x+isz*.5f,cy);
        if(i==0)icon_flag(d,ic,isz*.92f,lc);else if(i==1)icon_feather(d,ic,isz,lc);else icon_ram(d,ic,isz,lc);
        x+=isz+gap_icon;d->AddText(lf,fs,ImVec2(x,ty),lc,lab[i]);x+=lw[i]+gap_lv;
        d->AddText(vf,fs,ImVec2(x,ty),vc,v[i]);x+=vw[i]+gap_grp;
    }
}

void render() {
    ImGuiIO& io=ImGui::GetIO();
    g_alpha += ((g_open?1.f:0.f)-g_alpha)*(1.f-std::exp(-14.f*io.DeltaTime));
    if(g_alpha<.001f && !g_open) g_alpha=0.f;
    if(g_game_alpha<.001f){launcher.armed=launcher.dragged=false;return;}
    render_stats();
    // The floating opener must not cover a colour picker or steal its touches.
    if(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)){
        launcher.armed=launcher.dragged=false;return;
    }

    if(!std::isfinite(io.DisplaySize.x)||!std::isfinite(io.DisplaySize.y)||io.DisplaySize.x<60||io.DisplaySize.y<60)return;
    fit_launcher(io.DisplaySize);
    const float size=launcher.diameter;
    ImGui::SetNextWindowPos(launcher.pos);
    ImGui::SetNextWindowSize({size,size});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.f);
    ImGui::Begin("##menu_launcher",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollWithMouse);
    // Keep hit testing consistent with the foreground watermark even if it is
    // dragged across the menu; do not steal keyboard/navigation focus every frame.
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    const bool clicked=ImGui::InvisibleButton("Open / close menu",{size,size});
    const bool active=ImGui::IsItemActive();
    const bool hovered=ImGui::IsItemHovered()&&launcher_contains(io.MousePos);
    if(ImGui::IsItemActivated()){
        launcher.armed=launcher_contains(io.MousePos);launcher.dragged=false;
        launcher.press_mouse=io.MousePos;launcher.press_pos=launcher.pos;
    }
    if(active&&launcher.armed&&io.MouseDown[0]){
        const ImVec2 delta(io.MousePos.x-launcher.press_mouse.x,io.MousePos.y-launcher.press_mouse.y);
        if(delta.x*delta.x+delta.y*delta.y>64.f)launcher.dragged=true;
        if(launcher.dragged){
            launcher.pos=clamp_launcher({launcher.press_pos.x+delta.x,launcher.press_pos.y+delta.y},io.DisplaySize,size);
            launcher.user_moved=true;ImGui::SetWindowPos(launcher.pos);
        }
    }
    if(!io.MouseDown[0]){
        if(clicked&&launcher.armed&&!launcher.dragged)g_open=!g_open;
        launcher.armed=launcher.dragged=false;
    }
    launcher.hover+=((hovered?1.f:0.f)-launcher.hover)*(1.f-std::exp(-18.f*std::clamp(io.DeltaTime,0.f,.1f)));
    ImDrawList* d=ImGui::GetForegroundDrawList();
    const ImVec2 center(launcher.pos.x+size*.5f,launcher.pos.y+size*.5f);const float r=size*.5f;
    const float alpha=std::clamp(g_game_alpha,0.f,1.f);
    d->AddCircleFilled({center.x,center.y+2.f},r+2.f,IM_COL32(0,0,0,int(30*alpha)),64);
    const bool held=active&&launcher.armed;
    const int shade=int(launcher.hover*7.f)-(held?5:0);
    d->AddCircleFilled(center,r,IM_COL32(45+shade,48+shade,56+shade,int(250*alpha)),64);
    d->AddCircle(center,r-.5f,IM_COL32(255,255,255,int((g_open?32:14)*alpha)),64,1.f);
    ImVec2 uv0,uv1;
    if(preview_assets::uv(preview_assets::logo_rect,uv0,uv1)){
        const float image=size*.72f;
        d->AddImage(io.Fonts->TexID,{center.x-image*.5f,center.y-image*.5f},{center.x+image*.5f,center.y+image*.5f},uv0,uv1,IM_COL32(255,255,255,int(255*alpha)));
    }
    if(!cfg::style::overlay_enabled){const ImVec2 p(center.x+r*.42f,center.y+r*.42f);for(float x:{-3.f,3.f})d->AddRectFilled({p.x+x-1,p.y-4},{p.x+x+1,p.y+4},IM_COL32(218,165,229,int(255*alpha)),1);}
    ImGui::End();ImGui::PopStyleVar(2);
}
}
