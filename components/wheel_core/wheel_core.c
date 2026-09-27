#include "wheel_core.h"
#include <string.h>

int recovery_validate(const uint8_t *frame, size_t len, bool encrypted,
                      bool bonded, bool trusted_peer) {
  if (!frame || len < RECOVERY_HEADER_SIZE ||
      len > RECOVERY_HEADER_SIZE + RECOVERY_MAX_PAYLOAD)
    return -1;
  const size_t payload = (size_t)frame[4] | ((size_t)frame[5] << 8);
  if (payload != len - RECOVERY_HEADER_SIZE)
    return -1;
  if (frame[0] != 0xe9 || frame[1] != 0x90 || frame[2] != 1 || frame[3] < 1 ||
      frame[3] > 7 || frame[6] || frame[7])
    return -2;
  /* Transport supplies actual bond/encryption state, never packet flags.
     No application protocol version appears in this gate. */
  if (!encrypted || !bonded || !trusted_peer)
    return -3;
  if (frame[3] == 3) {
    if (payload < 2)
      return -4;
    const unsigned ssid_len = frame[24], pass_len = frame[25];
    if (ssid_len < 1 || ssid_len > 32 || pass_len < 8 || pass_len > 63 ||
        payload != 2u + ssid_len + pass_len)
      return -4;
    for (size_t i = 26u; i < 26u + ssid_len; ++i)
      if (frame[i] == 0)
        return -4;
    for (size_t i = 26u + ssid_len; i < len; ++i)
      if (frame[i] < 32 || frame[i] > 126)
        return -4;
  }
  if (frame[3] == 4 && payload != 36)
    return -2;
  if (frame[3] != 3 && frame[3] != 4 && payload != 0)
    return -2;
  return 0;
}

bool update_wheel_allowed(const update_gate_t *g) {
  if (!g || g->requested_release == 0)
    return false;
  if (g->standalone)
    return true; /* Explicit wheel-only mode, never paired success. */
  if (!g->gateway_online || !g->gateway_image_valid ||
      !g->authenticated_status || !g->status_fresh ||
      g->confirmed_release != g->requested_release ||
      !g->requested_transaction ||
      g->confirmed_transaction != g->requested_transaction)
    return false;
  uint8_t difference = 0, any_digest = 0;
  for (unsigned i = 0; i < 32; ++i) {
    difference |= g->confirmed_digest[i] ^ g->requested_digest[i];
    any_digest |= g->requested_digest[i];
  }
  return difference == 0 && any_digest != 0;
}

uint32_t rpm_mask(uint32_t rpm) {
  if (rpm <= 2000)
    return 0;
  unsigned pixels = rpm >= 6500 ? 23 : ((rpm - 2000) * 23u / 4500u);
  return (1u << pixels) - 1u; /* Bit 23 belongs exclusively to button light. */
}

int recovery_fragment(recovery_assembly_t *s, const uint8_t *p, size_t n,
                      uint64_t now) {
  if (!s)
    return -1;
  if (!p || n < 3 || n > 20 || p[1] < 24 || p[1] > 152 || p[0] > p[1] ||
      n - 2 > (size_t)(p[1] - p[0])) {
    memset(s, 0, sizeof(*s));
    return -1;
  }
  if (p[0] == 0) {
    memset(s, 0, sizeof(*s));
    s->expected = p[1];
    s->started = now;
  }
  if (s->expected != p[1] || s->size != p[0] || now < s->started ||
      now - s->started > 3000) {
    memset(s, 0, sizeof(*s));
    return -1;
  }
  memcpy(s->data + s->size, p + 2, n - 2);
  s->size += n - 2;
  return s->size == s->expected ? 1 : 0;
}
static uint32_t read_le(const uint8_t *p) {
  return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
bool manifest_layout_valid(const uint8_t *p, size_t n) {
  if (!p || n != 480 || memcmp(p, "BMW1", 4) || !read_le(p + 4))
    return false;
  uint32_t w = read_le(p + 8), g = read_le(p + 12);
  if (w <= 256 || w > 0x600000 || g <= 256 || g > 0x1e0000)
    return false;
  for (unsigned i = 80; i < 92; i += 4)
    if (read_le(p + i) != 1)
      return false;
  if (read_le(p + 92) == 0)
    return false;
  for (unsigned j = 0; j < 2; j++) {
    uint8_t v = 0;
    for (unsigned i = 0; i < 32; i++)
      v |= p[16 + j * 32 + i];
    if (!v)
      return false;
  }
  return true;
}
