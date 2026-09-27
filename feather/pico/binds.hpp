#pragma once

#include <cstdint>

// Keyboard + mouse-button mapping. Hand-kept copy of config/binds.json (which
// scripts/binds.py reads for the relay); nothing generates it, so change both.

void binds_map(uint8_t mods, const uint8_t keys[6], uint8_t mouse_btns, float *lx, float *ly,
               uint32_t *pad_bits);
