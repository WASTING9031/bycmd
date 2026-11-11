#pragma once
#include "imgui.h"
#include "cfg.hpp"
extern ImFont* espFont;
namespace preview_assets { inline ImFont* pixel=nullptr; }
namespace esp_fonts_ui {
// Selectable ESP fonts (loaded into the atlas by preview_assets::add).
enum { f_pixel=0, f_verdana, f_roboto, f_montserrat, f_press_start, f_rubik, f_ui, f_count };
inline const char* names[f_count] = {"Smallest Pixel", "Verdana", "Roboto Bold", "Montserrat", "Press Start 2P", "Rubik", "Menu sans"};
inline ImFont* fonts[f_count] = {};
inline float base_scale[f_count] = {1.f, 1.f, 1.f, 1.f, .75f, 1.f, 1.f}; // Press Start is wide
inline ImFont* get(int i) {
    if(i>=0&&i<f_count&&fonts[i]) return fonts[i];
    if(preview_assets::pixel) return preview_assets::pixel;
    return espFont?espFont:ImGui::GetFont();
}
inline float scale(int i) { return i>=0&&i<f_count?base_scale[i]:1.f; }
// Flags (money / ping / hp number / distance) always use the pixel font.
inline ImFont* pixel() { return get(f_pixel); }
inline ImFont* current() { return pixel(); }
inline ImFont* name_font() { return get(cfg::esp::name_font); }
inline ImFont* weapon_font() { return get(cfg::esp::weapon_font); }
}
