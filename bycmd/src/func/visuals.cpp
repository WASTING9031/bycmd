#include "visuals.hpp"
#include "../game/camera_globals.hpp"
#include "triggerbot.hpp"
#include "esp_style.hpp"
#include "weapon_icons.hpp"
#include "skeleton.hpp"
#include "mesh_chams.hpp"
#include "combat.hpp"
#include "particles.hpp"
#include "death_effect.hpp"
#include "../game/ammo_reader.hpp"
#include "world_visuals.hpp"
#include "bomb_timer.hpp"
#include "touch_aim.hpp"
#include "esp_stability.hpp"
#include "target_hud.hpp"
#include "../game/weapon_visual_data.hpp"
#include "../game/bomb_visual_data.hpp"
#include "../game/money_visual_data.hpp"
#include "../ui/esp_font.hpp"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../ui/theme/theme.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include "imgui.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <unordered_map>

extern ImFont* espFont;

static void esp_wait(const char* stage) {
    if (cfg::misc::target_hud) {
        target_hud::status=stage;
        target_hud::waiting(ImGui::GetBackgroundDrawList(), ImVec2(g_sw,g_sh), diag::now_ms());
    }
    particles::waiting();
    mesh_chams::waiting();
    static long long last = 0;
    if (diag::every(last, 5000)) diag::log("WAIT", "ESP blocked at: %s", stage);
}

