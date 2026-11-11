#pragma once
// Confirmed ARM64 dump: chc+30 -> PlayerAnimationConfiguration+28 ->
// OffsetPoints+10 -> ValueTransform[] (24-byte structs: Position, EulerAngles).
// This modifies animation reference points, not the world camera or aim rotation.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace viewmodel_core {
struct pose {float p[3],e[3];};
static_assert(sizeof(pose)==24,"ValueTransform stride");
struct settings {float x=0,y=0,z=0,roll=0,pitch=0,yaw=0;};
inline bool pointer(uint64_t a){return a>=0x10000&&a<0x800000000000ULL&&(a&7)==0;}
inline bool finite(const pose& v){for(float f:v.p)if(!std::isfinite(f)||std::fabs(f)>10)return false;for(float f:v.e)if(!std::isfinite(f)||std::fabs(f)>3600)return false;return true;}
inline float bounded(float x,float limit){return std::isfinite(x)?std::clamp(x,-limit,limit):0.f;}
struct target {uint64_t component=0,config=0,native=0,points=0,array=0;uint64_t count=0;std::vector<pose> original;};
template<class Read> bool ref(Read& r,uint64_t a,uint64_t& p){return r(a,&p,8)&&pointer(p);}
template<class Read,class Match> bool inspect(Read& r,Match& is,uint64_t component,target& out,const char** reason=nullptr){
 auto fail=[&](const char* why){if(reason)*reason=why;return false;};
 target t;t.component=component;
 if(!is(component,"chc"))return fail("viewmodel: animation component mismatch");
 if(!ref(r,component+0x30,t.config))return fail("viewmodel: config unavailable");
 if(!is(t.config,"PlayerAnimationConfiguration"))return fail("viewmodel: config type mismatch");
 if(!ref(r,t.config+0x10,t.native))return fail("viewmodel: config native unavailable");
 if(!ref(r,t.config+0x28,t.points))return fail("viewmodel: offset points unavailable");
 if(!is(t.points,"OffsetPoints"))return fail("viewmodel: offset points type unavailable");
 if(!ref(r,t.points+0x10,t.array))return fail("viewmodel: points array unavailable");
 if(!r(t.array+0x18,&t.count,8)||!t.count||t.count>32)return fail("viewmodel: points count invalid");
 t.original.resize(size_t(t.count));if(!r(t.array+0x20,t.original.data(),t.original.size()*sizeof(pose)))return fail("viewmodel: points unreadable");
 for(const auto& v:t.original)if(!finite(v))return fail("viewmodel: invalid point values");
 out=std::move(t);if(reason)*reason="ok";return true;
}
template<class Read,class Match> bool owns(Read& r,Match& is,const target& t){
 uint64_t c=0,n=0,p=0,a=0,count=0;
 return is(t.component,"chc")&&ref(r,t.component+0x30,c)&&c==t.config&&is(c,"PlayerAnimationConfiguration")&&
 ref(r,c+0x10,n)&&n==t.native&&ref(r,c+0x28,p)&&p==t.points&&is(p,"OffsetPoints")&&
 ref(r,p+0x10,a)&&a==t.array&&r(a+0x18,&count,8)&&count==t.count;
}
struct edit {target owner;std::vector<pose> last;std::vector<bool> wrote;};
template<class Read,class Write,class Match> bool apply(Read& r,Write& w,Match& is,edit& e,settings s){
 if(!owns(r,is,e.owner))return false;
 s.x=bounded(s.x,.4f);s.y=bounded(s.y,.4f);s.z=bounded(s.z,.4f);s.roll=bounded(s.roll,30.f);s.pitch=bounded(s.pitch,30.f);s.yaw=bounded(s.yaw,30.f);
 e.last.resize(e.owner.original.size());e.wrote.resize(e.owner.original.size(),false);
 for(size_t i=0;i<e.owner.original.size();++i){
  const uint64_t address=e.owner.array+0x20+i*sizeof(pose);pose current{};
  if(!r(address,&current,sizeof current))return false;
  const pose& expected=e.wrote[i]?e.last[i]:e.owner.original[i];
  // If the game/another owner edited an animation point, stop instead of fighting it.
  if(std::memcmp(&current,&expected,sizeof current))return false;
  pose v=e.owner.original[i];v.p[0]+=s.x;v.p[1]+=s.y;v.p[2]+=s.z;v.e[0]+=s.pitch;v.e[1]+=s.yaw;v.e[2]+=s.roll;
  if(std::memcmp(&v,&current,sizeof v)&&!w(address,&v,sizeof v))return false;
  e.last[i]=v;e.wrote[i]=true;
 }
 return true;
}
template<class Read,class Write,class Match> size_t restore(Read& r,Write& w,Match& is,edit& e){
 size_t failed=0;
 if(!owns(r,is,e.owner)){e={};return 1;}
 for(size_t i=0;i<e.wrote.size();++i)if(e.wrote[i]){
  pose current{};const uint64_t address=e.owner.array+0x20+i*sizeof(pose);
  if(!r(address,&current,sizeof current)){++failed;continue;}
  if(std::memcmp(&current,&e.last[i],sizeof current)){e.wrote[i]=false;continue;}
  if(!w(address,&e.owner.original[i],sizeof current))++failed;else e.wrote[i]=false;
 }
 if(!failed)e={};return failed;
}
} // namespace viewmodel_core
