#pragma once
// Small pixel deadband and size filtering only. Movement remains responsive, teleports reset.
#include "imgui.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace esp_stability {
struct state { uint32_t id{}; uint64_t camera{}; float sw{},sh{},cx{},cy{},height{}; long long time{}; };
inline std::unordered_map<uint64_t,state> cache;
inline void clear(){cache.clear();}
inline void prune(const std::vector<uint64_t>& active,long long now){
    for(auto it=cache.begin();it!=cache.end();) {
        if(now-it->second.time>500 || std::find(active.begin(),active.end(),it->first)==active.end()) it=cache.erase(it); else ++it;
    }
}
inline void apply(uint64_t object,uint32_t id,uint64_t camera,float sw,float sh,long long now,ImVec2& a,ImVec2& b){
    const float cx=(a.x+b.x)*.5f, cy=(a.y+b.y)*.5f, h=b.y-a.y;
    if(!object||!std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(h)||h<=0)return;
    auto& s=cache[object];
    const float jump=std::hypot(cx-s.cx,cy-s.cy);
    if(s.id!=id||s.camera!=camera||s.sw!=sw||s.sh!=sh||!s.time||now<=s.time||now-s.time>200||jump>std::max(80.f,h*.75f)||h>s.height*1.5f||h<s.height*.67f) {
        s={id,camera,sw,sh,cx,cy,h,now};
    } else {
        const float dt=std::clamp(float(now-s.time)/1000.f,0.f,.1f);
        const float k=1.f-std::exp(-24.f*dt);
        // Filter dimensions, not large camera/player movement.
        s.height+=(h-s.height)*k;
        if(std::fabs(cx-s.cx)>.65f)s.cx=cx;
        if(std::fabs(cy-s.cy)>.65f)s.cy=cy;
        s.time=now;
    }
    const float half=s.height*.5f, width=s.height*.27f;
    a={std::round(s.cx-width),std::round(s.cy-half)};
    b={std::round(s.cx+width),std::round(s.cy+half)};
}
}
