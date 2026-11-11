#pragma once
// Fonts + images for the ESP preview, stored inside the ImGui font atlas
// (custom rects), so no extra GL texture code is needed.
#include "imgui.h"
#include "esp_font.hpp"
#include "fonts/esp/esp_fonts.hpp"
#include "fonts/esp/esp_fonts_extra.hpp"
#include "fonts/menu/ui_sans.hpp"
#include "assets/preview_art.hpp"
#include "assets/logo.hpp"
#include "assets/skin_preview.hpp"
#include "assets/c4_bomb.hpp"
#include <cstring>

namespace preview_assets {
inline ImFont* verdana = nullptr;  // nickname font
inline int girl_rect = -1, logo_rect = -1, c4_rect = -1;
inline bool ready = false;
inline int skin_rects[2]={-1,-1};

// Call after the other fonts were added and BEFORE the atlas is built.
inline void add(ImFontAtlas* atlas) {
    static const ImWchar ranges[] = {0x0020,0x00FF,0x0400,0x045F,0x2000,0x206F,0};
    ImFontConfig px; px.FontDataOwnedByAtlas = false; px.OversampleH = 1; px.OversampleV = 1; px.PixelSnapH = true;
    pixel = atlas->AddFontFromMemoryTTF((void*)esp_fonts::smallest_pixel, sizeof(esp_fonts::smallest_pixel), 20.f, &px, ranges);
    ImFontConfig vf; vf.FontDataOwnedByAtlas = false; vf.OversampleH = 2; vf.OversampleV = 2;
    verdana = atlas->AddFontFromMemoryTTF((void*)esp_fonts::verdana, sizeof(esp_fonts::verdana), 19.f, &vf, ranges);
    using namespace esp_fonts_ui;
    fonts[f_pixel] = pixel; fonts[f_verdana] = verdana;
    fonts[f_roboto] = atlas->AddFontFromMemoryTTF((void*)esp_fonts_extra::roboto_bold, sizeof(esp_fonts_extra::roboto_bold), 19.f, &vf, ranges);
    fonts[f_montserrat] = atlas->AddFontFromMemoryTTF((void*)esp_fonts_extra::montserrat, sizeof(esp_fonts_extra::montserrat), 19.f, &vf, ranges);
    fonts[f_press_start] = atlas->AddFontFromMemoryTTF((void*)esp_fonts_extra::press_start, sizeof(esp_fonts_extra::press_start), 16.f, &px, ranges);
    fonts[f_rubik] = atlas->AddFontFromMemoryTTF((void*)esp_fonts_extra::rubik, sizeof(esp_fonts_extra::rubik), 19.f, &vf, ranges);
    fonts[f_ui] = atlas->AddFontFromMemoryTTF((void*)ui_fonts::ui_regular, sizeof(ui_fonts::ui_regular), 19.f, &vf, ranges);
    for(int& id:skin_rects) id=atlas->AddCustomRectRegular(skin_art::width,skin_art::height);
    c4_rect = atlas->AddCustomRectRegular(c4_art::width, c4_art::height);
    logo_rect = atlas->AddCustomRectRegular(menu_logo::width, menu_logo::height);
    girl_rect = atlas->AddCustomRectRegular(preview_art::girl_w, preview_art::girl_h);
}

// Call once after atlas->Build(): copies the art into the RGBA32 atlas pixels.
inline void blit(ImFontAtlas* atlas) {
    unsigned char* px = nullptr; int tw = 0, th = 0;
    atlas->GetTexDataAsRGBA32(&px, &tw, &th);
    if (!px) return;
    auto copy = [&](int id, const unsigned char* src, int w, int h) {
        const ImFontAtlasCustomRect* r = atlas->GetCustomRectByIndex(id);
        if (!r || !r->IsPacked()) return false;
        for (int y = 0; y < h; ++y) std::memcpy(px + (size_t(r->Y + y) * tw + r->X) * 4, src + size_t(y) * w * 4, size_t(w) * 4);
        return true;
    };
    auto skin_copy=[&](int id,const unsigned int* runs,size_t length){
        const auto* r=atlas->GetCustomRectByIndex(id);if(!r||!r->IsPacked())return;
        size_t at=0;
        for(size_t i=0;i<length;i+=2)for(unsigned int j=0;j<runs[i];++j,++at){
            unsigned char* dst=px+((r->Y+at/skin_art::width)*size_t(tw)+r->X+at%skin_art::width)*4;
            const unsigned int v=runs[i+1];dst[0]=v&255;dst[1]=(v>>8)&255;dst[2]=(v>>16)&255;dst[3]=(v>>24)&255;
        }
    };
    skin_copy(skin_rects[0],skin_art::fabm,sizeof(skin_art::fabm)/sizeof(unsigned int));
    skin_copy(skin_rects[1],skin_art::karambit,sizeof(skin_art::karambit)/sizeof(unsigned int));
    const auto* logo = atlas->GetCustomRectByIndex(logo_rect);
    if (logo && logo->IsPacked()) {
        size_t at = 0;
        for (size_t i = 0; i < sizeof(menu_logo::runs)/sizeof(menu_logo::runs[0]); i += 2) {
            for (unsigned int j = 0; j < menu_logo::runs[i]; ++j, ++at) {
                const size_t x = at % menu_logo::width, y = at / menu_logo::width;
                unsigned char* dst = px + ((logo->Y+y)*size_t(tw)+logo->X+x)*4;
                dst[0]=dst[1]=dst[2]=255; dst[3]=static_cast<unsigned char>(menu_logo::runs[i+1]);
            }
        }
    }
    copy(c4_rect, c4_art::rgba, c4_art::width, c4_art::height);
    ready = copy(girl_rect, preview_art::girl_rgba, preview_art::girl_w, preview_art::girl_h);
}

inline bool uv(int id, ImVec2& a, ImVec2& b) {
    if (!ready || id < 0) return false;
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    const ImFontAtlasCustomRect* r = atlas->GetCustomRectByIndex(id);
    if (!r) return false;
    atlas->CalcCustomRectUV(r, &a, &b);
    return true;
}
} // namespace preview_assets
