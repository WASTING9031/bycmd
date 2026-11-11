#pragma once
#include "viewmodel.hpp"
#include "scope.hpp"
#include "../game/camera_override_core.hpp"
namespace camera_overrides {
inline const char* status="off";
inline const char* zoom_status="off";
namespace detail {
inline int pid=-1;inline uint64_t lib=0,local=0,data=0,main_state=0,arms_state=0;
inline camera_override_core::scalar main_fov,arms_fov,zoom_fov;
inline camera_override_core::aspect_edit aspect;
inline float zoom_base=0;inline long long wide_since=0;
}
// No zoom: while PlayerAnimationData.WeaponCurAimState (+0xF1) is not None, the main camera state
// is held at the un-zoomed FOV learnt after >=250 ms out of scope. On unscope ownership is simply
// dropped (the field already holds the un-zoomed value the engine wants); switching the option
// off while scoped restores the engine's latest zoomed value. The FOV override already removes
// zoom, so it takes precedence. Same guarded bor+0x48 path as the FOV override (device-unverified).
inline void tick(bool run){
 struct report {~report(){
  static char prev[160]="";static long long last=0;
  if(cfg::camera_override::aspect||cfg::camera_override::fov||(cfg::viewmodel::enabled&&cfg::viewmodel::fov_override)){
   if(std::strcmp(prev,status)!=0||diag::now_ms()-last>=5000){std::snprintf(prev,sizeof prev,"%s",status);last=diag::now_ms();diag::log("CAMERA_OVERRIDE","%s",status);}
  }
 }} trace;
 using namespace detail;auto r=[](uint64_t a,void* b,size_t n){return mem_read(a,b,n);};auto w=[](uint64_t a,const void* b,size_t n){return mem_write(a,b,n);};
 const auto is=[](uint64_t a,const char* s){return viewmodel::detail::is(a,s);};
 if(pid!=proc::pid||lib!=proc::lib){pid=proc::pid;lib=proc::lib;local=data=main_state=arms_state=0;main_fov={};arms_fov={};zoom_fov={};aspect={};zoom_base=0;wide_since=0;}
 auto release=[&](){bool ok=true;
  for(auto* e:{&main_fov,&arms_fov,&zoom_fov})if(e->object){if(is(data,"PlayerAnimationData")&&rpm<uint64_t>(data+(e==&arms_fov?0x90:0x100))==e->object&&is(e->object,"bor"))ok &=camera_override_core::release(r,w,*e);else *e={};}
  if(aspect.native){bool owner=false;for(auto c:camctl::controllers)if(rpm<uint64_t>(c+0x10)==main_state&&camctl::native_of(c)==aspect.native)owner=true;
   if(owner)ok &=camera_override_core::restore_aspect(r,w,aspect);else aspect={};}return ok;};
 const bool vm=cfg::viewmodel::enabled&&cfg::viewmodel::fov_override;
 const bool nz=cfg::scope::no_zoom&&!cfg::camera_override::fov;
 const bool watch=cfg::scope::enabled||cfg::misc::scope_indicator; // read-only aim state
 auto idle=[&](const char* why){scope::g_aim_state=-1;zoom_base=0;wide_since=0;zoom_status=nz?why:"off";};
 if(!run||(!vm&&!cfg::camera_override::fov&&!cfg::camera_override::aspect&&!nz&&!watch)){status=release()?"off":"restore pending";idle("waiting for game");return;}
 // This instruction is the actual cgy.LateTick -> Camera.fieldOfView source load.
 if(rpm<uint32_t>(lib+0xA58E948)!=0xBD404900u){release();status="unsupported game build";idle("unsupported game build");return;}
 if(!local||!player::local(local)||player::health(local)<=0){if(!release()){status="restore pending";idle("restore pending");return;}local=data=main_state=arms_state=0;for(auto p:players())if(player::local(p)&&player::health(p)>0){local=p;break;}}
 if(!local){status="waiting for local player";idle("waiting for local player");return;}
 const uint64_t comp=find_player_component(local,rpm<uint64_t>(lib+0xCD2E320));
 if(!comp){if(!release()){status="restore pending";idle("restore pending");return;}data=main_state=arms_state=0;status="animation component unavailable";idle("animation component unavailable");return;}
 const uint64_t next=viewmodel::detail::is(comp,"chc")?rpm<uint64_t>(comp+0xA0):0;
 if(next!=data){if(!release()){status="restore pending";idle("restore pending");return;}data=next;main_state=arms_state=0;}
 if(!is(data,"PlayerAnimationData")||rpm<uint8_t>(data+0xA4)!=1){release();status="waiting for first-person camera state";idle("waiting for first-person camera");return;}
 {const uint8_t aim=rpm<uint8_t>(data+0xF1);scope::g_aim_state=aim<=3?int(aim):-1;}
 const uint64_t a=rpm<uint64_t>(data+0x100),b=rpm<uint64_t>(data+0x90);
 if(a!=main_state||b!=arms_state){if(!release()){status="restore pending";idle("restore pending");return;}main_state=a;arms_state=b;zoom_base=0;wide_since=0;}
 bool ok=true;const char* aspect_error=nullptr;
 for(int i=0;i<2;++i){auto& e=i?arms_fov:main_fov;const auto object=i?arms_state:main_state;const bool on=i?vm:cfg::camera_override::fov;
  if(!is(object,"bor")){if(on)ok=false;e={};if(!i)zoom_fov={};continue;}
  if(on){if(!camera_override_core::apply(r,w,e,object,i?cfg::viewmodel::fov:cfg::camera_override::fov_value)){ok=false;camera_override_core::release(r,w,e);}}else ok &=camera_override_core::release(r,w,e);
 }
 if(nz&&is(main_state,"bor")){
  const int aim=scope::g_aim_state;
  if(aim==0){
   zoom_fov={}; // out of scope: the engine owns the field again, it already holds the un-zoomed value
   const long long now=diag::now_ms();if(!wide_since)wide_since=now;float cur=0;
   if(now-wide_since>=250&&r(main_state+0x48,&cur,4)&&std::isfinite(cur)&&cur>=30&&cur<=140)zoom_base=cur;
   zoom_status=zoom_base>0?"ready (not scoped)":"learning un-zoomed FOV";
  }else if(aim>0){
   wide_since=0;
   if(zoom_base<=0)zoom_status="scoped before the un-zoomed FOV was learnt";
   else if(camera_override_core::apply(r,w,zoom_fov,main_state,zoom_base))zoom_status="holding un-zoomed FOV while scoped (device-unverified)";
   else{ok=false;camera_override_core::release(r,w,zoom_fov);zoom_status="FOV write failed";}
  }else{ok &=camera_override_core::release(r,w,zoom_fov);wide_since=0;zoom_status="unknown aim state";}
 }else{
  if(zoom_fov.object)ok &=camera_override_core::release(r,w,zoom_fov);
  wide_since=0;zoom_status=cfg::scope::no_zoom?(cfg::camera_override::fov?"covered by Override Fov":"waiting for camera state"):"off";
 }
 if(cfg::camera_override::aspect){
  // Only a real managed Camera owned by the local MainCamera state, never transform fallback.
  camctl::candidates();uint64_t native=0;
  for(auto c:camctl::controllers)if(rpm<uint64_t>(c+0x10)==main_state){native=camctl::native_of(c);break;}
  if(aspect.native&&aspect.native!=native)ok &=camera_override_core::restore_aspect(r,w,aspect);
  if(!native){ok=false;aspect_error="aspect: owned native camera not found";}
  else if(rpm<uint32_t>(lib+0x5ED6E54)!=0xB905041Fu||rpm<uint32_t>(lib+0x5EE18B4)!=0xBD017268u){ok=false;aspect_error="aspect: camera signature mismatch";}
  else if(!camera_override_core::apply_aspect(r,w,aspect,native,cfg::camera_override::aspect_value)){ok=false;aspect_error="aspect: projection unsupported or write failed";camera_override_core::restore_aspect(r,w,aspect);}
 }else ok &=camera_override_core::restore_aspect(r,w,aspect);
 const bool writing=vm||cfg::camera_override::fov||cfg::camera_override::aspect||nz;
 status=!writing?"reading aim state only":ok?"camera state / projection written (device-unverified)":aspect_error?aspect_error:"waiting / unsupported camera layout or write failure";
}
}
