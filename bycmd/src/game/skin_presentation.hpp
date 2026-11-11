#pragma once
#include "weapon_reader.hpp"
#include <initializer_list>

// xzl.SetWeaponContent reads rra.pryx(metaId), NOT jrx's cached metadata.
// Layouts verified against rra.pryx + List<WeaponMetaData>.get_Item in the ELF.
namespace skin_presentation {
struct context {uint64_t player{},weaponry{},presenter{},registry{};};
struct slot {uint64_t registry{},list{},array{},address{};uint16_t id{};uint8_t owner{};int skin{};};
struct saved {slot at{};int before{},after{};bool changed{},reload_pending{};};
template<class Read,class Match>
bool locate(uint64_t player,uint64_t weaponry,Read read,Match match,context& out,const char** reason=nullptr){
    auto fail=[&](const char* why){if(reason)*reason=why;return false;};
    // cpa components at player+0x20 are distinct from nzk parts at +0x28.
    uint64_t container=0,presenter=0;
    if(!read(player+0x20,&container,8)||!weapon_reader::pointer(container))return fail("skin: player component container unreadable");
    for(uint64_t field=0x10;field<=0x38;field+=8){
        uint64_t array=0,length=0;
        if(!read(container+field,&array,8)||!weapon_reader::pointer(array)||
           !read(array+0x18,&length,8)||length>512)continue;
        for(uint64_t i=0;i<length;++i){
            uint64_t candidate=0;
            if(read(array+0x20+i*8,&candidate,8)&&match(candidate,"xzl")){
                uint64_t again=0;
                if(read(container+field,&again,8)&&again==array)presenter=candidate;
                break;
            }
        }
        if(presenter)break;
    }
    uint64_t owner=0,part=0,registry=0;uint8_t terminated=1;
    if(!presenter)return fail("skin: weapon presenter xzl not found in component arrays");
    if(!read(presenter+0x18,&owner,8)||owner!=player)return fail("skin: presenter owner mismatch");
    if(!read(presenter+0x58,&part,8)||part!=weaponry)return fail("skin: presenter weaponry mismatch");
    if(!read(presenter+0x28,&terminated,1)||terminated)return fail("skin: presenter stopped or unreadable");
    if(!read(presenter+0x80,&registry,8)||!match(registry,"rra"))return fail("skin: metadata registry unavailable");
    out={player,weaponry,presenter,registry};if(reason)*reason="ok";return true;
}
template<class Read>
bool resolve(const context& ctx,uint16_t meta,uint8_t owner,Read read,slot& out){
    // ID 0 is the shared None/default record. Never edit it or fabricate one.
    if(!meta)return false;
    uint64_t list=0,array=0,length=0;int count=0;
    if(!read(ctx.registry+0x10,&list,8)||!weapon_reader::pointer(list)||
       !read(list+0x10,&array,8)||!weapon_reader::pointer(array)||
       !read(list+0x18,&count,4)||count<=int(meta)||count>65536||
       !read(array+0x18,&length,8)||length<uint64_t(count)||length>131072)return false;
    const uint64_t address=array+0x20+uint64_t(meta)*0x38;
    uint8_t actual=0;int skin=0;uint64_t again=0;int count2=0;
    if(!read(address,&actual,1)||actual!=owner||!read(address+4,&skin,4)||
       !read(ctx.registry+0x10,&again,8)||again!=list||
       !read(list+0x10,&again,8)||again!=array||
       !read(list+0x18,&count2,4)||count2!=count)return false;
    out={ctx.registry,list,array,address,meta,owner,skin};return true;
}
inline bool same(const slot& a,const slot& b){return a.registry==b.registry&&a.list==b.list&&a.array==b.array&&a.address==b.address&&a.id==b.id&&a.owner==b.owner;}
template<class Read,class Write,class Guard>
bool apply(saved& state,const context& ctx,uint16_t meta,uint8_t owner,int skin,Read read,Write write,Guard guard){
    slot now{};
    if(!guard()||!resolve(ctx,meta,owner,read,now))return false;
    if(state.at.address&&!same(now,state.at))return false;
    if(!state.at.address){state.at=now;state.before=now.skin;state.after=now.skin;state.reload_pending=true;}
    if(now.skin!=skin){
        if(now.skin!=state.after)state.before=now.skin;
        slot fresh{};
        if(!guard()||!resolve(ctx,meta,owner,read,fresh)||!same(now,fresh)||fresh.skin!=now.skin)return false;
        if(!write(now.address+4,&skin,4))return false;
        state.after=skin;state.changed=true;state.reload_pending=true;
    }
    slot check{};return guard()&&resolve(ctx,meta,owner,read,check)&&same(check,state.at)&&check.skin==skin;
}
template<class Read,class Write,class Guard>
bool invalidate(const context& ctx,saved& state,Read read,Write write,Guard guard){
    if(!state.reload_pending)return true;
    uint8_t current=0;uint64_t owner=0,part=0;
    if(!guard()||!read(ctx.presenter+0x18,&owner,8)||owner!=ctx.player||
       !read(ctx.presenter+0x58,&part,8)||part!=ctx.weaponry||
       !read(ctx.presenter+0x40,&current,1))return false;
    // Invalid cached tuple forces xzl.gjhw -> SetWeaponContent naturally.
    // Never call Unity/IL2CPP methods on the external/render thread.
    if(current!=0xff){const uint8_t invalid=0xff;if(!guard()||!write(ctx.presenter+0x40,&invalid,1))return false;}
    state.reload_pending=false;return true; // Request only, not visual verification.
}
template<class Read,class Write,class Guard>
bool restore(saved& state,const context& ctx,Read read,Write write,Guard guard){
    if(!state.at.address)return true;
    slot now{};
    if(!guard()||!resolve(ctx,state.at.id,state.at.owner,read,now))return false;
    // Scene/list replacement: the old address is not a safe restore target.
    if(!same(now,state.at)){state={};return true;}
    if(state.changed&&now.skin==state.after){
        if(!guard()||!write(now.address+4,&state.before,4))return false;
        slot check{};if(!resolve(ctx,state.at.id,state.at.owner,read,check)||!same(check,state.at)||check.skin!=state.before)return false;
    }
    state={};return true;
}
}
