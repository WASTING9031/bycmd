#pragma once
#include "../other/vector3.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace grenade_math {
inline bool finite(Vector3 p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::fabs(p.x)<100000&&std::fabs(p.y)<100000&&std::fabs(p.z)<100000;}
inline float distance(Vector3 a,Vector3 b) {const auto d=a-b;return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}
struct timer {bool valid{};float remaining{},total{};};
inline timer countdown(float start,float duration,float now) {
    if(!std::isfinite(start)||!std::isfinite(duration)||!std::isfinite(now)||start<0||duration<=0||duration>600||now<start-.05f) return {};
    return {true,std::clamp(start+duration-now,0.f,duration),duration};
}
// Free flight only: no geometry query, no speculative impact point or bounces.
inline std::vector<Vector3> predict(Vector3 p,Vector3 v,Vector3 up,float gravity,float drag,float step,float seconds) {
    std::vector<Vector3> out;
    const float norm=std::sqrt(up.x*up.x+up.y*up.y+up.z*up.z);
    if(!finite(p)||!finite(v)||!finite(up)||norm<.9f||norm>1.1f||!std::isfinite(gravity)||gravity>0||gravity< -100||
       !std::isfinite(drag)||drag<0||drag>100||!std::isfinite(step)||step<.001f||step>.1f||!std::isfinite(seconds)||seconds<=0||seconds>3) return out;
    up=up*(1.f/norm);out.push_back(p);
    const int count=std::min(3000,int(std::ceil(seconds/step)));
    for(int i=0;i<count;++i) {
        const float dt=std::min(step,seconds-i*step);if(dt<=0) break;
        v=(v+up*(gravity*dt))*std::clamp(1.f-drag*dt,0.f,1.f);
        p=p+v*dt;if(!finite(p)) break;
        if((i+1)%2==0||i==count-1)out.push_back(p);
    }
    return out;
}
// XZ envelope of actual flame positions. Not a collision/damage polygon.
inline std::vector<Vector3> hull(std::vector<Vector3> p) {
    p.erase(std::remove_if(p.begin(),p.end(),[](auto v){return !finite(v);}),p.end());
    std::sort(p.begin(),p.end(),[](auto a,auto b){return a.x==b.x?a.z<b.z:a.x<b.x;});
    p.erase(std::unique(p.begin(),p.end(),[](auto a,auto b){return a.x==b.x&&a.z==b.z;}),p.end());
    if(p.size()<3)return p;
    auto cross=[](auto a,auto b,auto c){return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);};
    std::vector<Vector3> h(p.size()*2);size_t k=0;
    for(auto v:p){while(k>=2&&cross(h[k-2],h[k-1],v)<=0)--k;h[k++]=v;}
    const size_t t=k+1;for(int i=int(p.size())-2;i>=0;--i){while(k>=t&&cross(h[k-2],h[k-1],p[i])<=0)--k;h[k++]=p[i];}
    h.resize(k-1);return h;
}
}
