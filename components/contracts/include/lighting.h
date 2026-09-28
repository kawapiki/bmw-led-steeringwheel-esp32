#pragma once
#include <stdint.h>
/* Normalized on/off phases from gateway, not animation commands. */
#define LIGHT_LOW_BEAM (1u << 0)
#define LIGHT_HIGH_BEAM (1u << 1)
#define LIGHT_ANGEL_EYE (1u << 2)
#define LIGHT_LEFT_INDICATOR (1u << 3)
#define LIGHT_RIGHT_INDICATOR (1u << 4)
#define LIGHT_BRAKE (1u << 5)
#define LIGHT_VALID_ALL 0x3fu
