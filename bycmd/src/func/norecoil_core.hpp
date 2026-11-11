#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>

// Supplied ARM64 dump: uyg+0x30 -> WeaponContext; RecoilContext +0x48;
// RecoilState +0x70. No global weapon assets or fire rate are modified.
namespace norecoil_core {
struct journal { uint64_t owner=0, klass=0; float original=0; bool active=false; };
template<class Read> bool read_value(Read&& rd,uint64_t p,float& v){return rd(p,&v,sizeof v)&&std::isfinite(v);}
template<class Read,class Same> bool owns(const journal& j,Read&& rd,Same&& same){
 uint64_t klass=0; return j.active && same(j.owner) && rd(j.owner,&klass,8) && klass==j.klass;
}
// If another game writer changed the multiplier, respect that value instead of overwriting it.
template<class Read,class Write,class Same>
bool restore(journal& j,Read&& rd,Write&& wr,Same&& same){
 if(!j.active)return true;
 if(!owns(j,rd,same))return false;
 float current=0; if(!read_value(rd,j.owner+0x78,current))return false;
 if(current==0.f && !wr(j.owner+0x78,&j.original,sizeof(float)))return false;
 j={};return true;
}
template<class Read,class Write,class Same>
bool apply(journal& j,uint64_t owner,uint64_t klass,Read&& rd,Write&& wr,Same&& same){
 if(!same(owner))return false;
 uint64_t actual=0; if(!rd(owner,&actual,8)||actual!=klass||!klass)return false;
 if(j.active && (j.owner!=owner || j.klass!=klass))return false;
 float value=0; if(!read_value(rd,owner+0x78,value)||value<0.f||value>=4.f)return false;
 if(!j.active){ if(value<=0.f)return false; j={owner,klass,value,true}; }
 else if(value>0.f) j.original=value;
 const float zero=0.f;
 if(value!=0.f && !wr(owner+0x78,&zero,sizeof zero))return false;
 // RecoilState is transient. Clear Progress, PreviousProgress, both points and
 // ApproachDeltaDist only; never overwrite LastShotTime (+0x8c in WeaponContext).
 float state[7]{};
 return wr(owner+0xA0,state,sizeof state);
}
}
