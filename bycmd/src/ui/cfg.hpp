#pragma once
#include "imgui.h"

namespace cfg {

namespace esp {
    inline bool enabled = true;
    inline bool box         = false;
    inline bool name        = false;
    inline bool health      = true;
    inline bool armor       = true;   // "HK" / "K" flag
    inline bool distance    = false;
    inline bool weapon_text = true;
    inline bool weapon_icons = false;
    inline bool loadout = false;       // row of icons: every other weapon / grenade / bomb / kit the player owns
    inline bool ammo = true;          // experimental magazine number + fixed accent line
    inline bool bomb = true;
    inline bool money = true;         // pixel font, configurable colour
    inline bool ping = false;         // EXPERIMENTAL field (dvm +0x10), pixel font
    inline bool pixel_font = true;    // legacy: flags font is always the pixel font now
    inline ImVec4 money_col = ImVec4(1.f,1.f,1.f,1.f);
    inline ImVec4 ping_col = ImVec4(1.f,1.f,1.f,1.f);
    inline ImVec4 loadout_col = ImVec4(1.f,1.f,1.f,1.f);
    inline ImVec4 bomb_col = ImVec4(1.f,.32f,.32f,1.f);
    inline float weapon_icon_size = 34.f; // width in overlay pixels
    inline ImVec4 weapon_text_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 weapon_icon_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline int  box_type    = 0;      // 0 full, 1 corner
    inline float box_rounding = 0.f;
    inline float box_thickness = 1.f;
    inline float corner_length = 0.25f;
    inline bool box_fill = false;
    inline float fill_opacity = 0.08f;
    inline bool outline = true;
    inline bool label_background = false;
    inline float font_size = 16.f;
    inline int name_font = 0;         // index into esp_fonts_ui::names
    inline int weapon_font = 0;
    inline bool health_gradient = false;
    inline bool health_numbers = true;
    inline float bar_width = 5.f;     // HP / armor bar thickness in px at 1080p (scaled to the screen)
    inline ImVec4 box_col      = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 name_col     = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 health_col   = ImVec4(.55f, .25f, .88f, 1.f);   // purple reference
    inline ImVec4 armor_col    = ImVec4(.55f, .25f, .88f, 1.f); // ammo accent and Target HUD armor
    inline ImVec4 distance_col = ImVec4(1.f, 1.f, 1.f, 1.f);

    inline bool  tracer           = false;
    inline int   tracer_origin    = 0;    // 0 bottom, 1 center, 2 top
    inline float tracer_thickness = 1.f;
    inline ImVec4 tracer_col      = ImVec4(1.f, 1.f, 1.f, 0.6f);

