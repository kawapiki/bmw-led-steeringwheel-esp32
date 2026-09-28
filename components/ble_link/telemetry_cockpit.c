#include "telemetry_cockpit.h"
#include <string.h>
static uint16_t read16(const uint8_t *p) {
  return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}
static uint32_t read32(const uint8_t *p) {
  return (uint32_t)read16(p) | (uint32_t)read16(p + 2) << 16;
}
static void write16(uint8_t *p, uint16_t v) {
  p[0] = v;
  p[1] = v >> 8;
}
static void write32(uint8_t *p, uint32_t v) {
  write16(p, v);
  write16(p + 2, v >> 16);
}
void cockpit_encode(uint8_t out[22], uint32_t seq, uint32_t uptime,
                    const cockpit_sample_t *s) {
  memset(out, 0, 22);
  out[0] = 1;
  out[1] = s->source;
  write16(out + 2, s->valid);
  write32(out + 4, seq);
  write32(out + 8, uptime);
  write16(out + 12, s->rpm);
  write16(out + 14, s->speed_dkph);
  out[16] = s->gear;
  out[17] = (uint8_t)(s->coolant_c + 40);
  out[18] = (uint8_t)(s->oil_c + 40);
  out[19] = s->closure_open;
}
bool cockpit_accept(telemetry_v1_tracker_t *t, uint32_t session,
                    const uint8_t *p, size_t n, uint64_t requested,
                    uint64_t now, cockpit_sample_t *s) {
  if (!p || !s || n != 22 || p[0] != 1 || (p[1] != 1 && p[1] != 2) || p[20] ||
      p[21] || (p[19] & ~0x3f))
    return false;
  uint16_t valid = read16(p + 2), rpm = read16(p + 12), speed = read16(p + 14);
  if (valid & ~COCKPIT_VALID_ALL)
    return false;
  uint8_t selector = p[16] >> 4, gear = p[16] & 15;
  if (((valid & COCKPIT_VALID_RPM) && rpm > 10000) ||
      ((valid & COCKPIT_VALID_SPEED) && speed > 3500) ||
      ((valid & COCKPIT_VALID_GEAR) &&
       (!selector || selector > 6 || gear > 8 || (selector <= 3 && gear))) ||
      ((valid & COCKPIT_VALID_COOLANT) && p[17] > 190) ||
      ((valid & COCKPIT_VALID_OIL) && p[18] > 220))
    return false;
  /* Reuse the deployed sequence/session/clock guard with explicitly encoded
   * synthetic RPM=0 when that field is unknown; validity remains independent.
   */
  uint8_t legacy[16];
  telemetry_v1_sample_t accepted;
  telemetry_v1_encode(legacy, read32(p + 4),
                      (valid & COCKPIT_VALID_RPM) ? rpm : 0, read32(p + 8));
  if (!telemetry_v1_accept(t, session, legacy, 16, requested, now, &accepted))
    return false;
  *s = (cockpit_sample_t){
      .sequence = accepted.sequence,
      .source = p[1],
      .received = accepted.received,
      .valid = valid,
      .rpm = (valid & COCKPIT_VALID_RPM) ? rpm : 0,
      .speed_dkph = (valid & COCKPIT_VALID_SPEED) ? speed : 0,
      .gear = (valid & COCKPIT_VALID_GEAR) ? p[16] : 0,
      .coolant_c = (valid & COCKPIT_VALID_COOLANT) ? (int16_t)p[17] - 40 : 0,
      .oil_c = (valid & COCKPIT_VALID_OIL) ? (int16_t)p[18] - 40 : 0,
      .closure_open =
          p[19] & ((valid >> COCKPIT_CLOSURE_SHIFT) & COCKPIT_CLOSURE_MASK)};
  return true;
}
void cockpit_demo(uint32_t now, cockpit_sample_t *s) {
  uint32_t phase = now % 60000, t = now % 18000;
  *s = (cockpit_sample_t){.source = 1,
                          .valid = COCKPIT_VALID_ALL,
                          .rpm = 800,
                          .gear = 0x10,
                          .coolant_c = 95 + (int16_t)((now / 5000) % 3),
                          .oil_c = 105 + (int16_t)((now / 7000) % 4)};
  if (phase < 6000)
    return;
  if (phase < 18000) {
    s->closure_open = 1u << ((phase - 6000) / 2000);
    return;
  }
  if (phase < 21000) {
    s->closure_open =
        COCKPIT_CLOSURE_FL | COCKPIT_CLOSURE_RR | COCKPIT_CLOSURE_TRUNK;
    return;
  }
  if (phase < 24000)
    return;
  uint32_t drive = phase - 24000;
  s->speed_dkph =
      drive < 18000 ? drive * 1300 / 18000 : (36000 - drive) * 1300 / 18000;
  s->gear = 0x40 | (1 + s->speed_dkph / 240);
  s->rpm = t < 12000   ? 800 + t * 6200 / 12000
           : t < 14000 ? 7000
                       : 7000 - (t - 14000) * 6200 / 4000;
}
