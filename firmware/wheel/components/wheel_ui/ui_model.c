#include "ui_model.h"
void ui_nav_key(ui_nav_t *n, ui_key_t key) {
  if (key == UI_BACK) {
    if (n->screen == UI_DETAIL)
      n->screen = UI_MENU;
    else if (n->screen == UI_MENU)
      n->screen = UI_SERVICE;
    else
      n->screen = UI_ENGINE;
  } else if (key == UI_NEXT) {
    if (n->screen == UI_MENU)
      n->item = (n->item + 1) % UI_SERVICE_COUNT;
    else if (n->screen <= UI_SERVICE)
      n->screen = (n->screen + 1) % (UI_SERVICE + 1);
  } else if (key == UI_SELECT) {
    if (n->screen == UI_SERVICE)
      n->screen = UI_MENU;
    else if (n->screen == UI_MENU)
      n->screen = UI_DETAIL;
  }
}
ui_reading_t ui_reading(const ui_telemetry_t *t) {
  ui_reading_t r = {0};
  if (!t->secure)
    r.status = UI_DATA_OFFLINE;
  else if (!t->compatible)
    r.status = UI_DATA_INCOMPATIBLE;
  else if (!t->fresh)
    r.status = UI_DATA_STALE;
  else if (!t->valid)
    r.status = UI_DATA_INVALID;
  else {
    r.valid_fields = t->valid_fields & 31u;
    r.available = (r.valid_fields & UI_VALID_RPM) != 0;
    r.rpm = r.available ? t->rpm : 0;
    r.speed_dkph = t->speed_dkph;
    r.gear = t->gear;
    r.coolant_c = t->coolant_c;
    r.oil_c = t->oil_c;
    r.closure_known = t->closure_known & 63u;
    r.closure_open = t->closure_open & r.closure_known;
    r.status = t->demo ? UI_DATA_DEMO : UI_DATA_LIVE;
  }
  return r;
}

#include <string.h>
static bool offer_equal(const ui_offer_t *a, const ui_offer_t *b) {
  return a->generation == b->generation && a->release == b->release &&
         a->standalone == b->standalone && !memcmp(a->digest, b->digest, 32);
}
void ui_confirmation_show(ui_confirmation_t *c, const ui_offer_t *o,
                          uint64_t now) {
  if (!o) {
    memset(c, 0, sizeof(*c));
    return;
  }
  if (!c->visible || !offer_equal(&c->shown, o)) {
    c->shown = *o;
    c->opened = now;
    c->armed = false;
  }
  c->visible = true;
}
void ui_confirmation_release(ui_confirmation_t *c, bool released,
                             uint64_t now) {
  if (c->visible && released && now >= c->opened && now - c->opened > 50)
    c->armed = true;
}
bool ui_confirmation_take(ui_confirmation_t *c, const ui_offer_t *o,
                          ui_offer_t *out) {
  bool ok = c->visible && c->armed && offer_equal(&c->shown, o);
  c->armed = false;
  if (ok) {
    *out = c->shown;
  }
  return ok;
}

void ui_door_update(ui_door_state_t *s, const ui_reading_t *r,
                    ui_screen_t screen, bool suppressed) {
  uint8_t known = r->closure_known & 63u;
  uint8_t open = (s->open & ~known) | (r->closure_open & known);
  if (open & ~s->open)
    s->acknowledged = false;
  s->open = open;
  s->known = known;
  if (open)
    s->active = true;
  if (known == 63u && !open) {
    s->active = false;
    s->acknowledged = false;
  }
  s->visible = !suppressed && s->active && !s->acknowledged &&
               (screen == UI_ENGINE || screen == UI_SHIFT);
}
bool ui_door_acknowledge(ui_door_state_t *s) {
  if (!s->visible)
    return false;
  s->acknowledged = true;
  s->visible = false;
  return true;
}
void ui_gear_text(char out[4], uint8_t gear, bool valid) {
  static const char modes[] = "?PRNDSM";
  unsigned selector = gear >> 4, number = gear & 15;
  if (!valid || !selector || selector > 6 || number > 8) {
    memcpy(out, "--", 3);
    return;
  }
  out[0] = modes[selector];
  out[1] = selector >= 4 && number ? '0' + number : 0;
  out[2] = 0;
}
