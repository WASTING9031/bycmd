#pragma once
#include "imgui.h"
#include "../game/weapon_catalog.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

extern ImFont* weaponIconFont; // loaded in Android_draw/draw.cpp (standoff2-weapon-icons.ttf)

// Primary: Weapons[] extracted from skeed; exact source mapping, including Mallard/M40 alias.
// Unmapped entries retain the previous font when possible, then the schematic fallback.
namespace weapon_icons {
// Ink bounds measured from the TTF in font units (1024/em), y from top of line (ascent 960).
struct glyph_info { uint8_t raw_id; char glyph; int16_t x0, y0, x1, y1; };
inline ImFont* legacy_font = nullptr;
inline constexpr glyph_info legacy_glyphs[] = {
    {11,'A',0,112,1178,912}, // G22
    {12,'B',0,123,1819,901}, // USP
    {13,'C',0,98,1126,927}, // P350
    {15,'D',0,98,1433,927}, // Deagle
    {16,'E',0,40,1459,984}, // Tec9
    {17,'F',0,121,1126,905}, // Five-Seven
    {18,'G',0,89,1996,934}, // Berettas
    {32,'H',0,79,1919,946}, // UMP45
    {33,'I',0,34,2074,990}, // Akimbo Uzi
    {34,'J',0,28,1408,995}, // MP7
    {35,'K',0,140,1741,883}, // P90
    {36,'L',0,138,1946,886}, // MP5
    {37,'M',0,63,1664,961}, // MAC10
    {42,'N',0,169,2944,854}, // VAL
    {43,'O',0,155,2816,869}, // M4A1
    {44,'P',0,121,2714,903}, // AKR
    {45,'Q',0,49,2765,974}, // AKR12
    {46,'R',0,74,2637,950}, // M4
    {47,'S',0,109,2816,914}, // M16
    {48,'T',0,67,2432,957}, // FAMAS
    {49,'U',0,141,2995,883}, // FN FAL
    {51,'V',0,211,3072,814}, // AWM
    {52,'W',0,190,3200,834}, // M40
    {53,'X',0,137,2816,886}, // M110
    {62,'Y',-1,227,2970,796}, // SM1014
    {63,'Z',0,199,2560,825}, // FabM
    {64,'a',0,97,3073,927}, // M60
    {65,'b',0,171,2278,854}, // SPAS
    {70,'0',0,292,2125,732}, // Kabar
    {71,'1',0,236,2150,788}, // M9 Bayonet
    {72,'2',0,114,1958,910}, // Karambit
    {73,'3',0,366,2586,657}, // jKommando
    {75,'4',0,375,2355,650}, // Butterfly
    {77,'5',0,336,2074,689}, // Flip
    {78,'6',0,357,2176,667}, // Kunai
    {79,'7',0,284,1972,739}, // Scorpion
    {80,'8',0,388,2074,637}, // Tanto
    {81,'9',1,136,2303,889}, // Dual Daggers
    {82,'[',0,255,2176,770}, // Kukri
    {83,']',0,360,2253,659}, // Stiletto
    {85,'{',0,168,1919,856}, // Mantis
    {86,'}',0,267,2125,758}, // Fang
    {88,'(',-1,335,2176,690}, // Sting
    {91,'c',-1,34,614,990}, // HE
    {92,'d',-1,28,628,996}, // Smoke
    {93,'e',2,0,562,1024}, // Flashbang
    {94,'f',12,-1,705,1024}, // Molotov
    {95,'g',-1,39,615,985}, // Incendiary
    {97,'f',12,-1,705,1024}, // Event Molotov (molotov glyph)
    {100,'h',0,26,1280,998}, // Bomb
    {101,'i',28,0,778,1024}, // Defuser (defuse kit glyph)
    {102,'i',28,0,778,1024}, // Defuse Kit
};
// Exact glyph bounds extracted from Weapons[]; y measured from its hhea ascent 960.
inline constexpr glyph_info new_glyphs[] = {
    {11,'D',75,102,1395,922},
    {12,'G',83,102,2299,922},
    {13,'E',85,102,1175,923},
    {15,'A',93,101,1558,922},
    {16,'H',101,97,1526,922},
    {17,'C',87,102,1236,924},
    {18,'B',79,102,2451,923},
    {32,'L',90,102,2365,924},
    {33,'I',90,102,1325,922},
    {34,'N',97,102,1438,922},
    {35,'P',98,120,2015,897},
    {36,'M',93,124,2605,896},
    {37,'K',93,102,1251,922},
    {42,'Q',93,102,2975,923},
    {43,'T',91,102,3373,922},
    {44,'W',86,128,2720,896},
    {45,'V',83,75,2576,947},
    {46,'S',100,96,2626,927},
    {47,'U',83,51,2300,974},
    {48,'R',90,127,2215,896},
    {51,'Z',98,205,3371,819},
    {52,'a',85,126,3087,898},
    {53,'X',77,151,2786,875},
    {54,'a',85,126,3087,898},
    {62,'b',98,230,2666,796},
    {63,'c',94,250,2536,773},
    {64,'g',83,154,2611,872},
    {65,'e',96,230,2779,795},
    {91,'j',88,154,627,872},
    {92,'k',84,154,419,872},
    {93,'i',84,154,384,870},
    {94,'l',87,128,609,898},
    {95,'n',81,154,417,872},
    {97,'l',87,128,609,898},
    {100,'o',94,256,911,768},
};
struct selection { const glyph_info* info; ImFont* font; };
inline const glyph_info* lookup(const glyph_info* list,size_t count,uint8_t raw) {
    for(size_t i=0;i<count;++i) if(list[i].raw_id==raw) return &list[i];
    return nullptr;
}
inline selection choose(uint8_t raw) {
    const auto* g=lookup(new_glyphs,sizeof(new_glyphs)/sizeof(new_glyphs[0]),raw);
    if(g && weaponIconFont && weaponIconFont->FindGlyphNoFallback((ImWchar)g->glyph)) return {g,weaponIconFont};
    g=lookup(legacy_glyphs,sizeof(legacy_glyphs)/sizeof(legacy_glyphs[0]),raw);
    if(g && legacy_font && legacy_font->FindGlyphNoFallback((ImWchar)g->glyph)) return {g,legacy_font};
    return {nullptr,nullptr};
}
inline const glyph_info* find_glyph(uint8_t raw) { return choose(raw).info; }
inline bool has_glyph(const weapon_catalog::definition* weapon) {
    return weapon && choose(weapon->raw_id).info;
}
// One glyph pass: soft drop shadow + 1 px hard outline (4 directions) + fill.
// k = screen px per font unit; ink_tl = where the top-left of the ink lands.
inline void glyph_at(ImDrawList* dl, ImFont* font, const glyph_info* g, float k, ImVec2 ink_tl, ImU32 color) {
    const float size = k * 1024.f;                            // ImGui pixel height == em (asc-desc = 1024)
    const ImVec2 pos(std::round(ink_tl.x - g->x0 * k), std::round(ink_tl.y - g->y0 * k));
    const char text[2] = { g->glyph, 0 };
    const unsigned al = (color >> IM_COL32_A_SHIFT) & 255;
    const ImU32 edge = IM_COL32(0, 0, 0, (al * 230) / 255), shadow = IM_COL32(0, 0, 0, (al * 90) / 255);
    dl->AddText(font, size, ImVec2(pos.x + 1.5f, pos.y + 2.f), shadow, text, text + 1);
    static const float dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
    for (const auto& d : dirs)
        dl->AddText(font, size, ImVec2(pos.x + d[0], pos.y + d[1]), edge, text, text + 1);
    dl->AddText(font, size, pos, color, text, text + 1);
}
// Fits the glyph ink into width x (width*0.375), centred, aspect ratio preserved.
inline bool draw_glyph(ImDrawList* dl, const weapon_catalog::definition* weapon,
                       ImVec2 top_left, float width, ImU32 color) {
    if (!has_glyph(weapon)) return false;
    const auto selected=choose(weapon->raw_id); const glyph_info* g=selected.info;
    const float box_w = width, box_h = width * .375f;
    const float ink_w = float(g->x1 - g->x0), ink_h = float(g->y1 - g->y0);
    if (ink_w <= 0.f || ink_h <= 0.f) return false;
    const float k = std::min(box_w / ink_w, box_h / ink_h);   // screen px per font unit
    glyph_at(dl, selected.font, g, k, ImVec2(top_left.x + (box_w - ink_w * k) * .5f, top_left.y + (box_h - ink_h * k) * .5f), color);
    return true;
}
// Width an icon takes when its ink is exactly `height` px tall (glyph aspect, or the
// 64x24 schematic box when the font has no glyph for this id).
inline float width_at_height(const weapon_catalog::definition* weapon, float height) {
    if (has_glyph(weapon)) {
        const glyph_info* g = find_glyph(weapon->raw_id);
        const float ink_w = float(g->x1 - g->x0), ink_h = float(g->y1 - g->y0);
        if (ink_w > 0.f && ink_h > 0.f) return height * ink_w / ink_h;
    }
    return height * 64.f / 24.f;
}
inline void draw_schematic(ImDrawList* dl, const weapon_catalog::definition* weapon,
                           ImVec2 top_left, float width, ImU32 color) {
    if (!dl || !std::isfinite(width) || width <= 0.f) return;
    const float s = width / 64.f;
    const ImU32 edge = IM_COL32(0, 0, 0, (color >> IM_COL32_A_SHIFT) & 255);
    const auto p = [&](float x, float y) { return ImVec2(top_left.x+x*s, top_left.y+y*s); };
    const auto line = [&](float x, float y, float xx, float yy, float t = 2.f) {
        dl->AddLine(p(x,y), p(xx,yy), edge, (t+1.8f)*s);
        dl->AddLine(p(x,y), p(xx,yy), color, t*s);
    };
    const auto rect = [&](float x, float y, float xx, float yy) {
        dl->AddRectFilled(p(x,y), p(xx,yy), color, .5f*s);
        dl->AddRect(p(x,y), p(xx,yy), edge, .5f*s, 0, .9f*s);
    };
    const auto circle = [&](float x, float y, float r) {
        dl->AddCircleFilled(p(x,y), r*s, color, 16);
        dl->AddCircle(p(x,y), r*s, edge, 16, .9f*s);
    };
    using K = weapon_catalog::icon_kind;
    const K kind = weapon ? weapon->kind : K::unknown;
    const unsigned v = weapon ? weapon->variant : 0;
    switch (kind) {
    case K::pistol:
        rect(15,5,48,11); line(46,9,52,9,2);
        line(23,11,19,22,7); line(29,12,34,12,1);
        line(34,12,32,17,1); line(32,17,26,17,1);
        if (v == 25) { rect(3,9,14,13); line(8,13,5,21,3); } // Berettas
        if (v == 20) line(48,8,60,8,4); // USP suppressor
        break;
    case K::smg:
        rect(16,6,43,13); line(43,9,56,9,3);
        line(17,10,7,14,4); line(24,13,21,21,4);
        line(33,13,34,23,4);
        if (v == 29) rect(23,3,43,6); // P90 top magazine
        if (v == 27) { rect(2,8,10,14); line(5,14,3,20,3); }
        break;
    case K::rifle:
    case K::machinegun:
    case K::sniper:
    case K::shotgun:
        line(4,14,16,10,5); rect(16,7,40,13);
        line(40,9,61,9,2); line(23,13,20,21,4);
        line(28,13,34,13,1); line(34,13,32,18,1);
        line(32,18,25,18,1);
        if (kind == K::sniper) {
            line(25,3,44,3,3); line(29,4,29,7,1); line(39,4,39,7,1);
            line(50,10,47,22,1); line(50,10,55,22,1);
        } else if (kind == K::shotgun) {
            line(38,12,54,12,4); // pump/tube
            if (v == 45) line(25,13,30,20,3);
        } else if (kind == K::machinegun) {
            rect(30,13,42,23); line(54,10,51,22,1); line(54,10,59,22,1);
        } else {
            if (v == 34 || v == 35) { // AKR/AKR12 curved magazine
                line(33,13,34,18,4); line(34,18,39,22,4);
            } else line(33,13,31,22,4);
            if (v == 33 || v == 32) line(55,9,63,9,4); // M4A1 / VAL
            if (v == 35 || v == 37) line(29,4,38,4,2);
            if (v == 38) line(9,5,28,5,2); // FAMAS carry handle
        }
        break;
    case K::knife:
        rect(6,10,22,15); line(23,7,23,19,3);
        dl->AddTriangleFilled(p(25,9),p(59,8),p(27,16),color);
        line(25,9,59,8,1); line(59,8,27,16,1);
        if (v == 11) rect(27,5,53,15); // event cleaver
        break;
    case K::grenade:
        if (v == 53 || v == 54) { // bottle grenades
            rect(27,8,38,23); rect(30,2,35,9); line(35,3,41,0,1);
        } else {
            rect(27,2,35,7); circle(31,15,8);
            line(35,3,41,6,2); line(41,6,40,15,2);
            if (v == 51) { line(26,12,36,12,1); line(26,18,36,18,1); }
        }
        break;
    case K::bomb:
        rect(16,5,48,22); rect(21,8,33,15);
        line(37,8,44,8,1); line(37,12,44,12,1); line(37,16,44,16,1);
        line(23,4,21,1,1); line(21,1,42,1,1); line(42,1,40,5,1);
        break;
    case K::gear:
        rect(23,7,41,22); line(23,7,26,2,3); line(41,7,38,2,3);
        line(28,11,36,11,1); line(28,15,36,15,1);
        if (v == 59) { circle(48,7,6); rect(43,7,55,10); }
        break;
    case K::hands:
        rect(22,11,40,22);
        for (int i=0;i<4;++i) line(24+i*4,12,24+i*4,3+(i==0?3:0),2.5f);
        line(22,15,17,10,3);
        break;
    case K::effect:
        line(32,0,29,10,3); line(29,10,37,10,3); line(37,10,29,24,3);
        line(20,5,17,2,1); line(44,17,48,21,1);
        break;
    case K::none: break;
    case K::unknown:
        line(32,2,43,12,2); line(43,12,32,22,2);
        line(32,22,21,12,2); line(21,12,32,2,2);
        circle(32,12,2);
        break;
    }
}
inline void draw(ImDrawList* dl, const weapon_catalog::definition* weapon,
                 ImVec2 top_left, float width, ImU32 color) {
    if (!dl || !std::isfinite(width) || width <= 0.f) return;
    if (weapon && weapon->kind == weapon_catalog::icon_kind::none) return;
    if (draw_glyph(dl, weapon, top_left, width, color)) return;
    draw_schematic(dl, weapon, top_left, width, color);
}
// Draws the icon with its ink exactly `height` px tall starting at top_left; returns the width used.
inline float draw_at_height(ImDrawList* dl, const weapon_catalog::definition* weapon,
                            ImVec2 top_left, float height, ImU32 color) {
    if (!dl || !std::isfinite(height) || height <= 0.f) return 0.f;
    if (weapon && weapon->kind == weapon_catalog::icon_kind::none) return 0.f;
    const float w = width_at_height(weapon, height);
    if (has_glyph(weapon)) {
        const auto selected=choose(weapon->raw_id); const glyph_info* g=selected.info;
        glyph_at(dl, selected.font, g, height / float(g->y1 - g->y0), top_left, color);
    } else draw_schematic(dl, weapon, top_left, w, color);
    return w;
}
} // namespace weapon_icons
