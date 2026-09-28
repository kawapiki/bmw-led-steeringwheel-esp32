#include "telemetry_v1.h"
static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void write32(uint8_t *p, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}
void telemetry_v1_encode(uint8_t out[16], uint32_t sequence, uint32_t rpm,
                         uint32_t uptime) {
  write32(out, 1); write32(out + 4, sequence);
  write32(out + 8, rpm); write32(out + 12, uptime);
}
bool telemetry_v1_accept(telemetry_v1_tracker_t *t, uint32_t session,
                         const uint8_t *p, size_t n, uint64_t requested,
                         uint64_t now, telemetry_v1_sample_t *sample) {
  if (!t || !sample || !p || n != 16 || !session || !requested ||
      now < requested || now - requested > 500 || read32(p) != 1)
    return false;
  uint32_t sequence = read32(p + 4), rpm = read32(p + 8), uptime = read32(p + 12);
  if (rpm > 10000) return false;
  if (t->seen && t->session == session) {
    uint32_t ds = sequence - t->sequence, dt = uptime - t->uptime;
    if (!ds || ds >= 0x80000000u || !dt || dt >= 0x80000000u) return false;
  }
  *t = (telemetry_v1_tracker_t){session, sequence, uptime, true};
  *sample = (telemetry_v1_sample_t){sequence, rpm, requested};
  return true;
}
