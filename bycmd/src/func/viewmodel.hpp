#pragma once
#include "../game/game.hpp"
#include "../game/player.hpp"
#include "../game/viewmodel_core.hpp"
#include "../ui/cfg.hpp"
namespace viewmodel {
inline const char* status="off";
namespace detail {
inline int pid=-1;inline uint64_t lib=0,local=0,component=0;
inline long long next=0;inline viewmodel_core::edit patch;
inline bool is(uint64_t object,const char* expected){
 if(!plausible(object))return false;
 // Prefer exact build TypeInfo identity over metadata string decoding. The latter may
 // be unavailable/obfuscated even while the objects themselves are valid.
 uint64_t slot=0;
 if(!std::strcmp(expected,"chc"))slot=0xCD2E320;
 else if(!std::strcmp(expected,"PlayerAnimationConfiguration"))slot=0xCD28480;
 else if(!std::strcmp(expected,"PlayerAnimationData"))slot=0xCD28488;
 else if(!std::strcmp(expected,"bor"))slot=0xCD2DEF0;
 const uint64_t actual=rpm<uint64_t>(object);
 if(slot){const uint64_t known=rpm<uint64_t>(proc::lib+slot);if(plausible(known))return actual==known;}
 std::string name,ns;return il2cpp::klass_identity(actual,name,ns)&&name==expected;
}
}
inline void tick(bool run){
 struct report { ~report(){
  static char previous[160]="";static long long last=0;
  if(cfg::viewmodel::enabled && (std::strcmp(previous,status)!=0 || diag::now_ms()-last>=5000)){
   std::snprintf(previous,sizeof previous,"%s",status);last=diag::now_ms();diag::log("VIEWMODEL","%s",status);
  }
 }} trace;
 using namespace detail;
 auto read=[](uint64_t a,void* b,size_t n){return mem_read(a,b,n);};
 auto write=[](uint64_t a,const void* b,size_t n){return mem_write(a,b,n);};
 auto match=[](uint64_t a,const char* n){return detail::is(a,n);};
 // Never restore into a different process, even if virtual addresses match.
 if(pid!=proc::pid||lib!=proc::lib){pid=proc::pid;lib=proc::lib;patch={};local=component=0;next=0;}
 auto release=[&](){if(!patch.owner.original.empty())return viewmodel_core::restore(read,write,match,patch);return size_t(0);};
 if(!run||!cfg::viewmodel::enabled){const auto failures=release();status=failures?"restore pending / owner changed":"off";if(!failures)local=component=0;return;}
 const auto now=diag::now_ms();if(now<next)return;next=now+100;
 // Getter instructions identify the supplied build; no fallback guessed offsets.
 if(rpm<uint32_t>(lib+0xAA7D714)!=0xF9401400u||rpm<uint32_t>(lib+0xAA7D718)!=0xD65F03C0u){const auto failures=release();status=failures?"restore pending / owner changed":"unsupported game build";return;}
 if(!local||!player::local(local)||player::health(local)<=0){
  if(release()){status="restore pending / waiting for old owner";return;}local=component=0;
  for(auto p:players())if(player::local(p)&&player::health(p)>0){local=p;break;}
 }
 if(!local){status="waiting for local player";return;}
 const uint64_t klass=rpm<uint64_t>(lib+0xCD2E320); // chc_TypeInfo, script.json
 const uint64_t current=find_player_component(local,klass);
 if(current!=component){if(release()){status="restore pending / waiting for old owner";return;}component=current;}
 if(!match(component,"chc")){const auto failures=release();status=failures?"restore pending / owner changed":"animation component unavailable";return;}
 // First-person state comes from the actual PlayerAnimationData; do not guess from camera distance.
 const uint64_t data=rpm<uint64_t>(component+0xA0);uint8_t first=0;
 if(!match(data,"PlayerAnimationData")||!mem_read(data+0xA4,&first,1)||first!=1){const auto failures=release();status=failures?"restore pending / owner changed":"waiting for first person";return;}
 if(!patch.owner.original.empty()&&!viewmodel_core::owns(read,match,patch.owner)){const auto failures=release();status=failures?"restore pending / owner changed":"animation owner changed";return;}
 if(patch.owner.original.empty()){
  viewmodel_core::target t;
  const char* reason=nullptr;
  if(!viewmodel_core::inspect(read,match,component,t,&reason)){
   status=reason?reason:"unsupported animation layout";
   if(reason && !std::strcmp(reason,"viewmodel: config type mismatch")){
    static long long probe=0;if(diag::every(probe,5000)){
     const uint64_t value=rpm<uint64_t>(component+0x30); const uint64_t klass=plausible(value)?rpm<uint64_t>(value):0;
     const uint64_t expected=rpm<uint64_t>(lib+0xCD28480);
     const std::string label=il2cpp::klass_label(klass);
     diag::log("VM_PROBE","config=0x%llx class=0x%llx expected=0x%llx name=%s",(unsigned long long)value,(unsigned long long)klass,(unsigned long long)expected,label.c_str());
    }
   }
   return;
  }
  patch.owner=std::move(t);
 }
 const viewmodel_core::settings s{cfg::viewmodel::position?cfg::viewmodel::x:0.f,cfg::viewmodel::position?cfg::viewmodel::y:0.f,cfg::viewmodel::position?cfg::viewmodel::z:0.f,
 cfg::viewmodel::angles?cfg::viewmodel::roll:0.f,cfg::viewmodel::angles?cfg::viewmodel::pitch:0.f,cfg::viewmodel::angles?cfg::viewmodel::yaw:0.f};
 if(!viewmodel_core::apply(read,write,match,patch,s)){const auto failures=release();status=failures?"restore pending after write failure":"write failed / animation changed";return;}
 status="offsets written (device-unverified)";
}
} // namespace viewmodel
