#pragma once

namespace ui::menu {
    void render();
    // Set by the Exit button (after confirmation). main.cpp restores all game patches and quits.
    inline bool g_exit_requested = false;
}

