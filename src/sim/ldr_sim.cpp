/**
 * @file ldr_sim.cpp
 * @brief Ambient light = full brightness. No auto-dim in the sim.
 */

#include "../system/ldr.h"

void ldr_init(void) {}
void ldr_loop(void) {}
float ldr_normalized(void) { return 1.0f; }
