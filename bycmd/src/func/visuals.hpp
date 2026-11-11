#pragma once
#include "imgui.h"
#include <cstdint>

namespace visuals {
    void draw();
    void draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text);
    void dtracer(const ImVec2& foot, float a);
    void doffscreen(const ImVec2& screen_dir, float distance, float a);
}
