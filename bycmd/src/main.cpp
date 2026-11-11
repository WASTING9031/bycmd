#include "Android_draw/draw.h"
#include "ui/theme/theme.hpp"
#include "game/camera_globals.hpp"
#include "ui/menu.hpp"
#include "ui/bar.hpp"
#include "ui/overlay_control.hpp"
#include "other/memory.hpp"
#include "game/game.hpp"
#include "func/mesh_chams.hpp"
#include "func/visuals.hpp"
#include "func/fog.hpp"
#include "func/sky.hpp"
#include "func/skins.hpp"
#include "func/viewmodel.hpp"
#include "func/camera_overrides.hpp"
#include "func/scope.hpp"
#include "func/world_mode.hpp"
#include "func/nightmode.hpp"
#include "func/silent_aim.hpp"
#include "func/triggerbot.hpp"
#include "func/legit_overlay.hpp"
#include "func/touch_aim.hpp"
#include "func/target_hud.hpp"
#include "func/anti_effects.hpp"
#include "func/norecoil.hpp"
#include "protect/oxorany.hpp"
#include <cstdio>
#include <thread>
#include <chrono>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

static void print_status(const char* status) {
    diag::log("STATUS", "%s", status);
}

// Функция для запуска Standoff
static void launch_standoff() {
    int rc = system(oxorany("monkey -p com.axlebolt.standoff2 -c android.intent.category.LAUNCHER 1"));
    diag::log(rc == 0 ? "INFO" : "WARN", "game launch command exit=%d package=com.axlebolt.standoff2", rc);
}

