#pragma once
// Existing RenderSettings / skybox material properties only. Mesh skies may ignore this.
// Only existing skybox colour properties are written; ambient lighting is untouched.
#include <cstdio>
#include "../game/game.hpp"
#include "../game/material_core.hpp"
#include "../game/mesh_core.hpp"
#include "../ui/cfg.hpp"
namespace sky {
inline const char* g_status="off";inline char g_buf[240];
namespace detail {
inline constexpr uint64_t rs_slot=0xCFB67B8,rs_skybox=0x180;
inline material_core::journal j;inline int pid=-1;inline uint64_t lib=0,rs=0;inline int32_t sky_id=0;
inline constexpr int property_count=7;
inline int ids[property_count]={-2,-2,-2,-2,-2,-2,-2};
inline const char* names[property_count]={"_Tint","_SkyTint","_Color","_GroundColor","_SkyColor","_HorizonColor","_ZenithColor"};
inline long long next=0,started=0;inline unsigned mask=0;inline uint64_t owner_mat=0;
inline auto reader(){return [](uint64_t a,void* b,size_t n){return mem_read(a,b,n);};}
inline auto writer(){return [](uint64_t a,const void* b,size_t n){return mem_write(a,b,n);};}
inline void restore(const char* why){
 if(j.entries.empty())return;auto r=reader();auto w=writer();size_t failed=0,skipped=0;
 if(pid!=proc::pid||lib!=proc::lib){j.entries.clear();return;}
 std::vector<uint64_t> allowed;
 if(owner_mat&&mesh_core::resolve_id(r,lib,sky_id)==owner_mat&&rpm<int32_t>(owner_mat+material_core::object_instance_id)==sky_id){
  for(int i=0;i<property_count;++i)if(ids[i]>=0){const uint64_t a=material_core::property_address(r,owner_mat,2,ids[i]);if(a)allowed.push_back(a);}
 }
 for(auto it=j.entries.begin();it!=j.entries.end();)if(std::find(allowed.begin(),allowed.end(),it->addr)==allowed.end()){++skipped;it=j.entries.erase(it);}else ++it;
 failed=j.restore(r,w);diag::log("SKY","restored (%s) failed=%zu stale-owner-skipped=%zu",why,failed,skipped);
}

}
inline bool enabled(){return cfg::world::sky;}
inline void tick(bool run){
 using namespace detail;
 if(!run||!enabled()||proc::pid<=0||!proc::lib){restore(run?"disabled":"game not running");g_status=enabled()?"waiting for game":"off";next=0;return;}
 if(pid!=proc::pid||lib!=proc::lib){j.entries.clear();for(int& i:ids)i=-2;pid=proc::pid;lib=proc::lib;rs=0;next=0;started=0;}
 const long long now=diag::now_ms();if(now<next)return;next=now+125;
 // Guard the analysed binary before using native RenderSettings offsets.
 if(rpm<uint32_t>(lib+0x88126D8)!=0xBD001400u){restore("build mismatch");g_status="Sky: game build differs, no writes";return;}
 auto r=reader();auto w=writer();const uint64_t cur=rpm<uint64_t>(lib+rs_slot);
 if(!material_core::ptr_ok(cur)){g_status="RenderSettings not ready";return;}
 const int32_t sid=rpm<int32_t>(cur+rs_skybox);const unsigned current_mask=unsigned(cfg::world::sky);
 if(cur!=rs||sid!=sky_id||current_mask!=mask){
  restore("scene/options changed");if(sid!=sky_id)for(int& i:ids)i=-2;rs=cur;sky_id=sid;mask=current_mask;started=now;
 }
 if(!started)started=now;
 const ImVec4 c=cfg::world::sky_col;
 uint64_t mat=sid?mesh_core::resolve_id(r,lib,sid):0;
 if(mat&&rpm<int32_t>(mat+material_core::object_instance_id)!=sid)mat=0;
 owner_mat=mat;
 if(j.entries.size()>128)restore("property sheet changed");
 int props=0;
 if(mat){
  for(int i=0;i<property_count;++i)if(ids[i]==-2)ids[i]=material_core::property_id(r,lib,names[i]);
  struct color4{float v[4];};
  for(int i=0;i<property_count;++i){
   const uint64_t a=material_core::property_address(r,mat,2,ids[i]);if(!a)continue;color4 value{};
   value={{c.x,c.y,c.z,1.f}};
   if(j.put(r,w,a,value))++props;
  }
 }

 std::snprintf(g_buf,sizeof g_buf,"skybox=%s color properties=%d (device-unverified)",mat?"found":sid?"unresolved":"none/mesh sky",props);g_status=g_buf;
 static long long last=0;if(diag::every(last,5000))diag::log("SKY","%s",g_status);
}
}
