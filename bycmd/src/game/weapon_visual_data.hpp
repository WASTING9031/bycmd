#pragma once
#include "game.hpp"
#include "weapon_catalog.hpp"
#include "weapon_reader.hpp"

namespace weapon_visual_data {
inline weapon_reader::result resolve(uint64_t player_object) {
    if(!plausible(proc::lib)) return {};
    const uint64_t part_class=rpm<uint64_t>(proc::lib+weapon_reader::weaponry_typeinfo);
    const uint64_t active_class=rpm<uint64_t>(proc::lib+weapon_reader::active_typeinfo);
    const auto read=[](uint64_t p,void* b,size_t n) {return mem_read(p,b,n);};
    const auto match=[part_class,active_class](uint64_t p,const char* expected) {
        const uint64_t klass=rpm<uint64_t>(p);
        const uint64_t desired=expected[0]=='u'?part_class:active_class;
        if(plausible(desired) && klass==desired) return true;
        // TypeInfo slots may not have been initialized yet. Only accept the
        // exact dumped type name, not an arbitrary part containing a valid ID.
        std::string name,ns;
        return il2cpp::klass_identity(klass,name,ns) && name==expected;
    };
    auto r=weapon_reader::resolve(player_object,read,match);
    static long long next_log=0;
    if(diag::every(next_log,5000)) diag::log("WEAPON","player=0x%llx source=uyg+0x80->jrx+0x10 state=%s part=0x%llx active=0x%llx id=%u",
        (unsigned long long)player_object,weapon_reader::message(r.state),
        (unsigned long long)r.part,(unsigned long long)r.active,unsigned(r.raw));
    return r;
}
// Per-player loadout (every weapon / grenade / bomb / kit that player owns).
// Cached per player object for 250 ms: inventories change rarely, reads are syscalls.
inline weapon_reader::loadout loadout(uint64_t player_object,uint64_t part) {
    struct entry { long long at{}; uint64_t part{}; weapon_reader::loadout value{}; };
    static std::unordered_map<uint64_t,entry> cache;
    static long long pruned=0;
    const long long now=diag::now_ms();
    if(now-pruned>5000) {
        for(auto it=cache.begin();it!=cache.end();) it=now-it->second.at>5000?cache.erase(it):std::next(it);
        pruned=now;
    }
    auto& e=cache[player_object];
    if(e.part==part && e.at && now-e.at<250) return e.value;
    const uint64_t active_class=rpm<uint64_t>(proc::lib+weapon_reader::active_typeinfo);
    const auto read=[](uint64_t p,void* b,size_t n) {return mem_read(p,b,n);};
    const auto match=[active_class](uint64_t p,const char* expected) {
        const uint64_t klass=rpm<uint64_t>(p);
        if(plausible(active_class)) return klass==active_class;
        std::string name,ns;
        return il2cpp::klass_identity(klass,name,ns) && name==expected;
    };
    e.value=weapon_reader::read_loadout(part,read,match);
    e.part=part; e.at=now;
    return e.value;
}
inline bool read_id(uint64_t player_object,uint8_t& raw) {
    auto r=resolve(player_object); raw=r.raw; return r.state==weapon_reader::status::ok;
}
inline const weapon_catalog::definition* describe(uint8_t raw) {return weapon_catalog::lookup(raw);}
}
