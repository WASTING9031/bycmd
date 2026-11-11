#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// WeaponId.Raw values explicitly supplied by the user (decimal enum values).
// These are NOT static-field offsets. SurvivorHands was truncated, so omitted.
namespace weapon_catalog {
enum class icon_kind { none, pistol, smg, rifle, sniper, shotgun, machinegun,
                       knife, grenade, bomb, gear, hands, effect, unknown };
struct definition { const char* name; icon_kind kind; uint8_t raw_id; uint8_t variant; };
inline constexpr std::array<definition, 65> definitions{{
    {"None", icon_kind::none, 0, 18}, // None
    {"G22", icon_kind::pistol, 11, 19}, // G22
    {"USP", icon_kind::pistol, 12, 20}, // USP
    {"P350", icon_kind::pistol, 13, 21}, // P350
    {"Deagle", icon_kind::pistol, 15, 22}, // Deagle
    {"Tec9", icon_kind::pistol, 16, 23}, // Tec9
    {"Five-Seven", icon_kind::pistol, 17, 24}, // FiveSeven
    {"Dual Berettas", icon_kind::pistol, 18, 25}, // Berettas
    {"UMP45", icon_kind::smg, 32, 26}, // UMP45
    {"Akimbo Uzi", icon_kind::smg, 33, 27}, // AkimboUzi
    {"MP7", icon_kind::smg, 34, 28}, // MP7
    {"P90", icon_kind::smg, 35, 29}, // P90
    {"MP5", icon_kind::smg, 36, 30}, // MP5
    {"MAC10", icon_kind::smg, 37, 31}, // MAC10
    {"VAL", icon_kind::rifle, 42, 32}, // VAL
    {"M4A1", icon_kind::rifle, 43, 33}, // M4A1
    {"AKR", icon_kind::rifle, 44, 34}, // AKR
    {"AKR12", icon_kind::rifle, 45, 35}, // AKR12
    {"M4", icon_kind::rifle, 46, 36}, // M4
    {"M16", icon_kind::rifle, 47, 37}, // M16
    {"FAMAS", icon_kind::rifle, 48, 38}, // FAMAS
    {"FN FAL", icon_kind::rifle, 49, 39}, // FnFal
    {"AWM", icon_kind::sniper, 51, 40}, // AWM
    {"M40", icon_kind::sniper, 52, 41}, // M40
    {"M110", icon_kind::sniper, 53, 42}, // M110
    {"Mallard", icon_kind::sniper, 54, 43}, // Mallard
    {"SM1014", icon_kind::shotgun, 62, 44}, // SM1014
    {"FabM", icon_kind::shotgun, 63, 45}, // FabM
    {"M60", icon_kind::machinegun, 64, 46}, // M60
    {"SPAS", icon_kind::shotgun, 65, 47}, // SPAS
    {"Knife", icon_kind::knife, 70, 48}, // Knife
    {"Bayonet", icon_kind::knife, 71, 101}, // KnifeBayonet
    {"Karambit", icon_kind::knife, 72, 102}, // KnifeKarambit
    {"jKommando", icon_kind::knife, 73, 103}, // jKommando
    {"Butterfly", icon_kind::knife, 75, 104}, // KnifeButterfly
    {"Flip Knife", icon_kind::knife, 77, 105}, // FlipKnife
    {"Kunai", icon_kind::knife, 78, 106}, // KunaiKnife
    {"Scorpion", icon_kind::knife, 79, 107}, // ScorpionKnife
    {"Tanto", icon_kind::knife, 80, 108}, // KnifeTanto
    {"Daggers", icon_kind::knife, 81, 109}, // DaggerKnife
    {"Kukri", icon_kind::knife, 82, 110}, // KnifeKukri
    {"Stiletto", icon_kind::knife, 83, 111}, // KnifeStilet
    {"Mantis", icon_kind::knife, 85, 112}, // KnifeMantis
    {"Fang", icon_kind::knife, 86, 113}, // KnifeFang
    {"Sting", icon_kind::knife, 88, 114}, // KnifeSting
    {"Hands", icon_kind::hands, 89, 49}, // Hands
    {"HE Grenade", icon_kind::grenade, 91, 50}, // GrenadeHE
    {"Smoke", icon_kind::grenade, 92, 51}, // GrenadeSmoke
    {"Flashbang", icon_kind::grenade, 93, 52}, // GrenadeFlash
    {"Molotov", icon_kind::grenade, 94, 53}, // GrenadeMolotov
    {"Incendiary", icon_kind::grenade, 95, 54}, // GrenadeIncendiary
    {"Bomb", icon_kind::bomb, 100, 55}, // Bomb
    {"Defuser", icon_kind::gear, 101, 56}, // Defuser
    {"Defuse Kit", icon_kind::gear, 102, 57}, // DefuseKit
    {"Vest", icon_kind::gear, 110, 58}, // Vest
    {"Vest + Helmet", icon_kind::gear, 111, 59}, // VestAndHelmet
    {"Watergun", icon_kind::pistol, 19, 115}, // Watergun
    {"Sci-fi Railgun", icon_kind::sniper, 66, 116}, // ScifiRailgun
    {"Sci-fi Shotgun", icon_kind::shotgun, 68, 117}, // ScifiShotgun
    {"Sci-fi P90", icon_kind::smg, 69, 118}, // ScifiP90
    {"Candy Cane", icon_kind::knife, 84, 119}, // CandyCane
    {"Rose Grenade", icon_kind::grenade, 96, 120}, // RoseGrenade
    {"Event Molotov", icon_kind::grenade, 97, 121}, // EventGrenadeMolotov
    {"Snowball", icon_kind::grenade, 99, 122}, // GrenadeSnowball
    {"Rose Hands", icon_kind::hands, 74, 123}, // RoseHands
}};
inline constexpr const definition* lookup(uint8_t raw) {
    for (const auto& d : definitions)
        if (d.raw_id == raw) return &d;
    return nullptr;
}
} // namespace weapon_catalog
