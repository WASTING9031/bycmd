#pragma once
// Approximate overlay effect; not a replacement for the game's character shader/model.
#include "../game/math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace death_effect {
struct particle { Vector3 origin, velocity; float size; };
struct burst { Vector3 center; long long at; std::array<particle,100> bits; };
struct track { uint32_t id=0; int hp=0; Vector3 pos{}; long long seen=0; };
inline std::unordered_map<uint64_t,track> tracked;
inline std::vector<burst> bursts;
inline void reset(){tracked.clear();bursts.clear();}
inline bool finite(Vector3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::fabs(p.x)<100000&&std::fabs(p.y)<100000&&std::fabs(p.z)<100000;}
inline float random(uint32_t& s){s^=s<<13;s^=s>>17;s^=s<<5;return float(s&0xffffff)/float(0x1000000);}
inline void spawn(Vector3 p,long long now,uint32_t seed){
 if(!finite(p))return; if(!seed)seed=0x91e10da5;
 burst b{};b.center=p;b.at=now;
 for(auto& x:b.bits){
  float a=random(seed)*6.2831853f, radial=.15f+random(seed)*.4f;
  x.origin=Vector3(p.x+std::cos(a)*radial,p.y-.75f+random(seed)*1.65f,p.z+std::sin(a)*radial);
  float speed=.55f+random(seed)*2.2f;
  x.velocity=Vector3(std::cos(a)*speed, .3f+random(seed)*2.1f, std::sin(a)*speed);
  x.size=.65f+random(seed)*1.3f;
 }
 if(bursts.size()>=16)bursts.erase(bursts.begin());bursts.push_back(b);
}
// Require a recent successful positive-health sample, same object and network ID,
// followed by a successful zero-health sample. Missing/unreadable entities do not count.
inline void observe(uint64_t object,uint32_t id,int hp,Vector3 pos,long long now){
 if(!object||hp<0||hp>10000||now<=0)return;
 auto it=tracked.find(object);
 if(it!=tracked.end()&&it->second.id==id&&it->second.hp>0&&hp==0&&now>=it->second.seen&&now-it->second.seen<=750){
  spawn(it->second.pos,now,id^uint32_t(object)^uint32_t(now));
  diag::log("DEATH_FX","triggered: observed HP transition to zero");
 }
 if(hp>0 && finite(pos) && (pos.x!=0||pos.y!=0||pos.z!=0))tracked[object]={id,hp,pos,now};
 else if(hp==0)tracked.erase(object);
}
// Caller must independently verify that this object's HP was read as zero.
inline bool confirmed_zero(uint64_t object,uint32_t id,long long now){
 auto it=tracked.find(object);
 if(it==tracked.end()||it->second.id!=id||it->second.hp<=0||now<it->second.seen||now-it->second.seen>750)return false;
 const auto previous=it->second;tracked.erase(it);
 spawn(previous.pos,now,id^uint32_t(object)^uint32_t(now));
 diag::log("DEATH_FX","triggered: tracked enemy confirmed zero HP");return true;
}
// Only a recently tracked object with the same network ID can trigger this path.
inline bool destroyed(uint64_t object,uint32_t id,long long now){
 auto it=tracked.find(object);
 if(it==tracked.end()||it->second.id!=id||it->second.hp<=0||now<it->second.seen||now-it->second.seen>750)return false;
 const auto previous=it->second;tracked.erase(it);
 spawn(previous.pos,now,id^uint32_t(object)^uint32_t(now));return true;
}
inline void prune(long long now){
 for(auto it=tracked.begin();it!=tracked.end();)if(now<it->second.seen||now-it->second.seen>1500)it=tracked.erase(it);else ++it;
 bursts.erase(std::remove_if(bursts.begin(),bursts.end(),[&](const burst& b){return now<b.at||now-b.at>1050;}),bursts.end());
}
inline void draw(ImDrawList* dl,const matrix& view,const matrix& proj,long long now){
 prune(now); if(!dl)return;
 dl->PushClipRect({0,0},{g_sw,g_sh},true);
 for(const auto& b:bursts){
  const float t=float(now-b.at)/1000.f, fade=std::clamp((1.05f-t)/.55f,0.f,1.f);
  ImVec2 top{},bottom{};
  if(t<.30f && world_to_screen(b.center+Vector3(0,.82f,0),view,proj,top)&&world_to_screen(b.center-Vector3(0,.85f,0),view,proj,bottom)){
   const float h=std::fabs(bottom.y-top.y),x=(top.x+bottom.x)*.5f,y=top.y;
   if(std::isfinite(h)&&h>3&&h<g_sh*1.5f){
    float flash=std::clamp(1.f-t/.30f,0.f,1.f)*fade;
    // A translucent light bloom and a schematic silhouette, not the real mesh.
    dl->AddCircleFilled({x,y+h*.40f},h*.27f,IM_COL32(255,244,209,int(27*flash)),24);
    ImU32 white=IM_COL32(255,255,242,int(190*flash));
    dl->AddCircleFilled({x,y+h*.12f},h*.085f,white,14);
    dl->AddRectFilled({x-h*.11f,y+h*.22f},{x+h*.11f,y+h*.64f},white,h*.06f);
    dl->AddLine({x-h*.08f,y+h*.30f},{x-h*.23f,y+h*.57f},white,h*.07f);
    dl->AddLine({x+h*.08f,y+h*.30f},{x+h*.23f,y+h*.57f},white,h*.07f);
    dl->AddLine({x-h*.05f,y+h*.60f},{x-h*.13f,y+h*.94f},white,h*.09f);
    dl->AddLine({x+h*.05f,y+h*.60f},{x+h*.13f,y+h*.94f},white,h*.09f);
   }
  }
  for(const auto& x:b.bits){
   auto at=[&](float q){q=std::max(0.f,q);return Vector3(x.origin.x+x.velocity.x*q,x.origin.y+x.velocity.y*q-2.8f*q*q,x.origin.z+x.velocity.z*q);};
   ImVec2 a{},z{};if(!world_to_screen(at(t),view,proj,a)||!world_to_screen(at(t-.035f),view,proj,z))continue;
   const float wd=std::clamp(g_sh/720.f*x.size,.8f,3.f);
   dl->AddLine(z,a,IM_COL32(255,244,208,int(70*fade)),wd*3.f);
   dl->AddLine(z,a,IM_COL32(255,255,246,int(240*fade)),wd);
  }
 }
 dl->PopClipRect();
}
} // namespace death_effect
