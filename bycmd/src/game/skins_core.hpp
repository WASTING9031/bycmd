#pragma once
#include <cstdint>
#include <cstddef>
#include "weapon_catalog.hpp"
#include "skin_presentation.hpp"

// Supplied ARM64 build: jrx embeds WeaponMetaData at +0x18.
// Only WeaponId.Raw and WeaponMetaData.SkinId may be written.
namespace skins_core {
constexpr int fabm_skin=240083, karambit_skin=240275;
struct values { uint8_t raw{}, owner{}; uint16_t meta{}; int skin{}; };
inline int category(uint8_t raw) {
    if(raw==63)return 0;
    auto* d=weapon_catalog::lookup(raw);
    return d && d->kind==weapon_catalog::icon_kind::knife ? 1 : -1;
}
// Experimental cross-type replacement: ONLY an original FabM is a source.
// Raw changes are gameplay data, not merely a material/preview change.
inline bool target(uint8_t original_raw,const int* selected,values& out) {
    if(original_raw!=63)return false;
    if(selected[0]==fabm_skin){out.raw=63;out.skin=fabm_skin;return true;}
    if(selected[0]==karambit_skin){out.raw=72;out.skin=karambit_skin;return true;}
    return false;
}
template<class Read> bool read_values(Read read,uint64_t object,values& v) {
    return read(object+0x10,&v.raw,1)&&read(object+0x18,&v.owner,1)&&
           read(object+0x50,&v.meta,2)&&read(object+0x1c,&v.skin,4);
}
inline bool identity(const values& a,const values& b) {return a.owner==b.owner&&a.meta==b.meta;}
struct record { uint64_t object{};values original{},applied{};bool raw_written{},skin_written{}; skin_presentation::saved presentation{}; };
// Guard must verify that the exact object is still in the live local inventory.
// Read-back confirms memory only; it is NOT proof of an on-screen model update.
template<class Read,class Write,class Guard>
bool apply(record& rec,uint64_t object,values desired,Read read,Write write,Guard guard) {
    values now{};
    if(!guard()||!read_values(read,object,now))return false;
    if(rec.object && (rec.object!=object||!identity(now,rec.original)))return false;
    if(!rec.object && now.raw!=63)return false;
    if(!rec.object){rec.object=object;rec.original=now;rec.applied=now;}
    // Never adopt an arbitrary held Karambit as a previously replaced FabM.
    if(rec.original.raw!=63 || (now.raw!=63 && now.raw!=rec.applied.raw))return false;
    if((desired.raw!=63 || desired.skin!=fabm_skin) &&
       (desired.raw!=72 || desired.skin!=karambit_skin))return false;
    if(now.skin!=desired.skin){
        // Preserve a new server-provided baseline, never an old speculative value.
        if(now.skin!=rec.applied.skin){rec.original.skin=now.skin;rec.skin_written=false;}
        if(!guard()||!write(object+0x1c,&desired.skin,4))return false;
        rec.applied.skin=desired.skin;rec.skin_written=true;
    }
    if(now.raw!=desired.raw){
        if(now.raw!=rec.applied.raw){rec.original.raw=now.raw;rec.raw_written=false;}
        if(!guard()||!write(object+0x10,&desired.raw,1))return false;
        rec.applied.raw=desired.raw;rec.raw_written=true;
    }
    values check{};
    return guard()&&read_values(read,object,check)&&identity(check,rec.original)&&
           check.raw==desired.raw&&check.skin==desired.skin;
}
template<class Read,class Write,class Guard>
bool restore(record& rec,Read read,Write write,Guard guard) {
    if(!rec.object)return true;
    values now{};
    if(!guard()||!read_values(read,rec.object,now))return false;
    if(!identity(now,rec.original)){rec={};return true;}
    if(rec.raw_written && now.raw==rec.applied.raw){
        if(!guard()||!write(rec.object+0x10,&rec.original.raw,1))return false;
        uint8_t v=0;if(!read(rec.object+0x10,&v,1)||v!=rec.original.raw)return false;
    }
    rec.raw_written=false;
    if(rec.skin_written && now.skin==rec.applied.skin){
        if(!guard()||!write(rec.object+0x1c,&rec.original.skin,4))return false;
        int v=0;if(!read(rec.object+0x1c,&v,4)||v!=rec.original.skin)return false;
    }
    rec={};return true;
}
}
