#pragma once
#include <cstdint>
#include <cstddef>

// Layouts confirmed in the supplied ARM64 dump.cs / offsets.hpp.
// Reader and class matching are injected so this exact path can be tested
// without a running game. This module never writes target memory.
namespace weapon_reader {
inline constexpr uint64_t weaponry_typeinfo = 0xCD34CE0; // uyg (player part)
inline constexpr uint64_t active_typeinfo = 0xCD30D00; // jrx
inline constexpr uint64_t context = 0x30; // inline WeaponContext in uyg
inline constexpr uint64_t active_field = 0x50; // WeaponContext.ActiveWeapon
inline constexpr uint64_t raw_field = 0x10; // jrx.oqrg -> WeaponId.Raw : byte
inline bool pointer(uint64_t p) { return p>=0x10000 && p<0x1000000000000ULL && (p&7)==0; }
enum class status { ok, invalid_player, parts_unreadable, weaponry_missing,
                    active_unreadable, no_active_weapon, wrong_active_class,
                    id_unreadable, switched_during_read };
inline const char* message(status s) {
    switch(s) {
    case status::ok: return "ok";
    case status::invalid_player: return "invalid player";
    case status::parts_unreadable: return "parts unavailable";
    case status::weaponry_missing: return "uyg not found";
    case status::active_unreadable: return "ActiveWeapon unreadable";
    case status::no_active_weapon: return "no active weapon";
    case status::wrong_active_class: return "active class is not jrx";
    case status::id_unreadable: return "WeaponId unreadable";
    case status::switched_during_read: return "weapon changed during read";
    }
    return "unknown";
}
struct result { status state{status::invalid_player}; uint8_t raw{}; uint64_t part{},active{}; };
template<class Read,class Match>
uint64_t find_weaponry(uint64_t container,Read&& read,Match&& matches,bool& readable) {
    readable=false;
    uint64_t list=0,array=0; int count=0; uint64_t length=0;
    if(read(container+0x10,&list,8) && pointer(list) &&
       read(list+0x18,&count,4) && count>=0 && count<=512 &&
       read(list+0x10,&array,8) && pointer(array) &&
       read(array+0x18,&length,8) && length>=uint64_t(count) && length<=4096) {
        uint64_t parts[512]{};
        if(count==0 || read(array+0x20,parts,size_t(count)*8)) {
            readable=true;
            for(int i=0;i<count;++i) if(pointer(parts[i]) && matches(parts[i],"uyg")) return parts[i];
        }
    }
    // nzk.kevw is a separately indexed cew[]; never parse it as List<T>.
    array=0;length=0;
    if(read(container+0x20,&array,8) && pointer(array) &&
       read(array+0x18,&length,8) && length<=4096) {
        uint64_t chunk[128]{};
        for(uint64_t off=0;off<length;off+=128) {
            const size_t n=size_t(length-off<128?length-off:128);
            if(!read(array+0x20+off*8,chunk,n*8)) continue;
            readable=true;
            for(size_t i=0;i<n;++i) if(pointer(chunk[i]) && matches(chunk[i],"uyg")) return chunk[i];
        }
        if(length==0) readable=true;
    }
    return 0;
}
template<class Read,class Match>
result resolve(uint64_t player,Read&& read,Match&& matches) {
    result out;
    if(!pointer(player)) return out;
    uint64_t container=0;
    out.state=status::parts_unreadable;
    if(!read(player+0x28,&container,8) || !pointer(container)) return out;
    bool readable=false;
    out.part=find_weaponry(container,read,matches,readable);
    if(!out.part) {out.state=readable?status::weaponry_missing:status::parts_unreadable;return out;}
    out.state=status::active_unreadable;
    if(!read(out.part+context+active_field,&out.active,8)) return out;
    if(!pointer(out.active)) {out.state=status::no_active_weapon;return out;}
    if(!matches(out.active,"jrx")) {out.state=status::wrong_active_class;return out;}
    out.state=status::id_unreadable;
    if(!read(out.active+raw_field,&out.raw,1)) return out;
    // Do not display an old weapon when ActiveWeapon changes mid-read.
    uint64_t again=0;
    if(!read(out.part+context+active_field,&again,8) || again!=out.active) {
        out.raw=0;out.state=status::switched_during_read;return out;
    }
    out.state=status::ok;return out;
}

// Whole loadout of ONE player, read from that player's own uyg part.
// Statically confirmed in libunity (dump 1.0.0):
//   uyg.ivuu  0x9C8FB18: ldrb w0,[x0,#0x18]          -> current slot (qco)
//   uyg.lwgn  0x9C81184: ldr  x0,[x0,#0x80]          -> ActiveWeapon (same field as above)
//   uyg.xybl  0x9C9B308: iterates uyg+0x28 (yhn) and uyg+0x20 (vch) = every weapon the player owns
//   yhn.gewy  0x9CACDB8: List<jrx> at +0x10, _size at +0x18
//   vch.kvdg  0x9CAB8A8: rcp<jrx> at +0x10, count at +0x18
//   rcp.get_Item 0x7E45BF0: jrx[] at +0x10 (elements at +0x20, length at +0x18)
inline constexpr uint64_t slots_field = 0x20;        // uyg.eagl : vch (slot -> jrx)
inline constexpr uint64_t items_field = 0x28;        // uyg.hase : yhn (List<jrx>)
inline constexpr uint64_t current_slot_field = 0x18; // uyg.azus : qco
inline constexpr int loadout_max = 12;
struct loadout { uint8_t raw[loadout_max]{}; int count{}; uint8_t slot{}; bool readable{}; };
template<class Read>
int read_jrx_array(uint64_t holder,Read&& read,uint64_t* out,int max) {
    // holder+0x10 -> array, holder+0x18 -> int count (List<T> and rcp<T> share this shape)
    uint64_t array=0; int count=0; uint64_t length=0;
    if(!read(holder+0x10,&array,8) || !pointer(array) ||
       !read(holder+0x18,&count,4) || count<0 || count>32 ||
       !read(array+0x18,&length,8) || length<uint64_t(count) || length>256) return -1;
    if(count>max) count=max;
    if(count && !read(array+0x20,out,size_t(count)*8)) return -1;
    return count;
}
template<class Read,class Match>
loadout read_loadout(uint64_t part,Read&& read,Match&& matches) {
    loadout out;
    if(!pointer(part)) return out;
    read(part+current_slot_field,&out.slot,1);
    uint64_t items[2*loadout_max]{}; int n=0;
    for(uint64_t field : {slots_field, items_field}) {
        // uyg+field -> vch / yhn ; +0x10 -> rcp<jrx> / List<jrx> ; then array + count
        uint64_t holder=0,list=0;
        if(!read(part+field,&holder,8) || !pointer(holder) ||
           !read(holder+0x10,&list,8) || !pointer(list)) continue;
        const int got=read_jrx_array(list,read,items+n,2*loadout_max-n);
        if(got<0) continue;
        out.readable=true; n+=got;
    }
    for(int i=0;i<n && out.count<loadout_max;++i) {
        const uint64_t w=items[i];
        if(!pointer(w)) continue;
        bool seen=false;
        for(int j=0;j<i;++j) if(items[j]==w) {seen=true;break;}
        if(seen || !matches(w,"jrx")) continue;
        uint8_t raw=0;
        if(!read(w+raw_field,&raw,1) || raw==0) continue;
        out.raw[out.count++]=raw;
    }
    return out;
}
}
