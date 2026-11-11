#pragma once
#include "game.hpp"

// Read-only client snapshot path for the supplied Unity 6 ARM64 build.
// NetObject.ownerId -> uwa.iwkb -> List<psb> -> jjw.ohwc.
// uqg.zphm @0xA797EB4 confirms money is ushort at jjw+0x30.
namespace money_visual_data {
inline bool type_is(uint64_t object,uint64_t slot) {
    const uint64_t expected=proc::lib?rpm<uint64_t>(proc::lib+slot):0;
    return plausible(object)&&expected&&rpm<uint64_t>(object)==expected;
}
// ping: EXPERIMENTAL guess - property class dvm (0xCD2EBC0) uint at +0x10 in the same actor list (not verified)
struct entry { uint8_t owner{}; uint16_t money{}; bool has_money{}; bool has_ping{}; uint32_t ping{}; };
struct snapshot {
    uint64_t world{};
    std::vector<entry> entries;
    bool get(uint64_t player,uint16_t& value) const {
        uint8_t owner{};
        if(!plausible(player)||rpm<uint64_t>(player+v100::player_world)!=world||
           !mem_read(player+v100::net_object_id+4,&owner,1)) return false;
        for(const auto& e:entries) if(e.owner==owner&&e.has_money) {value=e.money;return true;}
        return false;
    }
    bool get_ping(uint64_t player,uint32_t& value) const {
        uint8_t owner{};
        if(!plausible(player)||rpm<uint64_t>(player+v100::player_world)!=world||
           !mem_read(player+v100::net_object_id+4,&owner,1)) return false;
        for(const auto& e:entries) if(e.owner==owner&&e.has_ping) {value=e.ping;return true;}
        return false;
    }
};
inline bool list(uint64_t object,uint64_t& array,int& count) {
    array=0;count=0;
    if(!plausible(object)||!mem_read(object+0x10,&array,8)||!plausible(array)||
       !mem_read(object+0x18,&count,4)||count<0||count>256) return false;
    uint64_t length{};
    return mem_read(array+0x18,&length,8)&&length>=uint64_t(count)&&length<=512;
}
inline snapshot read_world(uint64_t world) {
    snapshot out;out.world=world;
    // ggk.swwa @0x8DF7DB8 -> ylk; ylk.mnwv @0x8DEA5BC returns the last ring item.
    if(!type_is(world,0xCD2F9B0)) return out;
    const uint64_t history=rpm<uint64_t>(world+0x30);
    if(!type_is(history,0xCD362A0)) return out;
    const uint64_t ring=rpm<uint64_t>(history+0x10);
    if(!plausible(ring)) return out;
    uint32_t version{};int first{},count{};
    const uint64_t buffer=rpm<uint64_t>(ring+0x18);
    uint64_t capacity{};
    if(!plausible(buffer)||!mem_read(ring+0x10,&version,4)||
       !mem_read(ring+0x20,&first,4)||!mem_read(ring+0x28,&count,4)||
       !mem_read(buffer+0x18,&capacity,8)||capacity<1||capacity>512||
       count<1||uint64_t(count)>capacity||first<0||uint64_t(first)>=capacity) return out;
    // wuu<object>.get_Item @0x87B802C: (first+index)%capacity.
    const uint64_t index=(uint64_t(first)+uint64_t(count)-1)%capacity;
    const uint64_t frame=rpm<uint64_t>(buffer+0x20+index*8);
    if(!type_is(frame,0xCD35628)) return out;
    const uint64_t actors=rpm<uint64_t>(frame+0x28);
    uint64_t actor_array{};int actor_count{};
    if(!list(actors,actor_array,actor_count)) return out;
    std::vector<uint64_t> actor_ptrs(actor_count);
    if(actor_count&&!mem_read(actor_array+0x20,actor_ptrs.data(),actor_count*8)) return out;
    bool seen[256]{};
    for(const uint64_t actor:actor_ptrs) {
        if(!type_is(actor,0xCD34C50)) continue; // uwa
        uint8_t owner{};
        if(!mem_read(actor+0x13,&owner,1)) continue;
        // Duplicate IDs imply an inconsistent snapshot; never attach wrong balances.
        if(seen[owner]) {out.entries.clear();return out;}
        seen[owner]=true;
        const uint64_t properties=rpm<uint64_t>(actor+0x28);
        uint64_t property_array{};int property_count{};
        if(!list(properties,property_array,property_count)) continue;
        entry e{};e.owner=owner;
        const int n=property_count<64?property_count:64;
        for(int j=0;j<n;++j) {
            const uint64_t state=rpm<uint64_t>(property_array+0x20+uint64_t(j)*8);
            uint16_t money{};uint32_t ping{};
            if(!e.has_money&&type_is(state,0xCD30B70)&&mem_read(state+0x30,&money,2)) {e.money=money;e.has_money=true;}
            else if(!e.has_ping&&type_is(state,0xCD2EBC0)&&mem_read(state+0x10,&ping,4)&&ping<5000) {e.ping=ping;e.has_ping=true;}
            if(e.has_money&&e.has_ping)break;
        }
        if(e.has_money||e.has_ping)out.entries.push_back(e);
        if(rpm<uint8_t>(actor+0x13)!=owner||rpm<uint64_t>(actor+0x28)!=properties) {
            out.entries.clear();return out;
        }
    }
    // Pool reuse or a concurrent network update invalidates this whole read.
    if(rpm<uint32_t>(ring+0x10)!=version||rpm<uint64_t>(ring+0x18)!=buffer||
       rpm<uint64_t>(frame+0x28)!=actors||rpm<uint64_t>(actors+0x10)!=actor_array||
       rpm<int>(actors+0x18)!=actor_count||rpm<uint64_t>(world+0x30)!=history)
        out.entries.clear();
    return out;
}
}
