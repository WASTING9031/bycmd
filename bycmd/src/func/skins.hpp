#pragma once
#include "../ui/theme/theme.hpp"
#include "../game/player.hpp"
#include "../game/weapon_visual_data.hpp"
#include "../game/skins_core.hpp"
#include "../ui/cfg.hpp"
#include <array>
#include <cstring>

namespace skins {
namespace detail {
inline int pid=-1;inline uint64_t lib=0,local=0;inline bool compatible=false;
inline long long next=0, next_log=0, next_signature=0;
inline void report(const char* message) {
    if(std::strcmp(cfg::skins::status,message))diag::log("SKINS","%s",message);
    cfg::skins::status=message;
}
inline std::array<skins_core::record,16> records{};
inline bool reset_reload_pending=false;
inline bool signature() {
    // jrx.uova copies inline metadata; chc.fiww feeds SkinId into animation.
    const uint8_t a[]={0x00,0x80,0xc1,0x3c,0x01,0x80,0xc2,0x3c,0x02,0x80,0xc3,0x3c,0x09,0x24,0x40,0xf9};
    const uint8_t b[]={0x08,0x28,0x40,0xf9,0x68,0x2e,0x00,0xb4,0x54,0x2e,0x00,0xb4,0x09,0x1d,0x40,0xb9,0x68,0x46,0x40,0xf9,0x89,0xd6,0x00,0xb9};
    uint8_t x[sizeof a]{},y[sizeof b]{};
    return mem_read(lib+0x9cac0c8,x,sizeof x)&&!std::memcmp(x,a,sizeof a)&&
           mem_read(lib+0xaa72a74,y,sizeof y)&&!std::memcmp(y,b,sizeof b);
}
inline bool presentation_signature(){
    const uint8_t s0[]={0xa0,0x0a,0x40,0xf9,0x40,0x04,0x00,0xb4,0x08,0x18,0x40,0xb9,0x1f,0x21,0x34,0x6b,0x4d,0x02,0x00,0x54};uint8_t b0[sizeof s0]{};
    const uint8_t s1[]={0xf5,0xb7,0x40,0x79,0xc0,0x0e,0x44,0x38,0xd4,0x06,0x40,0x79,0xd7,0x12,0x40,0x39};uint8_t b1[sizeof s1]{};
    const uint8_t s2[]={0xe0,0x83,0xc5,0x3c,0xe1,0x83,0xc6,0x3c,0xe2,0x83,0xc7,0x3c,0xe8,0x47,0x40,0xf9,0x60,0x86,0x02,0xad,0x69,0x56,0x40,0xb9,0xe0,0x87,0x04,0xad,0xe2,0x2f,0x80,0x3d,0xe8,0x63,0x00,0xf9,0x62,0x1e,0x80,0x3d,0x68,0x42,0x00,0xf9};uint8_t b2[sizeof s2]{};
    return mem_read(lib+0x76e4d40,b0,sizeof b0)&&!std::memcmp(b0,s0,sizeof s0)&&
        mem_read(lib+0xaa5b94c,b1,sizeof b1)&&!std::memcmp(b1,s1,sizeof s1)&&
        mem_read(lib+0xaa5cbb0,b2,sizeof b2)&&!std::memcmp(b2,s2,sizeof s2);
}
inline bool presentation_class(uint64_t object,const char* expected){
    if(!weapon_reader::pointer(object))return false;
    uint64_t klass=0,wanted=0;
    const uint64_t rva=expected[0]=='x'?0xCD36018:0xCD33AC0;
    return mem_read(object,&klass,8)&&mem_read(lib+rva,&wanted,8)&&plausible(wanted)&&klass==wanted;
}
inline bool active_class(uint64_t object) {
    uint64_t klass=0,expected=0;
    return weapon_reader::pointer(object)&&mem_read(object,&klass,8)&&
        mem_read(lib+weapon_reader::active_typeinfo,&expected,8)&&plausible(expected)&&klass==expected;
}
inline bool owned(uint64_t player_object,uint64_t part,uint64_t object) {
    if(pid!=proc::pid||lib!=proc::lib||!player::local(player_object)||player::health(player_object)<=0)return false;
    auto active=weapon_visual_data::resolve(player_object);
    if(active.state!=weapon_reader::status::ok||active.part!=part||!active_class(object))return false;
    if(active.active==object)return true;
    // uyg+0x20 -> vch+0x10 -> rcp<jrx> {array+0x10,count+0x18}.
    // Verified against vch.get_Item and rcp<jrx>.get_Item, not guessed List layout.
    uint64_t inventory=0,list=0,array=0,length=0;int count=0;
    if(!mem_read(part+0x20,&inventory,8)||!plausible(inventory)||
       !mem_read(inventory+0x10,&list,8)||!plausible(list)||
       !mem_read(list+0x10,&array,8)||!plausible(array)||
       !mem_read(list+0x18,&count,4)||count<0||count>32||
       !mem_read(array+0x18,&length,8)||length<uint64_t(count)||length>128)return false;
    uint64_t items[32]{};
    if(!mem_read(array+0x20,items,size_t(count)*8))return false;
    for(int i=0;i<count;++i)if(items[i]==object)
        return rpm<uint64_t>(part+0x20)==inventory&&rpm<uint64_t>(inventory+0x10)==list&&
               rpm<uint64_t>(list+0x10)==array&&rpm<int>(list+0x18)==count;
    return false;
}
}
inline void tick(bool running) {
    using namespace detail;
    if(!running||proc::pid<=0||!proc::lib){records={};reset_reload_pending=false;local=0;pid=-1;lib=0;report("waiting for game");return;}
    if(pid!=proc::pid||lib!=proc::lib){pid=proc::pid;lib=proc::lib;records={};reset_reload_pending=false;local=0;compatible=signature()&&presentation_signature();next=0;next_signature=0;
        diag::log("SKINS","session pid=%d module=0x%llx signatures=%s",pid,
            (unsigned long long)lib,compatible?"matched":"MISMATCH: writes blocked");}
    if(!compatible){
        const auto now=diag::now_ms();
        if(now>=next_signature){
            next_signature=now+3000;
            compatible=signature()&&presentation_signature();
            if(compatible)diag::log("SKINS","signatures matched after retry");
        }
        if(!compatible){report("signature mismatch or read failed; retrying");return;}
    }
    if(!diag::every(next,50))return;
    const bool enabled=cfg::skins::selected[0]!=0;
    bool pending=reset_reload_pending;for(const auto& r:records)pending|=r.object!=0;
    if(!enabled&&!pending){report("off");return;}
    uint64_t p=0;
    for(auto candidate:players())if(player::local(candidate)&&player::health(candidate)>0){
        if(p){report("ambiguous local player");return;}p=candidate;
    }
    if(!p){report("waiting for local player");return;}
    if(local && local!=p){records={};reset_reload_pending=false;}
    local=p;
    auto current=weapon_visual_data::resolve(p);
    if(current.state!=weapon_reader::status::ok){report("waiting for weapon");return;}
    auto read=[](uint64_t a,void* b,size_t n){return mem_read(a,b,n);};
    auto write=[](uint64_t a,const void* b,size_t n){return mem_write(a,b,n);};
    skin_presentation::context presentation{};
    const char* locate_reason=nullptr;
    if(!skin_presentation::locate(p,current.part,read,presentation_class,presentation,&locate_reason)){
        report(locate_reason?locate_reason:"skin: loader unavailable");
        if(locate_reason&&!std::strcmp(locate_reason,"skin: metadata registry unavailable")){
          static long long probe=0;if(diag::every(probe,5000)){
            uint64_t container=rpm<uint64_t>(p+0x20), presenter=0;
            for(uint64_t field=0x10;plausible(container)&&field<=0x38&&!presenter;field+=8){
              uint64_t arr=rpm<uint64_t>(container+field); if(!plausible(arr))continue;
              uint64_t n=rpm<uint64_t>(arr+0x18); if(n>512)continue;
              for(uint64_t i=0;i<n;i++){uint64_t obj=rpm<uint64_t>(arr+0x20+i*8);if(presentation_class(obj,"xzl")){presenter=obj;break;}}
            }
            const uint64_t value=plausible(presenter)?rpm<uint64_t>(presenter+0x80):0;
            const uint64_t klass=plausible(value)?rpm<uint64_t>(value):0, expected=rpm<uint64_t>(lib+0xCD33AC0);
            const std::string label=il2cpp::klass_label(klass);
            diag::log("SKIN_PROBE","presenter=0x%llx registry=0x%llx class=0x%llx expected=0x%llx name=%s",(unsigned long long)presenter,(unsigned long long)value,(unsigned long long)klass,(unsigned long long)expected,label.c_str());
          }
        }
        return;
    }
    const auto live_presentation=[&](){
        skin_presentation::context fresh{};
        return skin_presentation::locate(p,current.part,read,presentation_class,fresh)&&
            fresh.presenter==presentation.presenter&&fresh.registry==presentation.registry;
    };
    if(reset_reload_pending){
        skin_presentation::saved request{};request.reload_pending=true;
        if(!skin_presentation::invalidate(presentation,request,read,write,live_presentation)){
            report("restore model reload pending");return;
        }
        reset_reload_pending=false;
    }
    bool restore_failed=false;
    for(auto& r:records)if(r.object){
        if(!owned(p,current.part,r.object)){r={};continue;} // Never chase dropped/stale objects.
        skins_core::values now{};
        if(!skins_core::read_values(read,r.object,now)){restore_failed=true;continue;}
        if(!skins_core::identity(now,r.original)){r={};continue;}
        skins_core::values desired{};
        if(!skins_core::target(r.original.raw,cfg::skins::selected,desired)){
            auto guard=[&](){return owned(p,current.part,r.object);};
            auto presentation_guard=[&](){return guard()&&live_presentation();};
            if(!skin_presentation::restore(r.presentation,presentation,read,write,presentation_guard)){
                restore_failed=true;continue;
            }
            const bool held=r.object==current.active;
            if(!skins_core::restore(r,read,write,guard)){restore_failed=true;continue;}
            if(held){
                skin_presentation::saved request{};request.reload_pending=true;
                if(!skin_presentation::invalidate(presentation,request,read,write,live_presentation)){
                    reset_reload_pending=true;restore_failed=true;
                }
            }
        }
    }
    if(restore_failed){report("restore pending");return;}
    if(!enabled){report("off");return;}
    // Resolve selection using the saved ORIGINAL raw ID: the active object may
    // already report 72 after a FabM->Karambit write. Treating it as a real
    // knife would lose the source binding and prevent subsequent reset/switch.
    skins_core::record* slot=nullptr;
    for(auto& r:records)if(r.object==current.active){slot=&r;break;}
    skins_core::values desired{};
    const uint8_t source_raw=slot?slot->original.raw:current.raw;
    if(!skins_core::target(source_raw,cfg::skins::selected,desired)){report("hold FabM (not a knife)");return;}
    if(!slot)for(auto& r:records)if(!r.object){slot=&r;break;}
    if(!slot){report("record limit reached");return;}
    auto guard=[&](){auto fresh=weapon_visual_data::resolve(p);return fresh.state==weapon_reader::status::ok&&
        fresh.part==current.part&&fresh.active==current.active&&owned(p,current.part,current.active);};
    skins_core::values before{};
    if(!skins_core::read_values(read,current.active,before)){report("weapon metadata unreadable");return;}
    if(!before.meta){report("FabM has no metadata ID (0)");return;}
    // Validate the real registry BEFORE modifying the cached active weapon.
    skin_presentation::slot metadata{};
    if(!skin_presentation::resolve(presentation,before.meta,before.owner,read,metadata)){
        report("weapon metadata registry mismatch");return;
    }
    auto presentation_guard=[&](){return guard()&&live_presentation()&&
        rpm<uint16_t>(current.active+0x50)==before.meta&&rpm<uint8_t>(current.active+0x18)==before.owner;};
    const bool active_ok=skins_core::apply(*slot,current.active,desired,read,write,presentation_guard);
    if(!active_ok){report("active weapon write failed");return;}
    const bool metadata_ok=skin_presentation::apply(slot->presentation,presentation,before.meta,before.owner,
        desired.skin,read,write,presentation_guard);
    if(!metadata_ok){report("registry write failed / changed");return;}
    if(before.raw!=desired.raw||before.skin!=desired.skin)slot->presentation.reload_pending=true;
    const bool ok=skin_presentation::invalidate(presentation,slot->presentation,read,write,presentation_guard);
    report(ok?"model reload requested (unverified)":"model reload request pending");
    if(diag::every(next_log,2000)){
        skins_core::values check{};const bool readable=skins_core::read_values(read,current.active,check);
        diag::log("SKINS","source=%u target=%u skin=%d active=0x%llx read=%d raw=%u skinNow=%d reload=%d (NOT visual verification)",
            unsigned(source_raw),unsigned(desired.raw),desired.skin,(unsigned long long)current.active,
            readable?1:0,unsigned(check.raw),check.skin,ok?1:0);
        diag::log("SKINS","presenter=0x%llx registry=0x%llx metaId=%u registrySkin=%d wanted=%d",
            (unsigned long long)presentation.presenter,(unsigned long long)presentation.registry,
            unsigned(before.meta),rpm<int>(metadata.address+4),desired.skin);
    }
}
}
