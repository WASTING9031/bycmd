#pragma once

namespace ui::bar {
    inline bool g_open = false;
    inline float g_alpha = 0.f;
    inline float g_game_alpha = 1.f;
    // Process whose CPU / RAM is shown in the stats pill (game pid, 0 = this process).
    inline int g_stats_pid = 0;

    void render();
    void render_stats();
    void set_game_alpha(float a);
    float game_alpha();
}
