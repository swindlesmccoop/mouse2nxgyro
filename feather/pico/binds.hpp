#pragma once

#include <cstdint>

// Keyboard + mouse-button mapping baked from config/binds.json (same table as
// scripts/binds.py). No PC reload in the play path.

void binds_map(uint8_t mods, const uint8_t keys[6], uint8_t mouse_btns, float *lx, float *ly,
               uint32_t *pad_bits);
