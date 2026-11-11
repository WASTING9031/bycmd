#pragma once
// Experimental, exact-build ARM64 patches of UI alpha calculations. These modify game code.
// They do not remove world smoke particles or the gameplay status of blindness.
#include "../game/code_patch.hpp"
#include "../ui/cfg.hpp"
#include <cstdint>
namespace anti_effects {
struct Signature {uint64_t offset;uint32_t word;};
struct State {int pid=-1;uint64_t lib=0;bool owned=false;long long retry=0;const char* status="off";};
inline State flash, smoke;
inline constexpr uint32_t zero=0x2F00E400u; // movi d0,#0 (s0 = 0)
inline constexpr uint64_t flash_site=0xA6D5B24, smoke_site=0xA6D2FCC;
inline constexpr uint32_t flash_original=0x1E280800u, smoke_original=0xAA1F03E1u;
inline constexpr Signature flash_guard[]={
 {0xA6D5B00,0xFC1E0FE8u},{0xA6D5B0C,0xF9401000u},{0xA6D5B20,0x9713F5F9u},{0xA6D5B28,0xF0FBA388u},{0xA6D5B38,0x1E210808u},
 {0x7817440,0xFC1E0FE8u},{0x7817444,0xA9014FFEu},{0x7817448,0xB40001E0u},{0x7817450,0xAA0003F3u}
};
inline constexpr Signature smoke_guard[]={
 {0xA6D2FC8,0xF9401000u},{0xA6D2FD0,0x1745111Cu},
 {0x7817440,0xFC1E0FE8u},{0x7817444,0xA9014FFEu},{0x7817448,0xB40001E0u},{0x7817450,0xAA0003F3u}
};
// Read and swap are injected to enable deterministic host testing without a game.
template<class Read,class Swap,size_t N> void update(State& s,int pid,uint64_t lib,bool on,uint64_t site,uint32_t original,const Signature (&guards)[N],Read read,Swap swap,long long now){
 if(s.pid!=pid||s.lib!=lib){s={};s.pid=pid;s.lib=lib;}
 auto restore=[&]()->bool{
  if(!s.owned)return true;uint32_t current{};
  if(!read(lib+site,current))return false;
  if(current==original){s.owned=false;return true;}
  if(current!=zero){s.owned=false;s.status="site changed externally; no restore";return false;}
  if(!swap(lib+site,zero,original))return false;s.owned=false;return true;
 };
 if(!on||pid<=0||!lib){const bool ok=restore();s.status=ok?(on?"waiting for game":"off"):"restore pending or conflict";return;}
 if(s.owned){uint32_t current{};if(!read(lib+site,current)){s.status="read failed";return;}
  if(current==zero){s.status="patch applied; device-unverified";return;}
  s.owned=false;s.status=current==original?"patch not present; retrying":"patch changed externally";s.retry=now+3000;return;
 }
 if(now<s.retry)return;
 for(auto g:guards){uint32_t w{};if(!read(lib+g.offset,w)||w!=g.word){s.status="unsupported build / guard mismatch";s.retry=now+3000;return;}}
 uint32_t w{};if(!read(lib+site,w)||w!=original){s.status="patch site changed or unreadable";s.retry=now+3000;return;}
 const bool ok=swap(lib+site,original,zero);uint32_t after{};bool readback=read(lib+site,after);
 // Preserve ownership even if the write reported failure but the patched word is present.
 if(readback&&after==zero){s.owned=true;s.status=ok?"patch applied; device-unverified":"patch observed after write error";}
 else if(!readback){s.owned=true;s.status="patch outcome uncertain; restore pending";}
 else {s.status="write / verification failed";s.retry=now+3000;}
}
inline void tick(bool run){
 const auto read=[](uint64_t a,uint32_t& out){return mem_read(a,&out,sizeof out);};
 const auto swap=[](uint64_t a,uint32_t from,uint32_t to){return code_patch::swap(a,from,to);};
 auto now=diag::now_ms();int pid=proc::pid;uint64_t lib=proc::lib;
 update(flash,pid,lib,run&&cfg::anti_effects::flash,flash_site,flash_original,flash_guard,read,swap,now);
 update(smoke,pid,lib,run&&cfg::anti_effects::smoke_screen,smoke_site,smoke_original,smoke_guard,read,swap,now);
 static long long last=0;if((cfg::anti_effects::flash||cfg::anti_effects::smoke_screen)&&diag::every(last,5000))diag::log("ANTI_FX","flash=%s | smoke-screen=%s",flash.status,smoke.status);
}
}
