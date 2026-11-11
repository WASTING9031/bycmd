#pragma once
#include "game.hpp"

// Global view/projection matrices written by visuals::draw() after camera
// selection and consumed by silent_aim and any world-to-screen callers.
inline matrix g_view{}, g_proj{};
