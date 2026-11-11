#pragma once
#include "weapon_reader.hpp"
namespace ammo_reader {
// dump.cs jrx: xsal is byte at 0x52; asme is ushort at 0x54.
// Interpreting these as magazine/reserve is inferred from WeaponSnapshot;
// confirm by firing and reloading on the target build before relying on it.
struct result { bool valid{}; unsigned magazine{},reserve{}; };
template<class Read> result read(const weapon_reader::result& weapon,Read&& rpm) {
    result out; uint8_t magazine{}; uint16_t reserve{}; uint64_t active{};
    if(weapon.state!=weapon_reader::status::ok) return out;
    if(!rpm(weapon.active+0x52,&magazine,sizeof magazine) ||
       !rpm(weapon.active+0x54,&reserve,sizeof reserve) ||
       !rpm(weapon.part+weapon_reader::context+weapon_reader::active_field,&active,8) ||
       active!=weapon.active) return out;
    out={true,unsigned(magazine),unsigned(reserve)}; return out;
}
}
