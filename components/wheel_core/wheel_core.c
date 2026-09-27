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
  return (1u << pixels) - 1u; /* Logical RPM bits 0..22; the board maps them to physical pixels. */
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

int recovery_dispatch_accept(recovery_session_t *g, const uint8_t *p, size_t n,
                             uint32_t nonce) {
  if (!g || !nonce || recovery_validate(p, n, true, true, true))
    return RECOVERY_SESSION;
  if (g->nonce != nonce) {
    g->nonce = nonce;
    g->last_request = 0;
    g->established = false;
  }
  uint32_t session = read_le(p + 8), request = read_le(p + 12);
  if (session != nonce || !request)
    return RECOVERY_SESSION;
  if (request <= g->last_request)
    return RECOVERY_REPLAY;
  if (p[3] != 1 && !g->established)
    return RECOVERY_SESSION;
  g->last_request = request;
  if (p[3] == 1)
    g->established = true;
  return RECOVERY_OK;
}
uint32_t update_reconcile_phase(uint32_t phase, bool pending, bool match) {
  if (phase == UPDATE_DOWNLOADING)
    return UPDATE_FAILED;
  if (phase == UPDATE_BOOT_PENDING)
    return pending ? UPDATE_BOOT_PENDING : match ? UPDATE_VALID : UPDATE_FAILED;
  return phase;
}
int update_prepare(update_journal_t *j, uint32_t release, uint64_t tx,
                   const uint8_t hash[32], uint32_t floor) {
  if (!j || !hash || !tx || !release)
    return RECOVERY_STATE;
  if (j->phase == UPDATE_DOWNLOADING || j->phase == UPDATE_BOOT_PENDING)
    return RECOVERY_BUSY;
  if (release == floor && j->phase == UPDATE_VALID &&
      !memcmp(j->hash, hash, 32)) {
    j->tx = tx;
    return RECOVERY_OK;
  }
  if (release <= floor)
    return RECOVERY_STATE;
  if (j->phase == UPDATE_PREPARED && j->tx == tx && j->release == release &&
      !memcmp(j->hash, hash, 32))
    return RECOVERY_OK;
  if (j->tx == tx)
    return RECOVERY_STATE;
  *j = (update_journal_t){
      .schema = 1, .phase = UPDATE_PREPARED, .release = release, .tx = tx};
  memcpy(j->hash, hash, 32);
  return RECOVERY_OK;
}
int update_coordinator_action(uint32_t phase, bool match, bool active) {
  if (!match || phase == UPDATE_FAILED || phase == UPDATE_IDLE)
    return 0;
  if (phase == UPDATE_VALID)
    return 3;
  if (phase == UPDATE_PREPARED)
    return 1;
  if ((phase == UPDATE_DOWNLOADING || phase == UPDATE_BOOT_PENDING) && active)
    return 2;
  return 0;
}
bool recovery_auth_expired(bool connected, bool authenticated, uint64_t since,
                           uint64_t now) {
  return connected && !authenticated && now >= since && now - since >= 15000;
}
bool recovery_health_ready(uint64_t now, uint64_t worker, uint32_t wc,
                           uint64_t host, uint32_t hc) {
  return now >= 3000 && wc >= 3 && hc >= 3 && worker && host && now >= worker &&
         now >= host && now - worker <= 500 && now - host <= 500;
}
bool telemetry_is_fresh(bool connected, bool compatible, uint64_t received,
                        uint64_t now) {
  return connected && compatible && received && now >= received &&
         now - received <= 2500;
}
bool candidate_matches(uint32_t expected, const uint8_t a[32], uint32_t actual,
                       const uint8_t b[32]) {
  return expected && expected == actual && a && b && !memcmp(a, b, 32);
}
bool recovery_fragment_expire(recovery_assembly_t *s, uint64_t now) {
  if (s && s->size && now >= s->started && now - s->started > 3000) {
    volatile uint8_t *p = (volatile uint8_t *)s;
    for (size_t n = 0; n < sizeof(*s); n++)
      p[n] = 0;
    return true;
  }
  return false;
}
unsigned update_progress_percent(uint32_t received, uint32_t total) {
  if (!total)
    return 0;
  return received >= total ? 100 : (uint64_t)received * 100 / total;
}
bool sensor_configure(void *ctx, sensor_write_fn write, sensor_read_fn read,
                      sensor_delay_fn delay) {
  uint8_t value;
  if (!write || !read || !delay || read(ctx, 0, &value) || value != 0xa0)
    return false;
  if (write(ctx, 0x3d, 0))
    return false;
  delay(ctx, 25);
  if (read(ctx, 0x3d, &value) || value != 0)
    return false;
  /* m/s^2, dps, degrees, Celsius, Windows orientation convention. */
  if (write(ctx, 0x3b, 0) || read(ctx, 0x3b, &value) || value != 0)
    return false;
  if (write(ctx, 0x3d, 0x0c))
    return false;
  delay(ctx, 25);
  return !read(ctx, 0x3d, &value) && value == 0x0c;
}

int portal_request_accept(uint32_t previous, uint32_t incoming,
                          bool same_payload) {
  if (!incoming)
    return 1;
  if (incoming < previous)
    return -1;
  if (incoming == previous)
    return same_payload ? 0 : -1;
  return 1;
}
