#pragma once
// Overlay-only world particles around the camera, projected by the same matrices as the ESP.
// HONEST LIMITS: no game depth, no roof/wall collision, no ground splashes, no audio. Each of
// Soul / Snow / Rain is an independent cosmetic field; several can run at once. World Mode may
// also request snow (g_extra_snow), OR-ed with the user's Snow switch, never changing it.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include "../game/math.hpp"
#include "../ui/cfg.hpp"

namespace particles {
inline char g_status[160]="Off. Overlay effect around the camera: no depth or collision.";
// World Mode snow density (0 = none); the user's own Snow switch is never touched.
inline float g_extra_snow=0.f;

enum kind_t { K_SOUL=0, K_SNOW=1, K_RAIN=2 };

namespace detail {
struct bit { Vector3 pos; float speed=1,phase=0,scale=1,alpha=1; };
struct field { std::vector<bit> bits; };
inline field fields[3];
inline uint32_t rng=0x51ed2719u;
inline Vector3 last_eye{}; inline long long last_frame=0;
inline float rnd(){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return float(rng&0xFFFFFF)/float(0x1000000); }
inline bool finite(const Vector3& v){ return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
inline float wrap(float v,float c,float r){ const float w=r*2; return c-r+(v-c+r-std::floor((v-c+r)/w)*w); }
inline float snow_density(){ return std::max(cfg::particles::snow?1.f:0.f,std::isfinite(g_extra_snow)?std::clamp(g_extra_snow,0.f,2.f):0.f); }
inline int target_count(int k){ return k==K_RAIN?1100:k==K_SNOW?int(700*snow_density()):170; }
// Field half-extents around the eye (m): small, so most bits are near enough to read.
inline Vector3 extent(int k){ return k==K_SOUL?Vector3(10.f,4.f,10.f):k==K_SNOW?Vector3(12.f,8.f,12.f):Vector3(10.f,8.f,10.f); }
inline void seed(field& f,int k,const Vector3& eye){
    f.bits.resize(size_t(target_count(k)));
    const Vector3 r=extent(k);
    for(auto& b:f.bits){
        b.pos=Vector3(eye.x+(rnd()*2-1)*r.x, eye.y+(rnd()*2-1)*r.y, eye.z+(rnd()*2-1)*r.z);
        b.speed=.7f+rnd()*.6f; b.phase=rnd()*6.2831853f; b.scale=.7f+rnd()*.8f; b.alpha=.55f+rnd()*.45f;
    }
}
inline bool project(const Vector3& p,const matrix& view,const matrix& proj,ImVec2& s,float& depth){
    const auto cam=matrix_multiply(view,Vector4(p.x,p.y,p.z,1)); depth=-cam.z;
    if(depth<.2f) return false;
    const auto clip=matrix_multiply(proj,cam);
    if(!std::isfinite(clip.w)||clip.w<.0001f) return false;
    s=ImVec2((clip.x/clip.w+1)*.5f*g_sw,(1-clip.y/clip.w)*.5f*g_sh);
    return std::isfinite(s.x)&&std::isfinite(s.y);
}
inline bool wants(int k){
    if(k==K_SOUL) return cfg::particles::soul;
    if(k==K_SNOW) return snow_density()>0.f;
    return cfg::particles::rain;
}
inline ImVec4 col(int k){
    return k==K_SOUL?cfg::particles::soul_col:k==K_SNOW?cfg::particles::snow_col:cfg::particles::rain_col;
}
} // namespace detail

inline bool any(){ return detail::wants(K_SOUL)||detail::wants(K_SNOW)||detail::wants(K_RAIN); }

inline void waiting(){
    using namespace detail;
    if(!any()){ for(auto& f:fields)f.bits.clear(); last_frame=0; std::snprintf(g_status,sizeof g_status,"Off. Overlay effect around the camera: no depth or collision."); }
    else std::snprintf(g_status,sizeof g_status,"Particles: waiting for live camera");
}

inline void draw(const matrix& view,const matrix& proj){
    using namespace detail;
    if(!any()){ waiting(); return; }
    const float* v=reinterpret_cast<const float*>(&view);
    const Vector3 eye(-(v[0]*v[12]+v[1]*v[13]+v[2]*v[14]),
                      -(v[4]*v[12]+v[5]*v[13]+v[6]*v[14]),
                      -(v[8]*v[12]+v[9]*v[13]+v[10]*v[14]));
    if(!finite(eye)||g_sw<=0||g_sh<=0) return;
    const long long now=diag::now_ms();
    const float dx=eye.x-last_eye.x,dy=eye.y-last_eye.y,dz=eye.z-last_eye.z;
    const bool jump=dx*dx+dy*dy+dz*dz>900.f||now-last_frame>300;
    last_eye=eye; last_frame=now;
    float dt=ImGui::GetIO().DeltaTime; if(!std::isfinite(dt)) dt=1.f/60; dt=std::clamp(dt,0.f,.05f);
    const float time=float(ImGui::GetTime());
    const float u=std::clamp(g_sh/720.f,.75f,2.5f);   // sizes are tuned at 720 px height
    auto* dl=ImGui::GetBackgroundDrawList(); dl->PushClipRect(ImVec2(0,0),ImVec2(g_sw,g_sh),true);
    int drawn=0;
    for(int k=0;k<3;++k){
        field& f=fields[k];
        if(!wants(k)){ f.bits.clear(); continue; }
        if(f.bits.size()!=size_t(target_count(k))||jump) seed(f,k,eye);
        const Vector3 r=extent(k);
        const ImVec4 c=col(k);
        for(size_t i=0;i<f.bits.size();++i){
            auto& b=f.bits[i];
            if(k==K_RAIN){
                b.pos.y=wrap(b.pos.y-24.f*b.speed*dt,eye.y,r.y);
                b.pos.x=wrap(b.pos.x+1.4f*dt,eye.x,r.x);
                b.pos.z=wrap(b.pos.z,eye.z,r.z);
            }else if(k==K_SNOW){
                b.pos.y=wrap(b.pos.y-1.9f*b.speed*dt,eye.y,r.y);
                b.pos.x=wrap(b.pos.x+std::sin(time*.6f+b.phase)*.55f*dt,eye.x,r.x);
                b.pos.z=wrap(b.pos.z+std::cos(time*.5f+b.phase)*.45f*dt,eye.z,r.z);
            }else{ // soul: slow rising wisp
                b.pos.y=wrap(b.pos.y+.9f*b.speed*dt,eye.y,r.y);
                b.pos.x=wrap(b.pos.x+std::sin(time*.5f+b.phase)*.6f*dt,eye.x,r.x);
                b.pos.z=wrap(b.pos.z+std::cos(time*.4f+b.phase)*.5f*dt,eye.z,r.z);
            }
            ImVec2 s; float depth=0;
            if(!project(b.pos,view,proj,s,depth)) continue;
            if(s.x<-40||s.x>g_sw+40||s.y<-40||s.y>g_sh+40) continue;
            const float near_fade=std::clamp((depth-.35f)*1.5f,0.f,1.f),far_fade=std::clamp((15.f-depth)/7.f,0.f,1.f);
            float alpha=c.w*b.alpha*near_fade*far_fade;
            if(k==K_SOUL) alpha*=.35f+.65f*std::pow(.5f+.5f*std::sin(time*1.6f+b.phase),2.f);
            if(alpha<.01f) continue;
            auto ink=[&](float al){ return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x,c.y,c.z,std::clamp(al,0.f,1.f))); };
            if(k==K_RAIN){
                const float len=std::clamp(110.f/depth,7.f,38.f)*b.scale*u,wdt=std::clamp(2.2f-depth*.09f,.9f,2.f)*u;
                dl->AddLine(s,ImVec2(s.x-len*.08f,s.y+len),ink(alpha*.6f),wdt);
            }else if(k==K_SNOW){
                const float rad=std::clamp(22.f/depth,1.1f,5.f)*b.scale*u;
                if(rad>2.f) dl->AddCircleFilled(s,rad*2.1f,ink(alpha*.16f),12);
                dl->AddCircleFilled(s,rad,ink(alpha*.95f),rad>2.5f?12:8);
            }else{
                const float rad=std::clamp(26.f/depth,1.6f,6.f)*b.scale*u;
                dl->AddLine(s,ImVec2(s.x,s.y+rad*5.f),ink(alpha*.18f),rad*.9f);       // short trail under the rising wisp
                dl->AddCircleFilled(s,rad*3.f,ink(alpha*.12f),16);
                dl->AddCircleFilled(s,rad*1.6f,ink(alpha*.30f),14);
                dl->AddCircleFilled(s,rad,ink(alpha),12);
            }
            ++drawn;
        }
    }
    dl->PopClipRect();
    std::snprintf(g_status,sizeof g_status,"Particles: soul=%d snow=%d rain=%d | %d drawn (overlay, no depth)",
                  detail::wants(K_SOUL),detail::wants(K_SNOW),detail::wants(K_RAIN),drawn);
}
}