void visuals::draw() {
    if(!cfg::style::overlay_enabled){death_effect::reset();combat::reset();target_hud::reset();target_hud::status="overlay paused";particles::waiting();return;}
    static int environment_pid=-1;
    if(environment_pid!=proc::pid){environment_pid=proc::pid;death_effect::reset();combat::reset();target_hud::reset();}
    if(!cfg::misc::death_animation)death_effect::reset();
    if(!cfg::misc::target_hud){target_hud::reset();target_hud::status="off";}
    else target_hud::status="waiting for players / camera";
    bool player_esp = cfg::esp::enabled && (cfg::esp::box    || cfg::esp::name   || cfg::esp::health  ||
               cfg::esp::distance || cfg::esp::tracer || cfg::esp::offscreen || cfg::esp::armor || cfg::esp::ping ||
               cfg::esp::money || cfg::esp::weapon_text || cfg::esp::weapon_icons || cfg::esp::loadout || cfg::esp::ammo || cfg::esp::bomb);
    const bool extra_any=cfg::misc::hit_log||cfg::misc::damage_marker||cfg::misc::bullet_tracer||cfg::misc::footsteps||cfg::misc::scope_indicator||cfg::misc::target_hud||cfg::misc::death_animation;
    const bool any=player_esp||extra_any||particles::any()||world_visuals::grenades_enabled()||world_visuals::drops_enabled()||cfg::mesh_chams::enabled||cfg::world_esp::bomb_timer||cfg::legit::enabled;
    if (!any) { esp_wait("all visual options disabled"); return; }

    std::vector<uint64_t> Players = players();
    if (Players.empty()) { esp_wait("player discovery / registry scan"); return; }

    static constexpr float kHipToFoot =  0.90f;
    static constexpr float kHipToHead =  0.75f;

    struct name_cache_entry { uint32_t id{}; std::string name; long long refreshed{}; };
    static std::unordered_map<uint64_t, name_cache_entry> name_cache;
    static size_t name_cursor = 0;
    for(auto it=name_cache.begin();it!=name_cache.end();){
        if(std::find(Players.begin(),Players.end(),it->first)==Players.end()||
           rpm<uint32_t>(it->first+v100::net_object_id)!=it->second.id)it=name_cache.erase(it);else ++it;
    }
    {
        const uint64_t object = Players[name_cursor++ % Players.size()];
        const uint32_t id = rpm<uint32_t>(object + v100::net_object_id);
        name_cache_entry& entry = name_cache[object];
        const long long now = diag::now_ms();
        if (entry.id != id || now - entry.refreshed > 5000) {
            entry.id = id;
            entry.name = player_name(object);
            entry.refreshed = now;
        }
    }

    static long long object_log = 0;
    if (diag::every(object_log, 10000)) {
        for (size_t i = 0; i < Players.size(); ++i) {
            const uint64_t object = Players[i];
            const Vector3 position = player::position(object);
            std::string name_text = name_cache[object].name;
            if (name_text.empty()) name_text = "<name unavailable>";
            diag::log("PLAYER", "[%zu] object=0x%llx id=%u name=%s local=%d team=%d hp=%d pos=(%.3f %.3f %.3f)",
                      i, (unsigned long long)object, rpm<uint32_t>(object + v100::net_object_id),
                      name_text.c_str(), player::local(object) ? 1 : 0, player::team(object),
                      player::health(object), position.x, position.y, position.z);
        }
    }

    uint64_t LocalPlayer = 0;
    Vector3 LocalPosition{};
    int local_flags = 0;
    for (uint64_t p : Players) {
        if (!player::local(p)) continue;
        ++local_flags;
        const Vector3 pos = player::position(p);
        const int t = player::team(p);
        const bool live = std::isfinite(pos.x) && (pos.x != 0.f || pos.y != 0.f || pos.z != 0.f);
        if (!LocalPlayer) { LocalPlayer = p; LocalPosition = pos; }
        if (live && (t == 1 || t == 2)) { LocalPlayer = p; LocalPosition = pos; break; }
        if (live && !(player::team(LocalPlayer) == 1 || player::team(LocalPlayer) == 2)) { LocalPlayer = p; LocalPosition = pos; }
    }
    if (!LocalPlayer) { esp_wait("local player flag"); return; }
    static uint64_t combat_local=0,combat_world=0;
    const auto local_world=rpm<uint64_t>(LocalPlayer+v100::player_world);
    if(combat_local!=LocalPlayer||combat_world!=local_world){combat::reset();target_hud::reset();death_effect::reset();esp_stability::clear();combat_local=LocalPlayer;combat_world=local_world;}
    static long long local_log = 0;
    if (local_flags > 1 && diag::every(local_log, 5000))
        diag::log("INFO", "local flag on %d players, using 0x%llx", local_flags, (unsigned long long)LocalPlayer);

    const auto money_frame=(cfg::esp::money||cfg::esp::ping||cfg::misc::target_hud)
        ?money_visual_data::read_world(rpm<uint64_t>(LocalPlayer+v100::player_world)):money_visual_data::snapshot{};

    int LocalTeam = player::team(LocalPlayer);
    const bool local_team_valid = LocalTeam == 1 || LocalTeam == 2;
    if (!local_team_valid) {
        static long long team_log = 0;
        if (diag::every(team_log, 5000))
            diag::log("WARN", "local team=%d (not 1/2): team check off, all players shown", LocalTeam);
    }
    if (!std::isfinite(LocalPosition.x) ||
        (LocalPosition.x == 0.f && LocalPosition.y == 0.f && LocalPosition.z == 0.f)) {
        esp_wait("local transform hierarchy"); return;
    }

    int enemies = 0, alive = 0, positioned = 0, projected = 0;
    struct enemy_frame {
        uint64_t object{};
        Vector3  position{};
        int      health{};
        float    distance{};
        uint32_t id{};
    };
    std::vector<enemy_frame> frame_enemies;
    // A tracked enemy may leave the player list or lose its team on death.
    // Accept zero HP only with the same class, world, network ID and valid stats.
    auto observe_tracked_zero=[&](uint64_t object,uint32_t id,long long now)->bool {
        const uint64_t klass=rpm<uint64_t>(proc::lib+v100::player_typeinfo);
        uint64_t world=0,stats=0,stats_class=0;uint32_t actual_id=0;uint16_t values[3]{};
        if(!exact_class(object,klass)||!mem_read(object+v100::player_world,&world,8)||world!=local_world||
           !mem_read(object+v100::net_object_id,&actual_id,4)||actual_id!=id||
           !mem_read(object+v100::player_stats,&stats,8)||
           !mem_read(proc::lib+v100::stats_typeinfo,&stats_class,8)||!exact_class(stats,stats_class)||
           !mem_read(stats+v100::stats_hp,values,sizeof values)||values[0]!=0||values[2]==0||values[2]>10000)return false;
        return death_effect::confirmed_zero(object,id,now);
    };
    for (uint64_t Player : Players) {
        if (!Player || Player == LocalPlayer) continue;
        if(cfg::misc::death_animation) {
            auto it=death_effect::tracked.find(Player);
            if(it!=death_effect::tracked.end()) observe_tracked_zero(Player,it->second.id,diag::now_ms());
        }
        int PlayerTeam = player::team(Player);
        if (local_team_valid && (PlayerTeam == LocalTeam || (PlayerTeam != 1 && PlayerTeam != 2))) continue;
        // Only confirmed enemy team and readable stats produce a death event.
        if(cfg::misc::death_animation && local_team_valid && (PlayerTeam==1 || PlayerTeam==2) && rpm<uint64_t>(Player+v100::player_world)==local_world) {
            uint8_t destroyed=1; uint64_t stats=0,klass=0; uint32_t net_id=0; uint16_t values[3]{};
            if(mem_read(Player+0x46,&destroyed,1) && !destroyed && mem_read(Player+v100::player_stats,&stats,8) &&
               mem_read(proc::lib+v100::stats_typeinfo,&klass,8) && exact_class(stats,klass) &&
               mem_read(Player+v100::net_object_id,&net_id,4) && mem_read(stats+v100::stats_hp,values,sizeof values) && values[2]>0 && values[2]<=10000 && values[0]<=values[2]) {
                Vector3 sample{};if(values[0]>0)sample=player::position(Player);
                death_effect::observe(Player,net_id,values[0],sample,diag::now_ms());
            }
        }
        ++enemies;

        Vector3 PlayerPosition = player::position(Player);
        if (!bomb_visual_data::finite(PlayerPosition) || (PlayerPosition.x == 0.f && PlayerPosition.y == 0.f && PlayerPosition.z == 0.f)) continue;
        ++positioned;

        int Health = player::health(Player);
        if (Health <= 0) continue;
        ++alive;

        float Distance = calculate_distance(PlayerPosition, LocalPosition);
        if (!std::isfinite(Distance) || Distance > 500.f) continue;

        frame_enemies.push_back({Player, PlayerPosition, Health, Distance,rpm<uint32_t>(Player+v100::net_object_id)});
    }

    // players() excludes destroyed objects. Recheck only recently observed enemies
    // missing from the list, and require readable, unchanged class/world/network ID.
    if(cfg::misc::death_animation && local_team_valid && plausible(local_world)) {
        const long long at=diag::now_ms();
        std::vector<std::pair<uint64_t,uint32_t>> recent;
        for(const auto& entry:death_effect::tracked)
            if(at>=entry.second.seen && at-entry.second.seen<=750 &&
               std::find(Players.begin(),Players.end(),entry.first)==Players.end())
                recent.push_back({entry.first,entry.second.id});
        const uint64_t player_class=rpm<uint64_t>(proc::lib+v100::player_typeinfo);
        for(const auto& entry:recent){
            uint64_t world=0;uint32_t id=0;uint8_t removed=0;
            if(!exact_class(entry.first,player_class)||
               !mem_read(entry.first+v100::player_world,&world,8)||world!=local_world||
               !mem_read(entry.first+v100::net_object_id,&id,4)||id!=entry.second)continue;
            if(observe_tracked_zero(entry.first,id,at))continue;
            if(mem_read(entry.first+0x46,&removed,1)&&removed==1 && death_effect::destroyed(entry.first,id,at))
                diag::log("DEATH_FX","triggered: enemy object destroyed (estimated)");
        }
        static long long death_log=0;
        if(diag::every(death_log,5000))diag::log("DEATH_FX","tracked=%zu active=%zu missing=%zu",death_effect::tracked.size(),death_effect::bursts.size(),recent.size());
    }
    std::vector<camera_candidate> Cameras = camera_candidates(LocalPlayer);
    if (camctl::controllers.empty()) {
        std::vector<camera_candidate> TransformCameras = player::camera_fallback_candidates(LocalPlayer);
        for(auto& c:TransformCameras)c.view_off=c.proj_off=-2;
        Cameras.insert(Cameras.end(), TransformCameras.begin(), TransformCameras.end());
    }
    if (Cameras.empty()) { esp_wait("native / transform camera resolution"); return; }

    matrix ViewMatrix{}, ProjectionMatrix{};
    uint64_t selected_camera = 0;
    int selected_score = -1, selected_front = 0, selected_onscreen = 0;
    float selected_camera_distance = 1000000.f;
    static long long candidate_log = 0;
    const bool log_candidates = diag::every(candidate_log, 5000);

    for (const auto& Camera : Cameras) {
        int score = 0, front = 0, onscreen = 0;
        const float* view = reinterpret_cast<const float*>(&Camera.view);
        const Vector3 camera_position(
            -(view[0]*view[12] + view[1]*view[13] + view[2]*view[14]),
            -(view[4]*view[12] + view[5]*view[13] + view[6]*view[14]),
            -(view[8]*view[12] + view[9]*view[13] + view[10]*view[14]));
        const float camera_distance = calculate_distance(camera_position, LocalPosition);
        if (camera_distance < 2.f)        score += 100;
        else if (camera_distance < 10.f)  score += 80;
        else if (camera_distance < 30.f)  score += 30;
        const float* projection = reinterpret_cast<const float*>(&Camera.projection);
        if (std::fabs(projection[11]) > .5f && std::fabs(projection[15]) < .1f) score += 30;

        for (const auto& Enemy : frame_enemies) {
            Vector3 Head(Enemy.position.x, Enemy.position.y + kHipToHead, Enemy.position.z);
            ImVec2 screen_head, screen_foot;
            if (!world_to_screen(Head, Camera.view, Camera.projection, screen_head) ||
                !world_to_screen(Enemy.position, Camera.view, Camera.projection, screen_foot)) continue;
            if (!std::isfinite(screen_head.x) || !std::isfinite(screen_head.y) ||
                !std::isfinite(screen_foot.x) || !std::isfinite(screen_foot.y)) continue;
            ++front;
            ++score;
            const bool head_inside = screen_head.x >= -g_sw*.25f && screen_head.x <= g_sw*1.25f &&
                                     screen_head.y >= -g_sh*.25f && screen_head.y <= g_sh*1.25f;
            const bool foot_inside = screen_foot.x >= -g_sw*.25f && screen_foot.x <= g_sw*1.25f &&
                                     screen_foot.y >= -g_sh*.25f && screen_foot.y <= g_sh*1.25f;
            const float body_height = std::fabs(screen_foot.y - screen_head.y);
            if (head_inside && foot_inside) { ++onscreen; score += 6; }
            if (body_height >= 2.f && body_height <= g_sh*1.5f) score += 3;
        }
        if(Camera.address==g_camera&&camera_distance<10.f)score+=24;
        if (score > selected_score || (score == selected_score && Camera.address == g_camera)) {
            selected_score   = score;
            selected_front   = front;
            selected_onscreen = onscreen;
            selected_camera_distance = camera_distance;
            selected_camera  = Camera.address;
            ViewMatrix       = Camera.view;
            ProjectionMatrix = Camera.projection;
        }
        if (log_candidates)
            diag::log("CAMERA", "candidate=0x%llx score=%d front=%d onscreen=%d localDistance=%.2f proj=(%.3f %.3f)",
                      (unsigned long long)Camera.address, score, front, onscreen, camera_distance,
                      Camera.projection.m11, Camera.projection.m22);
    }

    if (!selected_camera) {
        g_camera = 0;
        esp_wait("no camera candidate selected");
        return;
    }
    if (selected_camera_distance >= 10.f) {
        if (selected_camera_distance < 30.f && selected_front == 0) {
            g_camera = 0;
            esp_wait("camera 10-30m away, no enemies in front");
            return;
        }
        if (selected_camera_distance >= 30.f && selected_onscreen == 0 && selected_front == 0) {
            g_camera = 0;
            esp_wait("camera candidates do not project players");
            return;
        }
    }
    for(const auto& camera:Cameras)if(camera.address==selected_camera){
        matrix fresh_view{},fresh_projection{};bool valid=false;
        if(camera.view_off==-2){
            for(const auto& latest:player::camera_fallback_candidates(LocalPlayer))if(latest.address==selected_camera){
                fresh_view=latest.view;fresh_projection=latest.projection;valid=true;break;
            }
        } else if(camera.view_off<0)valid=validate_camera(selected_camera,fresh_view,fresh_projection);
        else {
            if(camera.proj_off==camera.view_off+int(sizeof(matrix))){
                struct {matrix view,projection;} pair{};
                valid=mem_read(selected_camera+camera.view_off,&pair,sizeof pair);
                fresh_view=pair.view;fresh_projection=pair.projection;
            } else valid=mem_read(selected_camera+camera.view_off,&fresh_view,sizeof fresh_view)&&
                         mem_read(selected_camera+camera.proj_off,&fresh_projection,sizeof fresh_projection);
            valid=valid&&matrix_window_sane(reinterpret_cast<const float*>(&fresh_view),true)&&
                         matrix_window_sane(reinterpret_cast<const float*>(&fresh_projection),false);
        }
        if(!valid){esp_wait("selected camera changed / invalid");return;}
        ViewMatrix=fresh_view;ProjectionMatrix=fresh_projection;break;
    }
    if (g_camera != selected_camera) {
        diag::log("CAMERA", "selected=0x%llx score=%d front=%d onscreen=%d localDistance=%.2f candidates=%zu",
                  (unsigned long long)selected_camera, selected_score, selected_front,
                  selected_onscreen, selected_camera_distance, Cameras.size());
    }
    g_camera = selected_camera;
    g_view = ViewMatrix;
    g_proj = ProjectionMatrix;
    if (cfg::mesh_chams::enabled && local_team_valid) {
        mesh_chams::begin_frame(ViewMatrix,ProjectionMatrix,local_world);
        for (const auto& enemy : frame_enemies) {
            if (rpm<uint32_t>(enemy.object+v100::net_object_id)!=enemy.id ||
                player::health(enemy.object)<=0 || rpm<uint64_t>(enemy.object+v100::player_world)!=local_world) continue;
            if (!bomb_visual_data::finite(enemy.position) || enemy.distance>500.f) continue;
            mesh_chams::submit_enemy(enemy.object,enemy.position);
        }
        mesh_chams::end_frame();
    } else mesh_chams::waiting();
    if(world_visuals::grenades_enabled()) {
        const auto frame=grenade_visual_data::read_world(local_world,cfg::world_esp::prediction_seconds);
        world_visuals::grenades(ImGui::GetBackgroundDrawList(),frame,ViewMatrix,ProjectionMatrix,LocalPosition);
    } else grenade_visual_data::reset();
    if(world_visuals::drops_enabled()) {
        const auto& drops=bomb_visual_data::current(local_world);
        world_visuals::markers(ImGui::GetBackgroundDrawList(),drops,ViewMatrix,ProjectionMatrix);
        static long long drop_log=0;
        if(diag::every(drop_log,5000)) {
            int count=0;for(const auto& m:drops.markers)if(m.kind==bomb_visual_data::marker_kind::dropped)++count;
            diag::log("DROPPED","pickup objects=%d (enabled filters / projection may hide some)",count);
        }
    }
    if(cfg::world_esp::bomb_timer) {
        const auto& bomb=bomb_visual_data::current(local_world);
        if(bomb.planted) for(const auto& m:bomb.markers) {
            if(m.kind!=bomb_visual_data::marker_kind::planted) continue;
            ImVec2 at{};
            if(world_to_screen(m.position,ViewMatrix,ProjectionMatrix,at))
                bomb_timer::marker(ImGui::GetBackgroundDrawList(),at,ImVec2(g_sw,g_sh),bomb.clock);
        }
    }
    particles::draw(ViewMatrix, ProjectionMatrix);
    if(cfg::misc::death_animation)death_effect::draw(ImGui::GetBackgroundDrawList(),ViewMatrix,ProjectionMatrix,diag::now_ms());
    const long long now_ms = diag::now_ms();
    ImDrawList* const dl = ImGui::GetBackgroundDrawList();
    const ImVec2 screen(g_sw, g_sh);
    std::vector<combat::target> targets;
    struct drawn { uint64_t object; int hp; float distance; Vector3 hip; ImVec2 min, max; bool on_screen; ImVec2 fallback; bool fallback_on; };
    std::vector<drawn> draws;
    std::vector<uint64_t> alive_objects;

    for (const auto& Enemy : frame_enemies) {
        const uint64_t Player = Enemy.object;
        if (rpm<uint32_t>(Player + v100::net_object_id) != Enemy.id) continue;
        const int Health = player::health(Player);
        if (Health <= 0 || rpm<uint64_t>(Player + v100::player_world) != rpm<uint64_t>(LocalPlayer + v100::player_world)) continue;
        // One position sample for camera scoring and final projection; do not mix samples.
        const Vector3 hip = Enemy.position;
        if (!bomb_visual_data::finite(hip) || (hip.x == 0 && hip.y == 0 && hip.z == 0)) continue;
        alive_objects.push_back(Player);
        const float Distance = calculate_distance(hip, LocalPosition);
        if (!std::isfinite(Distance) || Distance > 500.f) continue;
        // The box uses the character root, not the animated skeleton extrema.
        const skeleton::pose pose = (extra_any || cfg::legit::enabled) ? skeleton::read(Player) : skeleton::pose{};
        skeleton::rect r{};
        ImVec2 head{}, foot{};
        const bool hv = world_to_screen(Vector3(hip.x, hip.y + kHipToHead + .12f, hip.z), ViewMatrix, ProjectionMatrix, head);
        const bool fv = world_to_screen(Vector3(hip.x, hip.y - kHipToFoot, hip.z), ViewMatrix, ProjectionMatrix, foot);
        if (hv && fv && std::isfinite(head.x) && std::isfinite(head.y) && std::isfinite(foot.x) && std::isfinite(foot.y)) {
            const float h=std::fabs(foot.y-head.y), cx=(head.x+foot.x)*.5f;
            if(h>=4.f && h<=g_sh*1.5f) {
                r.min={cx-h*.27f,std::min(head.y,foot.y)}; r.max={cx+h*.27f,std::max(head.y,foot.y)}; r.ok=true;
                esp_stability::apply(Player,Enemy.id,selected_camera,g_sw,g_sh,now_ms,r.min,r.max);
            }
        }
        const Vector3 aim_point = pose.ok ? pose.head : Vector3(hip.x, hip.y + kHipToHead - .12f, hip.z);
        const Vector3 foot_point = pose.ok ? pose.bottom : Vector3(hip.x, hip.y - kHipToFoot, hip.z);
        if(cfg::legit::enabled && local_team_valid && player::health(LocalPlayer)>0) {
            // Head is skeletal top when available; chest and pelvis are approximate body points.
            Vector3 point=aim_point;
            if(cfg::legit::bone==1)point=pose.ok ? pose.head*.65f+pose.bottom*.35f : Vector3(hip.x,hip.y+.32f,hip.z);
            else if(cfg::legit::bone==2)point=hip;
            ImVec2 screen{};if(world_to_screen(point,ViewMatrix,ProjectionMatrix,screen))
                touch_aim::offer(Player,screen.x,screen.y,g_sw,g_sh,cfg::legit::fov);
        }
        targets.push_back({Player, Enemy.id, aim_point, foot_point, .3f});
        combat::observe_steps(Player, foot_point, now_ms);
        const bool on = r.ok && r.max.x > -50.f && r.min.x < g_sw + 50.f && r.max.y > -50.f && r.min.y < g_sh + 50.f;
        ImVec2 fallback{};
        const bool fallback_on = world_to_screen(aim_point,ViewMatrix,ProjectionMatrix,fallback) &&
            std::isfinite(fallback.x) && std::isfinite(fallback.y) && fallback.x>=0 && fallback.y>=0 && fallback.x<=g_sw && fallback.y<=g_sh;
        draws.push_back({Player, Health, Distance, hip, r.min, r.max, on, fallback, fallback_on});
    }
    esp_stability::prune(alive_objects, now_ms);
    skeleton::forget_missing(alive_objects);
    combat::forget(alive_objects);
    combat::update_local(LocalPlayer, ViewMatrix, targets, now_ms);
    if (cfg::misc::hit_log||cfg::misc::damage_marker) for (uint64_t p : Players) {
        if (p == LocalPlayer||rpm<uint64_t>(p+v100::player_world)!=local_world) continue;
        const int t = player::team(p);
        if (local_team_valid && (t == LocalTeam || (t != 1 && t != 2))) continue;
        const auto f = name_cache.find(p);
        combat::observe_enemy(p, rpm<uint32_t>(p + v100::net_object_id), player::health(p), f != name_cache.end() ? f->second.name.c_str() : "", now_ms);
    }
    combat::draw_world(dl, ViewMatrix, ProjectionMatrix, now_ms);
    static long long weapon_log = 0;
    const bool log_weapons = player_esp && diag::every(weapon_log, 5000);
    std::string weapon_line;
    if (player_esp) for (const auto& D : draws) {
        const uint64_t Player = D.object;
        if (!D.on_screen) {
            if (cfg::esp::offscreen) {
                ImVec2 s; const bool front = world_to_screen(D.hip, ViewMatrix, ProjectionMatrix, s);
                float dx = s.x - g_sw * .5f, dy = s.y - g_sh * .5f;
                if (!front) { dx = -dx; dy = -dy; }
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > .5f && std::isfinite(len)) doffscreen(ImVec2(dx / len, dy / len), D.distance, 1.f);
            }
            continue;
        }
        ++projected;
        if (cfg::esp::tracer) dtracer(ImVec2((D.min.x + D.max.x) * .5f, D.max.y), 1.f);
        triggerbot::offer(D.min.x, D.min.y, D.max.x, D.max.y);
        esp_style::player_info info;
        info.min = D.min; info.max = D.max; info.hp = D.hp; info.max_hp = 100; info.distance = D.distance;
        const auto found = name_cache.find(Player);
        info.name = found != name_cache.end() && !found->second.name.empty() ? found->second.name.c_str() : "Enemy";
        if (cfg::esp::armor) info.armor = player::armor(Player, info.helmet);
        uint16_t balance{};
        if (cfg::esp::money && money_frame.get(Player, balance)) { info.has_money = true; info.money = balance; }
        uint32_t ping{};
        if (cfg::esp::ping && money_frame.get_ping(Player, ping)) { info.has_ping = true; info.ping = ping; }
        if (cfg::esp::weapon_text || cfg::esp::weapon_icons || cfg::esp::loadout || cfg::esp::ammo) {
            const auto weapon = weapon_visual_data::resolve(Player);
            if (weapon.state == weapon_reader::status::ok) { info.has_weapon = true; info.weapon = weapon.raw; }
            if (cfg::esp::ammo && weapon.state == weapon_reader::status::ok) {
                const auto* def = weapon_catalog::lookup(weapon.raw);
                if (def && (def->kind==weapon_catalog::icon_kind::pistol || def->kind==weapon_catalog::icon_kind::smg || def->kind==weapon_catalog::icon_kind::rifle || def->kind==weapon_catalog::icon_kind::sniper || def->kind==weapon_catalog::icon_kind::shotgun || def->kind==weapon_catalog::icon_kind::machinegun)) {
                    const auto ammo=ammo_reader::read(weapon, [](uint64_t a, void* b, size_t n){return mem_read(a,b,n);});
                    if (ammo.valid) {info.has_ammo=true;info.ammo=ammo.magazine;}
                }
            }
            weapon_reader::loadout owned{};
            if (cfg::esp::loadout && weapon.part) {
                owned = weapon_visual_data::loadout(Player, weapon.part);
                info.loadout_count = std::min(owned.count, int(sizeof info.loadout));
                for (int i = 0; i < info.loadout_count; ++i) info.loadout[i] = owned.raw[i];
            }
            if (log_weapons && weapon_line.size() < 1200) {
                char b[160];
                int k = std::snprintf(b, sizeof b, " | %.16s obj=0x%llx active=0x%llx id=%u(%s) slot=%u inv=",
                                      info.name, (unsigned long long)Player, (unsigned long long)weapon.active,
                                      unsigned(weapon.raw), weapon_reader::message(weapon.state), unsigned(owned.slot));
                for (int i = 0; i < owned.count && k > 0 && k < int(sizeof b) - 6; ++i)
                    k += std::snprintf(b + k, sizeof b - size_t(k), i ? ",%u" : "%u", unsigned(owned.raw[i]));
                if (!owned.readable && k > 0 && k < int(sizeof b) - 4) std::snprintf(b + k, sizeof b - size_t(k), "n/a");
                weapon_line += b;
            }
        }
        info.bomb = cfg::esp::bomb && bomb_visual_data::carries(Player);
        esp_style::player(dl, info, 1.f);
    }
    combat::draw_hud(dl, ProjectionMatrix, screen, now_ms);
    if (cfg::misc::target_hud) {
        static uint64_t held = 0;
        const ImVec2 c(g_sw * .5f, g_sh * .5f); const float reach = std::hypot(g_sw, g_sh);
        const drawn* best = nullptr; float best_d = reach;
        const drawn* kept = nullptr; float kept_d = reach;
        for (const auto& D : draws) {
            const float dist = target_hud::target_distance(c,D.min,D.max,D.on_screen,D.fallback,D.fallback_on);
            if (D.object == held) { kept = &D; kept_d = dist; }
            if (dist < best_d) { best_d = dist; best = &D; }
        }
        if (kept && kept_d < reach && (!best || kept_d <= best_d + 24.f)) best = kept;
        held = best ? best->object : 0;
        target_hud::status=best?"target acquired":"no enemy on screen";
        static long long hud_log=0;
        if(diag::every(hud_log,5000))diag::log("TARGET_HUD","status=%s candidates=%zu radius=%.0fpx",target_hud::status,draws.size(),reach);
        if (best) {
            target_hud::info t;
            const auto found = name_cache.find(best->object);
            t.name = found != name_cache.end() ? found->second.name : std::string();
            t.hp = best->hp; t.max_hp = 100; t.distance = best->distance;
            t.armor = player::armor(best->object, t.helmet);
            const auto weapon = weapon_visual_data::resolve(best->object);
            if (weapon.state == weapon_reader::status::ok) { t.has_weapon = true; t.weapon = weapon.raw; }
            uint16_t balance{};
            if (money_frame.get(best->object, balance)) { t.has_money = true; t.money = balance; }
            target_hud::frame(dl, screen, best->object, &t, now_ms);
        } else target_hud::frame(dl, screen, 0, nullptr, now_ms);
    }
    if (log_weapons && !weapon_line.empty()) diag::log("WEAPON", "per-player%s", weapon_line.c_str());

    static long long frame_log = 0;
    if (diag::every(frame_log, 5000)) {
        diag::log(projected ? "INFO" : "WARN",
                  "ESP frame players=%zu local=0x%llx team=%d pos=(%.2f %.2f %.2f) enemies=%d positioned=%d alive=%d projected=%d camera=0x%llx",
                  Players.size(), (unsigned long long)LocalPlayer, LocalTeam,
                  LocalPosition.x, LocalPosition.y, LocalPosition.z, enemies, positioned, alive,
                  projected, (unsigned long long)g_camera);
    }
}

void visuals::draw_text_outlined(ImDrawList* dl, ImFont* f,float size,const ImVec2& p,ImU32 c,const char* s) { esp_style::text(dl,f,size,p,c,s); }
void visuals::dtracer(const ImVec2& foot,float a) {
    ImVec2 o(g_sw*.5f,cfg::esp::tracer_origin==1?g_sh*.5f:cfg::esp::tracer_origin==2?8.f:g_sh-8.f);
    esp_style::tracer(ImGui::GetBackgroundDrawList(),o,foot,a);
}
void visuals::doffscreen(const ImVec2& dir,float d,float a) {
    esp_style::arrow(ImGui::GetBackgroundDrawList(),nullptr,dir,d,ImVec2(g_sw*.5f,g_sh*.5f),ImVec2(g_sw,g_sh),a);
}
