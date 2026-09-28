#pragma once
#include <stdint.h>
/* Logical cockpit validity, independent of the permanent recovery wire ABI. */
#define COCKPIT_VALID_RPM (1u << 0)
#define COCKPIT_VALID_SPEED (1u << 1)
#define COCKPIT_VALID_GEAR (1u << 2)
#define COCKPIT_VALID_COOLANT (1u << 3)
#define COCKPIT_VALID_OIL (1u << 4)
#define COCKPIT_CLOSURE_SHIFT 5u
#define COCKPIT_CLOSURE_MASK 0x3fu
#define COCKPIT_VALID_ALL 0x07ffu
#define COCKPIT_CLOSURE_FL (1u << 0)
#define COCKPIT_CLOSURE_FR (1u << 1)
#define COCKPIT_CLOSURE_RL (1u << 2)
#define COCKPIT_CLOSURE_RR (1u << 3)
#define COCKPIT_CLOSURE_TRUNK (1u << 4)
#define COCKPIT_CLOSURE_HOOD (1u << 5)
typedef enum {
  COCKPIT_GEAR_UNKNOWN = 0,
  COCKPIT_GEAR_P = 1,
  COCKPIT_GEAR_R = 2,
  COCKPIT_GEAR_N = 3,
  COCKPIT_GEAR_D = 4,
  COCKPIT_GEAR_S = 5,
  COCKPIT_GEAR_M = 6
} cockpit_gear_selector_t;
/* Selector in high nibble; actual forward gear 1..8, or unknown 0, in low. */
