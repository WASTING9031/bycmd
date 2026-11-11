#pragma once
#include "cfg.hpp"
#include <array>
#include <cstddef>
namespace overlay_control {
// Feed each retained journal its normal disabled path, preserving menu choices.
// World Mode needs no flag: its preset guard is inactive while the overlay is paused.
struct pause_guard {
 bool paused;std::array<bool*,10> flags;std::array<bool,10> values{};int skins[2]{};
 pause_guard():paused(!cfg::style::overlay_enabled),flags{{&cfg::fog::enabled,&cfg::world::sky,&cfg::viewmodel::enabled,&cfg::camera_override::fov,&cfg::camera_override::aspect,&cfg::scope::enabled,&cfg::scope::no_zoom,&cfg::anti_effects::flash,&cfg::anti_effects::smoke_screen,&cfg::rage::norecoil}} {
  if(!paused)return;for(size_t i=0;i<flags.size();++i){values[i]=*flags[i];*flags[i]=false;}
  for(int i=0;i<2;++i){skins[i]=cfg::skins::selected[i];cfg::skins::selected[i]=0;}
 }
 ~pause_guard(){if(!paused)return;for(size_t i=0;i<flags.size();++i)*flags[i]=values[i];for(int i=0;i<2;++i)cfg::skins::selected[i]=skins[i];}
 pause_guard(const pause_guard&)=delete;pause_guard& operator=(const pause_guard&)=delete;
};
}
