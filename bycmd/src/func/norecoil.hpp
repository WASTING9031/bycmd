#pragma once
#include "../game/game.hpp"
#include "../game/player.hpp"
#include "../game/weapon_visual_data.hpp"
#include "../ui/cfg.hpp"
#include "norecoil_core.hpp"
#include <cstring>
namespace norecoil {
inline const char* status="off";
namespace detail {inline int pid=-1; inline uint64_t lib=0,local=0; inline norecoil_core::journal j;}
inline void tick(bool run) {
 using namespace detail;
 struct log_status {~log_status(){static char last[128]="";static long long at=0;
  if(cfg::rage::norecoil&&(std::strcmp(last,status)||diag::now_ms()-at>=5000)){
   std::snprintf(last,sizeof last,"%s",status);at=diag::now_ms();diag::log("NORECOIL","%s",status);
  }}} log;
 auto rd=[](uint64_t a,void* b,size_t n){return mem_read(a,b,n);};
 auto wr=[](uint64_t a,const void* b,size_t n){return mem_write(a,b,n);};
 if(pid!=proc::pid||lib!=proc::lib){pid=proc::pid;lib=proc::lib;local=0;j={};}
 auto same=[&](uint64_t component){
  if(!run || !plausible(local) || !player::local(local))return false;
  auto r=weapon_visual_data::resolve(local);
  return r.state==weapon_reader::status::ok && r.part==component;
 };
 auto release=[&](){return norecoil_core::restore(j,rd,wr,same);};
 if(!run||!cfg::rage::norecoil){bool ok=release();status=ok?"off":"restore pending / owner unavailable";if(ok)local=0;return;}
 // A few executable words identify the exact profile, not a guarantee of compatibility.
 if(!plausible(lib)||rpm<uint32_t>(lib+0x9C82FE0)!=0x9100c000u||rpm<uint32_t>(lib+0x9C82FE4)!=0xd65f03c0u||rpm<uint32_t>(lib+0x9CA513C)!=0xf9402a68u||rpm<uint32_t>(lib+0x9CA514C)!=0x9101c260u){
  bool ok=release();status=ok?"unsupported build":"restore pending / unsupported build";return;
 }
 if(!local||!player::local(local)||player::health(local)<=0){
  if(!release()){status="restore pending / local player unavailable";return;}local=0;
  for(uint64_t p:players())if(player::local(p)&&player::health(p)>0){local=p;break;}
 }
 if(!local){status="waiting for local player";return;}
 auto w=weapon_visual_data::resolve(local);
 if(w.state!=weapon_reader::status::ok){status="waiting for active weapon";return;}
 const auto* def=weapon_catalog::lookup(w.raw);
 if(!def){status="unknown weapon";return;}
 using K=weapon_catalog::icon_kind;
 if(def->kind!=K::pistol&&def->kind!=K::smg&&def->kind!=K::rifle&&def->kind!=K::sniper&&def->kind!=K::shotgun&&def->kind!=K::machinegun){
  bool ok=release();status=ok?"waiting for firearm":"restore pending";return;
 }
 if(j.active && j.owner!=w.part && !release()){status="restore pending / weaponry changed";return;}
 const uint64_t klass=rpm<uint64_t>(w.part);
 if(!norecoil_core::apply(j,w.part,klass,rd,wr,same)){status="read/write failed or owner changed";return;}
 status="writing local recoil state (experimental)";
}
}