    inline bool  offscreen        = false;
    inline float offscreen_radius = 90.f;
    inline float offscreen_size   = 22.f;  // up to 48
    inline ImVec4 offscreen_col   = ImVec4(1.f, .35f, .35f, 1.f);
    inline bool  offscreen_dist   = true;
    inline bool offscreen_silhouette = true;
}

// GLES3 overlay reconstruction of the enemy's SkinnedMeshRenderer; no game writes.
namespace mesh_chams {
    inline bool enabled=false;
    inline int effect=0;
    inline ImVec4 color={.78f,.25f,1.f,1.f}, rim_color={.25f,.85f,1.f,1.f};
    inline float opacity=1.f, rim_power=2.5f;
    inline bool outline=false;
    inline float outline_px=2.f;
    inline ImVec4 outline_color={.25f,.85f,1.f,.9f};
}

namespace world_esp {
    // Grenade visuals restored from the supplied headers.
    inline bool grenade_tracking=true, grenade_times=true, grenade_effects=true;
    inline bool grenade_prediction=false, grenade_trail=false;
    inline bool grenade_name=true, grenade_background=true, grenade_distance=false;
    inline bool effect_times=false; // Active smoke and fire areas have no timers.
    inline float prediction_seconds=1.5f, grenade_scale=1.f;
    // Pickups on the ground: independent filters and display styles.
    inline bool bomb_planted=false, bomb_dropped=false, weapon_dropped=false;
    inline bool bomb_timer=false, bomb_marker=true;
    inline ImVec4 bomb_timer_color=ImVec4(1.f,.36f,.36f,1.f);
    inline bool dropped_smoke=false, dropped_molotov=false, dropped_flash=false, dropped_he=false;
    inline bool dropped_icons=true, dropped_names=true;
    inline bool bomb_distance=false, weapon_distance=false;
}

// Touch aim uses external uinput and never writes game memory.
namespace rage { inline bool norecoil = false; }

// Silent aim: writes weapon aim transform toward best target's head, restores next frame.
namespace silent {
    inline bool  enabled  = false;
    inline float fov      = 150.f;    // screen-space radius in px
    inline int   bone     = 0;        // 0 head, 1 chest, 2 pelvis
    inline float max_dist = 120.f;    // world-space metres
}

// Triggerbot: fires when crosshair overlaps an enemy bbox.
namespace trigger {
    inline bool  enabled      = false;
    inline long long cooldown_ms = 350; // minimum ms between taps
    inline long long jitter_ms  = 60;   // random pre-fire delay up to N ms
}

namespace legit {
    inline bool enabled=false, draw_fov=false, edit_position=false;
    inline float fov=120.f, smooth=.2f, gain=.12f;
    inline ImVec4 fov_color=ImVec4(.8f,.65f,.95f,1.f);
    inline float point_x=.7f, point_y=.65f; // normalized to the screen
    inline bool visible_check=false; // no visibility implementation; ON blocks, OFF can aim through walls
    inline int bone=0;
}

namespace anti_effects { inline bool flash=false, smoke_screen=false; }

namespace viewmodel {
    inline bool enabled=false;
    inline bool position=true, angles=true, fov_override=false;
    inline float x=0.f,y=0.f,z=0.f,roll=0.f,pitch=0.f,yaw=0.f,fov=60.f;
}

namespace camera_override {
    inline bool fov=false, aspect=false;
    inline float fov_value=75.f, aspect_value=1.77778f;
}
namespace fog {
    inline bool enabled = false;
    inline int mode = 0;
    inline float start = 5.f;
    inline float end = 60.f;
    inline float density = 0.03f;
    inline ImVec4 color = ImVec4(.62f, .55f, .75f, 1.f);
}

// Static tint of the existing native skybox. No added sky overlay.
namespace world {
    inline bool sky = false;
    inline ImVec4 sky_col = ImVec4(.45f,.30f,.85f,1.f);
}

namespace misc {
    inline bool hit_log = false, damage_marker = false;
    inline bool death_animation = false;
    inline bool  nightmode         = false;
    inline float nightmode_opacity  = 0.35f;
    inline ImVec4 nightmode_color = ImVec4(.025f,.025f,.045f,1.f);
    inline float nightmode_edges = 0.10f;
    inline float damage_time=.8f;
    inline ImVec4 damage_color={1.f,.65f,.8f,1.f};
    inline bool bullet_tracer = false;
    inline ImVec4 bullet_col = ImVec4(1.f, .45f, .25f, .9f);
    inline float bullet_time = 1.5f;
    inline bool footsteps = false;
    inline ImVec4 footstep_col = ImVec4(1.f, .85f, .30f, .9f);
    inline bool scope_indicator = false;
    inline bool target_hud = false;      // info panel for the enemy nearest the crosshair
    inline ImVec4 target_hud_col = ImVec4(197.f/255.f,143.f/255.f,200.f/255.f,1.f); // accent strip
    inline int target_hud_pos = 0;       // 0 Crosshair, 1 Left, 2 Top
}

// Overlay-only world particles around the camera, projected by the ESP matrices.
// No game depth, collision or audio. Three independent effects, each its own switch.
namespace particles {
    inline bool soul = false;            // slow rising wisps
    inline bool snow = false;            // falling snow
    inline bool rain = false;            // falling rain streaks
    inline ImVec4 soul_col = ImVec4(.72f,.55f,1.f,1.f);
    inline ImVec4 snow_col = ImVec4(.95f,.97f,1.f,1.f);
    inline ImVec4 rain_col = ImVec4(.62f,.74f,.95f,1.f);
}

// World mode: one look at a time. 0 Off, 1 Winter, 2 Winter Realistic, 3 Glass.
// Drives the real Fog / Sky writers with a preset only where the user's own Fog / Sky
// switch is off (menu values are never changed), plus an overlay grade and, for the
// winter looks, overlay snow.
namespace world_mode {
    inline int mode = 0;
    inline float strength = .65f;        // preset fog amount + overlay grade amount
}

// Custom scope: overlay reticle while scoped; the game's sniper sight is hidden while on.
// "Scoped" = the local PlayerAnimationData aim state. No zoom holds the un-zoomed FOV
// through the same guarded camera path as the FOV override (device-unverified).
namespace scope {
    inline bool enabled = false;
    inline int  style = 0;               // 0 Lines, 1 Cross, 2 Circle, 3 Dot
    inline float size = 60.f;            // Cross arm / Circle radius, px
    inline float thickness = 1.5f;
    inline ImVec4 color = ImVec4(.06f,.06f,.07f,1.f);
    inline bool no_zoom = false;
}

namespace style {
    inline bool overlay_enabled=true;
    inline bool capture_overlay = true;
    inline bool capture_applied = true;
    inline const char* capture_error = nullptr;
    inline bool  liquid_glass  = false;
    inline float glass_opacity = 0.62f;
    inline bool  stats_bar     = true;
    inline bool  preview_window = false;
}

namespace skins {
    inline int selected[2] = {0, 0};
    inline const char* status = "off";
}

}
