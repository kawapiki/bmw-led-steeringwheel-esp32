#include "telemetry_lighting.h"
#include <string.h>
void lighting_encode(uint8_t out[16], uint32_t sequence, uint32_t uptime,
                     const lighting_sample_t *s) {
  memset(out, 0, 16);
  out[0] = 1;
  out[1] = s->source;
  out[2] = s->valid;
  out[3] = s->on;
  for (unsigned i = 0; i < 4; ++i) {
    out[4 + i] = sequence >> (8 * i);
    out[8 + i] = uptime >> (8 * i);
  }
}
bool lighting_accept(telemetry_v1_tracker_t *tracker, uint32_t session,
                     const uint8_t *p, size_t n, uint64_t requested,
                     uint64_t now, lighting_sample_t *s) {
  if (!p || !s || n != 16 || p[0] != 1 || (p[1] != 1 && p[1] != 2) ||
      (p[2] & ~LIGHT_VALID_ALL) || (p[3] & ~LIGHT_VALID_ALL) || p[12] ||
      p[13] || p[14] || p[15])
    return false;
  uint32_t sequence = 0, uptime = 0;
  for (unsigned i = 0; i < 4; ++i) {
    sequence |= (uint32_t)p[4 + i] << (8 * i);
    uptime |= (uint32_t)p[8 + i] << (8 * i);
  }
  uint8_t legacy[16];
  telemetry_v1_sample_t accepted;
  telemetry_v1_encode(legacy, sequence, 0, uptime);
  if (!telemetry_v1_accept(tracker, session, legacy, 16, requested, now,
                           &accepted))
    return false;
  *s = (lighting_sample_t){.sequence = accepted.sequence,
                           .received = accepted.received,
                           .source = p[1],
                           .valid = p[2],
                           .on = p[3] & p[2]};
  return true;
}
void lighting_demo(uint32_t now, lighting_sample_t *s) {
  uint32_t phase = (now % 60000) / 6000;
  *s = (lighting_sample_t){.source = 1, .valid = LIGHT_VALID_ALL};
  if (phase == 0)
    s->on = LIGHT_LOW_BEAM;
  else if (phase == 1)
    s->on = LIGHT_ANGEL_EYE;
  else if (phase == 2)
    s->on = LIGHT_HIGH_BEAM;
  else if (phase == 3 && ((now / 500) % 2 == 0))
    s->on = LIGHT_LEFT_INDICATOR;
  else if (phase == 4 && ((now / 500) % 2 == 0))
    s->on = LIGHT_RIGHT_INDICATOR;
  else if (phase == 5)
    s->on = LIGHT_BRAKE;
}
