#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum {
  UI_ENGINE,
  UI_SHIFT,
  UI_GATEWAY,
  UI_SERVICE,
  UI_MENU,
  UI_DETAIL
} ui_screen_t;
typedef enum { UI_NEXT, UI_SELECT, UI_BACK } ui_key_t;
#define UI_SERVICE_COUNT 9u
typedef struct {
  ui_screen_t screen;
  unsigned item;
} ui_nav_t;
enum {
  UI_VALID_RPM = 1u,
  UI_VALID_SPEED = 2u,
  UI_VALID_GEAR = 4u,
  UI_VALID_COOLANT = 8u,
  UI_VALID_OIL = 16u
};
typedef struct {
  bool secure, compatible, valid, demo, fresh;
  uint32_t rpm;
  uint16_t valid_fields, speed_dkph;
  uint8_t gear, closure_open, closure_known;
  int16_t coolant_c, oil_c;
} ui_telemetry_t;
typedef enum {
  UI_DATA_OFFLINE,
  UI_DATA_INCOMPATIBLE,
  UI_DATA_STALE,
  UI_DATA_INVALID,
  UI_DATA_DEMO,
  UI_DATA_LIVE
} ui_data_status_t;
typedef struct {
  bool available;
  uint32_t rpm;
  ui_data_status_t status;
  uint16_t valid_fields, speed_dkph;
  uint8_t gear, closure_open, closure_known;
  int16_t coolant_c, oil_c;
} ui_reading_t;
typedef struct {
  uint8_t open, known;
  bool active, acknowledged, visible;
} ui_door_state_t;
void ui_door_update(ui_door_state_t *state, const ui_reading_t *reading,
                    ui_screen_t screen, bool suppressed);
bool ui_door_acknowledge(ui_door_state_t *state);
void ui_gear_text(char out[4], uint8_t gear, bool valid);
void ui_nav_key(ui_nav_t *nav, ui_key_t key);
ui_reading_t ui_reading(const ui_telemetry_t *telemetry);

/* A confirmation is bound to the offer actually rendered, never re-read later.
 */
typedef struct {
  uint32_t generation, release;
  uint8_t digest[32];
  bool standalone;
} ui_offer_t;
typedef struct {
  ui_offer_t shown;
  bool visible, armed;
  uint64_t opened;
} ui_confirmation_t;
void ui_confirmation_show(ui_confirmation_t *c, const ui_offer_t *offer,
                          uint64_t now);
void ui_confirmation_release(ui_confirmation_t *c, bool released, uint64_t now);
bool ui_confirmation_take(ui_confirmation_t *c, const ui_offer_t *current,
                          ui_offer_t *out);