int main() {
    const int stdout_flags = ::fcntl(STDOUT_FILENO, F_GETFL, 0);
    if (stdout_flags >= 0) ::fcntl(STDOUT_FILENO, F_SETFL, stdout_flags | O_NONBLOCK);
    const int instance_lock = ::open("/data/local/tmp/bycmd-1.0.0.lock", O_CREAT | O_RDWR, 0600);
    if (instance_lock < 0 || flock(instance_lock, LOCK_EX | LOCK_NB) != 0) {
        diag::log("ERROR", "another bycmd instance (probably the OLD build) is still running; kill it first: pkill -f bycmd");
        if (instance_lock >= 0) ::close(instance_lock);
        return 2;
    }
    diag::log("INFO", "killed Standoff2 external-zenin-diagnostics-2026-11 starting");
    screen_config();

    int max_size = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    int min_size = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);

    g_sw = static_cast<float>(max_size);
    g_sh = static_cast<float>(min_size);

    native_window_screen_x = max_size;
    native_window_screen_y = max_size;

    diag::log("INFO", "display width=%d height=%d orientation=%d overlay=%dx%d",
              displayInfo.width, displayInfo.height, displayInfo.orientation,
              native_window_screen_x, native_window_screen_y);
    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true)) {
        diag::log("ERROR", "initGUI_draw failed");
        return -1;
    }
    diag::log("INFO", "GUI initialized");

    touch::init(max_size, min_size, (uint8_t)displayInfo.orientation);

    // Автоматический запуск Standoff
    print_status(oxorany("Game detect "));
    
    print_status(oxorany("start cheat...."));
    
    launch_standoff();
    
    game::init();
    ui::bar::g_open=true; // Open the menu directly; no intro or loading layer.

    static float alpha = 0.f;
    static bool prev = false;
    static bool game_started = false;

    while (true) {
        drawBegin();
        touch_aim::begin_frame();
        triggerbot::begin_frame();
        bool run = game::valid();
        ui::bar::g_stats_pid = run ? proc::pid : 0;
        { overlay_control::pause_guard pause_features;
        world_mode::preset_guard world_preset; // inactive while paused; never changes menu values
        fog::tick(run);
        sky::tick(run);
        skins::tick(run);
        viewmodel::tick(run);
        camera_overrides::tick(run);
        scope::tick(run);
        anti_effects::tick(run);
        norecoil::tick(run);
        } // cfg restored for menu; journals used their normal disable/restore path.

#if defined(__x86_64__)
        bool is_landscape = (displayInfo.orientation == 0 || displayInfo.orientation == 2);
#else
        bool is_landscape = (displayInfo.orientation == 1 || displayInfo.orientation == 3);
#endif

        if (run && !prev) {
            if (!game_started) {
                print_status(oxorany("Game detect "));
                
                print_status(oxorany("start cheat...."));
                game_started = true;
            } else {
                print_status(oxorany("Game detect "));
            }
            prev = true;
        } else if (!run && prev) {
            print_status(oxorany("game closed"));
            prev = false;
            game_started = false;
        }

        if (is_landscape) {
            ImGuiIO& io = ImGui::GetIO();
            float dt = io.DeltaTime;
            if (dt <= 0.f || dt > 0.1f) dt = 0.016f;

            float target = 1.f; // Menu / resume button must remain usable while paused or waiting.
            float spd = run ? 4.f : 6.f;

            if (alpha < target) {
                alpha += dt * spd;
                if (alpha > target) alpha = target;
            } else if (alpha > target) {
                alpha -= dt * spd;
                if (alpha < target) alpha = target;
            }

            ui::bar::set_game_alpha(alpha);

            if (alpha > 0.001f) {
                ui::menu::render();
            }

            legit_overlay::render();

            if (run && proc::lib != 0) {
                // Library health is independent from asynchronous registry discovery.
                game::check_lib(proc::lib);
                world_mode::draw();   // grade under everything; also sets the World Mode snow
                nightmode::draw();     // independent of player/camera discovery; behind ESP
                visuals::draw();      // particles, ESP, indicators, Target HUD
                silent_aim::tick(true, g_view, g_proj);
                scope::draw();        // custom reticle on top while scoped
            } else {
                if (cfg::misc::target_hud) {
                    target_hud::status="waiting for game / library";
                    target_hud::waiting(ImGui::GetBackgroundDrawList(), ImVec2(g_sw,g_sh), diag::now_ms());
                }
                world_mode::idle();
                particles::waiting();
            }
        }

        touch_aim::finish(run && proc::lib!=0 && is_landscape,ui::bar::g_open || ui::menu::g_exit_requested,int(displayInfo.orientation));
        triggerbot::tick(run && proc::lib!=0 && is_landscape, ui::bar::g_open || ui::menu::g_exit_requested, int(displayInfo.orientation));
        bool vis = ui::bar::g_open;
        drawEnd();
        if (ui::menu::g_exit_requested) break;
        usleep(vis ? 1500 : 4000);
    }

    // ── Exit: undo everything written into the game before quitting ─────────
    if (ui::menu::g_exit_requested) {
        print_status(oxorany("exit: restoring game state..."));
        cfg::fog::enabled = false;
        cfg::world::sky = false;
        cfg::skins::selected[0] = cfg::skins::selected[1] = 0;
        cfg::viewmodel::enabled = false;
        cfg::camera_override::fov=cfg::camera_override::aspect=false;
        cfg::scope::enabled = cfg::scope::no_zoom = false;
        cfg::anti_effects::flash = cfg::anti_effects::smoke_screen = false;
        cfg::world_mode::mode = 0;
        cfg::rage::norecoil = false;
        cfg::silent::enabled = false;
        cfg::trigger::enabled = false;
        const bool run = game::valid();
        for (int i = 0; i < 60; ++i) {
            fog::tick(run);
            sky::tick(run);
            skins::tick(run);
            viewmodel::tick(run);
            camera_overrides::tick(run);
            scope::tick(run);
            anti_effects::tick(run);
            norecoil::tick(run);
            usleep(10000);
        }
        diag::log("STATUS", "exit restoration attempted; anti-flash=%s smoke-screen=%s norecoil=%s", anti_effects::flash.status, anti_effects::smoke.status,norecoil::status);
        print_status(oxorany("bye"));
    }

    touch_aim::device.close_device();
    mesh_chams::shutdown();
    shutdown();
    return 0;
}
