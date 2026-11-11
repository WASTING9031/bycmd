#pragma once
#include "math.hpp"
#include "bomb_visual_data.hpp"
#include "grenade_math.hpp"
#include <unordered_map>
namespace grenade_visual_data {
using grenade_math::timer;
struct flying {
    uint64_t object{};uint32_t id{};uint8_t raw{};Vector3 position{};
    timer fuse{};bool fuse_is_limit{};float elapsed{-1.f};std::vector<Vector3> trail,path;
};
struct effect {uint8_t raw{};Vector3 position{};timer clock{};std::vector<Vector3> flames;float reference_radius{};};
struct snapshot {std::vector<flying> projectiles;std::vector<effect> effects;};
inline bool type(uint64_t o,uint64_t t) {const uint64_t cls=proc::lib?rpm<uint64_t>(proc::lib+t):0;return plausible(o)&&cls&&rpm<uint64_t>(o)==cls;}
inline bool grenade(uint8_t id) {return id==91||id==92||id==93||id==94||id==95||id==97;}
inline uint64_t part(uint64_t o,uint64_t t) {const auto cls=rpm<uint64_t>(proc::lib+t);return cls?player::find_part(o,cls):0;}
inline uint64_t component(uint64_t o,uint64_t t) {const auto cls=rpm<uint64_t>(proc::lib+t);return cls?find_player_component(o,cls):0;}
inline uint64_t parameters(uint64_t o,uint8_t raw) {
    const uint64_t expected=raw==91?0xCD235D0:raw==92?0xCD235F8:raw==93?0xCD235C8:0xCD235B8;
    for(auto t:{0xCD34FA8ULL,0xCD36010ULL,0xCD2D9B8ULL,0xCD2E6F8ULL,0xCD2F528ULL}) {
        const auto c=component(o,t),p=c?rpm<uint64_t>(c+0x58):0;
        if(type(p,expected))return p;
    }
    // Effect behaviours retain their own concrete gameplay parameter object.
    const uint64_t slot=raw==91?0xCD31BF8:raw==92?0xCD2ED30:raw==93?0xCD34A48:0xCD35018;
    const uint64_t offset=raw==92?0x58:raw==93?0x38:0x48;
    const auto c=component(o,slot),p=c?rpm<uint64_t>(c+offset):0;
    return type(p,expected)?p:0;
}
inline bool physics(uint64_t controller,Vector3& up,float& gravity,float& drag,float& step) {
    if(!type(controller,0xCD29048))return false;
    const uint64_t state=rpm<uint64_t>(controller+0x30),math=rpm<uint64_t>(controller+0x68);
    if(!type(state,0xCD1E670)||!type(math,0xCD1E650))return false;
    int limit{},hits{};if(!mem_read(state+0x20,&limit,4)||!mem_read(state+0x24,&hits,4)||limit<0||hits<0)return false;
    // BallisticsState.GetValid @0xA815A20.
    const uint64_t config=rpm<uint64_t>(state+(limit<hits?0x10:0x18));
    const bool first=type(config,0xCD20E78),curved=type(config,0xCD20848);
    if(!first&&!curved)return false;
    const uint64_t p=rpm<uint64_t>(config+0x10);
    return plausible(p)&&mem_read(p+(first?0x24:0x38),&gravity,4)&&mem_read(p+0x18,&drag,4)&&
           mem_read(controller+0x38,&up,sizeof up)&&mem_read(math+0x10,&step,4);
}
struct history {uint32_t id{};uint8_t raw{};std::vector<Vector3> points;float last{};bool seen{};};
inline std::unordered_map<uint64_t,history> histories;
inline int cache_pid=-1;inline uint64_t cache_lib=0,cache_world=0;inline float last_now=-1;
inline void reset(){histories.clear();last_now=-1;}
// Diagnostics: why an in-flight grenade is not shown (logged as GRENADE every 5 s).
struct diag_counts {int objects{},edu{},view_bad{},meta_bad{},raw_bad{},controller_bad{},pos_bad{},shown{},no_fuse{},effects{};};
inline diag_counts dc;
inline std::vector<uint64_t> logged_classes;
inline std::vector<uint64_t> objects(uint64_t world) {
    std::vector<uint64_t> out;if(!g_registry||!plausible(world))return out;
    const uint64_t store=rpm<uint64_t>(g_registry+0x30),list=rpm<uint64_t>(store+0x18),array=rpm<uint64_t>(list+0x10);
    const int count=rpm<int>(list+0x18);const uint64_t length=rpm<uint64_t>(array+0x18);
    if(!plausible(store)||!plausible(list)||!plausible(array)||count<0||count>4096||length<uint64_t(count)||length>8192)return out;
    std::vector<uint64_t> all(count);if(count&&!mem_read(array+0x20,all.data(),count*8))return out;
    if(rpm<int>(list+0x18)!=count||rpm<uint64_t>(list+0x10)!=array)return out;
    for(auto o:all){uint8_t destroyed=1;if(plausible(o)&&rpm<uint64_t>(o+0x18)==world&&mem_read(o+0x46,&destroyed,1)&&!destroyed)out.push_back(o);}
    return out;
}
inline snapshot read_world(uint64_t world,float horizon) {
    snapshot out;float now{};const bool clock=bomb_visual_data::world_time(world,now);
    if(cache_pid!=proc::pid||cache_lib!=proc::lib||cache_world!=world||!clock||now<last_now-.1f){reset();cache_pid=proc::pid;cache_lib=proc::lib;cache_world=world;}
    if(clock)last_now=now;for(auto& pair:histories)pair.second.seen=false;
    dc=diag_counts{};
    for(auto o:objects(world)) {
        ++dc.objects;
        if(type(o,0xCD2EDA8)) { // edu in-flight net object
            ++dc.edu;
            if(out.projectiles.size()>=128)continue;
            const uint64_t view=rpm<uint64_t>(o+0x58),meta=rpm<uint64_t>(o+0x68);
            if(!type(view,0xCD313C0)){++dc.view_bad;continue;}
            if(!type(meta,0xCD2F2E0)){++dc.meta_bad;continue;}
            flying f;f.object=o;f.id=rpm<uint32_t>(o+0x10);f.raw=rpm<uint8_t>(meta+0x1C);if(!grenade(f.raw)){++dc.raw_bad;continue;}
            const uint64_t controller=rpm<uint64_t>(view+0x10);
            if(!type(controller,0xCD29048)){++dc.controller_bad;continue;}
            if(!mem_read(controller+0x44,&f.position,sizeof f.position)||!grenade_math::finite(f.position)){
                // fall back to the rendered transform
                const auto tf=rpm<uint64_t>(view+0x20),native=rpm<uint64_t>(tf+0x10);Vector4 q{};
                if(!plausible(tf)||!player::decode_transform_pose(native,f.position,q)||!grenade_math::finite(f.position)){++dc.pos_bad;continue;}
            }
            // Use the rendered transform when it agrees with the current physics pose.
            // A broken/stale view must not teleport the marker across the map.
            const auto tf=rpm<uint64_t>(view+0x20),native=rpm<uint64_t>(tf+0x10);Vector3 rendered{};Vector4 rotation{};
            if(plausible(tf)&&player::decode_transform_pose(native,rendered,rotation)&&
               grenade_math::finite(rendered)&&grenade_math::distance(rendered,f.position)<3.f)f.position=rendered;
            const uint64_t p=parameters(o,f.raw);float start{},duration{};
            const bool has_start=clock&&mem_read(meta+0x18,&start,4)&&std::isfinite(start)&&start>0&&now>=start-.05f&&now-start<60.f;
            if(p&&has_start&&mem_read(p+(f.raw==91?0x84:0x74),&duration,4))f.fuse=grenade_math::countdown(start,duration,now);
            if(has_start)f.elapsed=std::max(0.f,now-start);
            if(!f.fuse.valid)++dc.no_fuse;
            ++dc.shown;
            f.fuse_is_limit=f.raw==92||f.raw==94||f.raw==95||f.raw==97;
            if(clock) {
                if(histories.size()<128||histories.count(o)) {
                    auto& h=histories[o];if(h.id!=f.id||h.raw!=f.raw||(!h.points.empty()&&grenade_math::distance(h.points.back(),f.position)>20))h={};
                    h.id=f.id;h.raw=f.raw;h.seen=true;
                    if(h.points.empty()||now-h.last>=.03f){h.points.push_back(f.position);h.last=now;}
                    if(h.points.size()>96)h.points.erase(h.points.begin(),h.points.begin()+(h.points.size()-96));
                    f.trail=h.points;
                }
            }
            Vector3 velocity{},up{};float gravity{},drag{},step{};uint8_t sleep=1;
            if(mem_read(controller+0x80,&velocity,sizeof velocity)&&mem_read(controller+0xA8,&sleep,1)&&!sleep&&
               physics(controller,up,gravity,drag,step)) {
                float seconds=std::clamp(horizon,.25f,3.f);if(f.fuse.valid)seconds=std::min(seconds,f.fuse.remaining);
                if(grenade_math::distance(velocity,{})<200)f.path=grenade_math::predict(f.position,velocity,up,gravity,drag,step,seconds);
            }
            out.projectiles.push_back(std::move(f));
        } else {
            if(out.effects.size()>=64)continue;
            const uint64_t meta=rpm<uint64_t>(o+0x58),view=rpm<uint64_t>(o+0x60);
            if(!type(meta,0xCD36078)||!type(view,0xCD30BE0)) { // jlv
                const uint64_t k=rpm<uint64_t>(o);
                if(plausible(k)&&logged_classes.size()<64&&std::find(logged_classes.begin(),logged_classes.end(),k)==logged_classes.end()){
                    logged_classes.push_back(k);
                    diag::log("GRENADE","net object class seen: %s (0x%llx)",il2cpp::klass_label(k).c_str(),(unsigned long long)(k));
                }
                continue;
            }
            ++dc.effects;
            effect e;e.raw=rpm<uint8_t>(meta+0x18);if(!grenade(e.raw))continue;
            const uint64_t tf=rpm<uint64_t>(view+0x18),native=rpm<uint64_t>(tf+0x10);Vector4 q{};
            if(!player::decode_transform_pose(native,e.position,q)||!grenade_math::finite(e.position))continue;
            float start{},life{};const bool started=clock&&mem_read(meta+0x1C,&start,4);
            const uint64_t fire=part(o,0xCD2F350);
            if(fire&&(e.raw==94||e.raw==95||e.raw==97)) {
                const uint64_t sim=component(o,0xCD35018),params=sim?rpm<uint64_t>(sim+0x48):0;
                float decay{};uint8_t fading=0;
                if(type(params,0xCD235B8)&&mem_read(params+0x64,&life,4)&&mem_read(params+0xA4,&decay,4)&&
                   std::isfinite(life)&&life>0&&life<=600&&std::isfinite(decay)&&decay>=0&&decay<=60&&mem_read(fire+0x10,&fading,1)) {
                    if(fading){float fade_start{};if(clock&&mem_read(fire+0x14,&fade_start,4)&&decay>0)e.clock=grenade_math::countdown(fade_start,decay,now);}
                    else if(started)e.clock=grenade_math::countdown(start,life+decay,now);
                }
                const uint64_t list=rpm<uint64_t>(fire+0x20),a=rpm<uint64_t>(list+0x10);
                const int count=rpm<int>(list+0x18);const uint64_t length=rpm<uint64_t>(a+0x18);
                if(plausible(list)&&plausible(a)&&count>=0&&count<=256&&length>=uint64_t(count)&&length<=512) {
                    struct flame {Vector3 pos,normal;};std::vector<flame> points(count);
                    if(count&&mem_read(a+0x20,points.data(),count*sizeof(flame)))for(auto f:points)if(grenade_math::finite(f.pos))e.flames.push_back(f.pos);
                    if(rpm<int>(list+0x18)!=count||rpm<uint64_t>(list+0x10)!=a)e.flames.clear();
                }
            } else {
                // pea removes a detonation object at ybl.glhx + GrenadeParameters.Lifetime.
                const uint64_t c=component(o,0xCD32C28);
                if(c&&started&&mem_read(c+0x40,&life,4))e.clock=grenade_math::countdown(start,life,now);
            }
            if(e.clock.valid&&e.clock.remaining<=0)continue;
            // Reference ranges only; not a wall-aware damage/visibility boundary.
            // This build uses a 4 m smoke occlusion sphere (tjq.rwlg @ A80A834).
            if(e.raw==92)e.reference_radius=4.f;
            else if(e.raw==91||e.raw==93) {
                const auto p=parameters(o,e.raw);float radius{};
                if(p&&mem_read(p+0x68,&radius,4)&&std::isfinite(radius)&&radius>0&&radius<=100)e.reference_radius=radius;
            }
            // HE/flash: marker exists only while the real detonation object exists.
            // Its clock is effect lifetime, never a continuing damage/fuse timer.
            if(e.raw==91||e.raw==92||e.raw==93||!e.flames.empty())out.effects.push_back(std::move(e));
        }
    }
    for(auto i=histories.begin();i!=histories.end();)if(!i->second.seen)i=histories.erase(i);else++i;
    static long long lg=0;
    if(diag::every(lg,5000))diag::log("GRENADE","objects=%d edu=%d view_bad=%d meta_bad=%d raw_bad=%d controller_bad=%d pos_bad=%d shown=%d no_fuse=%d effects=%d clock=%d",
        dc.objects,dc.edu,dc.view_bad,dc.meta_bad,dc.raw_bad,dc.controller_bad,dc.pos_bad,dc.shown,dc.no_fuse,dc.effects,clock?1:0);
    return out;
}
}
