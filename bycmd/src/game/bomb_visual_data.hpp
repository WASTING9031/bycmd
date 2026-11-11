#pragma once
#include "game.hpp"
#include "player.hpp"
#include "weapon_visual_data.hpp"
#include "bomb_clock.hpp"
namespace bomb_visual_data {
inline constexpr uint64_t planted_type=0xCD2FA30, state_type=0xCD2F630;
inline constexpr uint64_t drop_type=0xCD2DAD8, drop_weapon_type=0xCD33B78, drop_view_type=0xCD35D28;
enum class marker_kind { planted, dropped };
struct marker { Vector3 position{}; uint8_t raw{}; marker_kind kind{}; };
struct snapshot { std::vector<marker> markers; bool planted{}; bomb_clock::reading clock{}; };
inline bool finite(const Vector3& p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::fabs(p.x)<100000&&std::fabs(p.y)<100000&&std::fabs(p.z)<100000;}
inline bool type_is(uint64_t object,uint64_t type,const char* name) {
    if (!plausible(object)) return false;
    const uint64_t cls=rpm<uint64_t>(object),expected=rpm<uint64_t>(proc::lib+type);
    if (expected) return cls==expected;
    std::string actual,ns;
    return il2cpp::klass_identity(cls,actual,ns) && actual==name;
}
inline bool carries(uint64_t object) {
    auto w=weapon_visual_data::resolve(object);
    if(w.state==weapon_reader::status::ok && w.raw==100) return true;
    // Inventory is independent of the active gun: a stashed bomb still counts.
    // Both containers of this player's uyg are checked (vch slots + yhn list, see weapon_reader).
    if(!plausible(w.part)) return false;
    const auto owned=weapon_visual_data::loadout(object,w.part);
    for(int i=0;i<owned.count;++i) if(owned.raw[i]==100) return true;
    return false;
}
inline uint64_t component(uint64_t object,uint64_t type) {
    const uint64_t cls=rpm<uint64_t>(proc::lib+type);
    if(!cls) return 0;
    const uint64_t c=find_player_component(object,cls);
    return c?c:player::find_part(object,cls);
}
inline bool world_time(uint64_t world,float& now) {
    // xrmo(qyn) 0x8E2A118: client xiz.dupj()->aof.naxj (+0x28).
    // ggk.dupj 0x8DF1874 returns ggk.lenf (+0x20).
    if(!type_is(world,0xCD2F9B0,"ggk")) return false;
    const uint64_t clock=rpm<uint64_t>(world+0x20);
    double value{};
    if(!type_is(clock,0xCD2DA20,"aof") || !mem_read(clock+0x28,&value,sizeof value) ||
       !std::isfinite(value) || value<0. || value>1e9) return false;
    now=static_cast<float>(value);return true;
}
inline bool duration(uint64_t object,float& seconds) {
    // qik's bomb simulation and dnm's client view both retain BombParameters.
    uint64_t c=component(object,0xCD33338);
    uint64_t parameters=c?rpm<uint64_t>(c+0x38):0;
    if(!type_is(parameters,0xCD1F410,"BombParameters")) {
        c=component(object,0xCD2EA28);
        parameters=c?rpm<uint64_t>(c+0x30):0;
    }
    return type_is(parameters,0xCD1F410,"BombParameters") &&
           mem_read(parameters+0x3C,&seconds,sizeof seconds) &&
           std::isfinite(seconds) && seconds>0.f && seconds<=600.f;
}
inline snapshot read_world(uint64_t world) {
    snapshot out;
    if(!g_registry||!plausible(world)) return out;
    const uint64_t store=rpm<uint64_t>(g_registry+v100::registry_store);
    if(!plausible(store)) return out;
    const uint64_t list=rpm<uint64_t>(store+v100::store_objects);
    if(!plausible(list)) return out;
    const int count=rpm<int>(list+v100::list_count);
    const uint64_t array=rpm<uint64_t>(list+v100::list_items);
    if(!plausible(array)) return out;
    const uint64_t length=rpm<uint64_t>(array+v100::array_length);
    if(count<0||count>4096||length<uint64_t(count)||length>8192) return out;
    float now{};const bool time_valid=world_time(world,now);
    for(int i=0;i<count;++i) {
        const uint64_t object=rpm<uint64_t>(array+v100::array_data+uint64_t(i)*8);
        if(!plausible(object)||rpm<uint64_t>(object+v100::player_world)!=world) continue;
        uint8_t destroyed=1;
        if(!mem_read(object+0x46,&destroyed,1)||destroyed) continue;
        Vector3 position{};
        if(type_is(object,planted_type,"gio")) {
            const uint64_t state=rpm<uint64_t>(object+0x58);
            if(!type_is(state,state_type,"fqh")) continue;
            // qik.emxs 0xA6EFA18 stops when +0x24 or +0x25 is set.
            uint8_t terminal[2]={1,1};
            if(!mem_read(state+0x24,terminal,2)||terminal[0]||terminal[1]) continue;
            float planted_at{},seconds{};
            const bool fields_valid=mem_read(state+0x18,&planted_at,4)&&duration(object,seconds);
            const auto clock=fields_valid&&time_valid?bomb_clock::evaluate(planted_at,seconds,now):bomb_clock::reading{};
            if(clock.valid && clock.remaining<=0.f) continue;
            if(!out.planted || (!out.clock.valid&&clock.valid)) {out.planted=true;out.clock=clock;}
            if(mem_read(state+0x30,&position,sizeof position)&&finite(position)) out.markers.push_back({position,100,marker_kind::planted});
        } else if(type_is(object,drop_type,"ask")) {
            const uint64_t weapon=player::find_part(object,rpm<uint64_t>(proc::lib+drop_weapon_type));
            if(!weapon) continue;
            const uint8_t raw=rpm<uint8_t>(weapon+0x10);
            if(!weapon_catalog::lookup(raw)) continue;
            const uint64_t view=rpm<uint64_t>(object+0x58);
            if(!type_is(view,drop_view_type,"xne")) continue;
            const uint64_t tf=rpm<uint64_t>(view+0x48),native=rpm<uint64_t>(tf+v100::unity_cached_ptr);
            Vector4 rotation{};
            if(player::decode_transform_pose(native,position,rotation)&&finite(position)) out.markers.push_back({position,raw,marker_kind::dropped});
        }
    }
    return out;
}
inline const snapshot& current(uint64_t world) {
    static snapshot cached;
    static int pid=-1;static uint64_t lib=0,last_world=0;static long long refreshed=0;
    const long long now=diag::now_ms();
    if(pid!=proc::pid||lib!=proc::lib||last_world!=world) {
        cached={};pid=proc::pid;lib=proc::lib;last_world=world;refreshed=0;
    }
    if(!refreshed||now-refreshed>=100) {cached=read_world(world);refreshed=now;}
    return cached;
}
}
