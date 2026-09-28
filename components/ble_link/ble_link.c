#include "ble_link.h"
#include "demo.h"
#include "telemetry_v1.h"
#include "telemetry_cockpit.h"
#include "telemetry_lighting.h"
#include "wheel_core.h"

#include "esp_random.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs.h"
#include <string.h>
void ble_store_config_init(void);
#include "freertos/semphr.h"
#include "host/ble_sm.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"
static const ble_uuid128_t svc = BLE_UUID128_INIT(
    0x19, 0x09, 0x90, 0xe9, 0x01, 0, 0, 0x80, 0, 0x10, 0, 0, 1, 0, 0, 0x90);
static const ble_uuid128_t chr = BLE_UUID128_INIT(
    0x19, 0x09, 0x90, 0xe9, 0x01, 0, 0, 0x80, 0, 0x10, 0, 0, 2, 0, 0, 0x90);
static uint32_t pair_passkey;
static const ble_uuid128_t app_svc = BLE_UUID128_INIT(
    0x19, 0x09, 0x90, 0xe9, 0x01, 0, 0, 0x80, 0, 0x10, 0, 0, 3, 0, 0, 0x90);
static const ble_uuid128_t app_chr = BLE_UUID128_INIT(
    0x19, 0x09, 0x90, 0xe9, 0x01, 0, 0, 0x80, 0, 0x10, 0, 0, 4, 0, 0, 0x90);
static const ble_uuid128_t cockpit_chr = BLE_UUID128_INIT(
    0x19, 0x09, 0x90, 0xe9, 0x01, 0, 0, 0x80, 0, 0x10, 0, 0, 5, 0, 0, 0x90);
static const ble_uuid128_t lighting_chr = BLE_UUID128_INIT(
    0x19, 0x09, 0x90, 0xe9, 0x01, 0, 0, 0x80, 0, 0x10, 0, 0, 6, 0, 0, 0x90);
static uint16_t app_handle, cockpit_handle, lighting_handle;
static uint8_t lighting_packet[LIGHTING_PACKET_SIZE];
static telemetry_v1_tracker_t lighting_tracker;
static bool app_lighting, next_lighting;
static uint8_t cockpit_packet[COCKPIT_PACKET_SIZE];
static bool app_cockpit;
static uint8_t telemetry[16];
static uint32_t telemetry_sequence;
static telemetry_v1_tracker_t telemetry_tracker;
static uint32_t app_request, app_session;
static uint64_t app_requested, app_polled;
static bool app_pending, recovery_busy, app_discovered;
static bool central, secure, have_peer, ready;
static ble_addr_t approved;
static uint16_t conn = BLE_HS_CONN_HANDLE_NONE, handle;
static QueueHandle_t incoming;
static SemaphoreHandle_t completed;
static volatile int result;
static uint32_t operation, connection_nonce, host_cycles;
static uint64_t host_heartbeat;
static struct ble_npl_callout heartbeat_callout;
static uint64_t connected_at;
static uint8_t frozen[128];
static size_t frozen_n;
static recovery_assembly_t assembly;
static uint8_t received[152], status_data[128];
static size_t received_n, status_n;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static void publish(demo_state_t *s, void *a) {
  (void)a;
  s->link_secure = secure;
  if (!secure) {
    s->app_compatible = false;
    s->telemetry_source = DEMO_SOURCE_NONE;
    s->telemetry_received = 0;
    s->telemetry_valid = 0;
  }
  if (!secure || s->maintenance || s->writing ||
      !telemetry_is_fresh(true, true, s->lights_received, demo_ms())) {
    s->lights_valid = s->lights_on = s->lights_source = 0;
    if (!secure) s->lights_received = 0;
  }
  s->telemetry_fresh = !s->maintenance && !s->writing &&
      telemetry_is_fresh(secure, s->app_compatible,
                         s->telemetry_received, demo_ms());
}
static bool authenticated(uint16_t c) {
  struct ble_gap_conn_desc d;
  return secure && ble_gap_conn_find(c, &d) == 0 && d.sec_state.encrypted &&
         d.sec_state.authenticated && d.sec_state.bonded && have_peer &&
         memcmp(&d.peer_id_addr, &approved, sizeof(approved)) == 0;
}
static int access_cb(uint16_t c, uint16_t h, struct ble_gatt_access_ctxt *ctx,
                     void *a) {
  (void)h;
  (void)a;
  if (!authenticated(c))
    return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
  if (ctx->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    if (ctx->offset == 0) {
      portENTER_CRITICAL(&lock);
      memcpy(frozen, status_data, status_n);
      frozen_n = status_n;
      if (frozen_n >= 56)
        for (unsigned i = 0; i < 4; i++)
          frozen[52 + i] = connection_nonce >> (8 * i);
      portEXIT_CRITICAL(&lock);
    }
    int rc = os_mbuf_append(ctx->om, frozen, frozen_n);
    return rc ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
  }
  uint8_t p[20];
  uint16_t n = 0;
  if (ble_hs_mbuf_to_flat(ctx->om, p, sizeof(p), &n) || n < 3)
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  int state = recovery_fragment(&assembly, p, n, demo_ms());
  demo_clear(p, sizeof(p));
  if (state < 0)
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  if (state == 1) {
    recovery_message_t m = {.size = assembly.size};
    memcpy(m.bytes, assembly.data, m.size);
    int valid = recovery_validate(m.bytes, m.size, true, true, true);
    demo_clear(&assembly, sizeof(assembly));
    bool sent = !valid && xQueueSend(incoming, &m, 0) == pdTRUE;
    demo_clear(&m, sizeof(m));
    if (!sent)
      return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  return 0;
}
static int app_access(uint16_t c, uint16_t h, struct ble_gatt_access_ctxt *ctx,
                      void *a) {
  (void)h;
  (void)a;
  if (!authenticated(c))
    return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
  uint8_t copy[16];
  portENTER_CRITICAL(&lock);
  memcpy(copy, telemetry, sizeof(copy));
  portEXIT_CRITICAL(&lock);
  return os_mbuf_append(ctx->om, copy, sizeof(copy))
             ? BLE_ATT_ERR_INSUFFICIENT_RES
             : 0;
}
static int cockpit_access(uint16_t c, uint16_t h,
                          struct ble_gatt_access_ctxt *ctx, void *a) {
  (void)h; (void)a;
  if (!authenticated(c)) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
  uint8_t copy[COCKPIT_PACKET_SIZE];
  portENTER_CRITICAL(&lock);
  memcpy(copy, cockpit_packet, sizeof(copy));
  portEXIT_CRITICAL(&lock);
  return os_mbuf_append(ctx->om, copy, sizeof(copy))
             ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
}
static int lighting_access(uint16_t c, uint16_t h,
                          struct ble_gatt_access_ctxt *ctx, void *a) {
  (void)h; (void)a;
  if (!authenticated(c)) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
  uint8_t copy[LIGHTING_PACKET_SIZE];
  portENTER_CRITICAL(&lock);
  memcpy(copy, lighting_packet, sizeof(copy));
  portEXIT_CRITICAL(&lock);
  return os_mbuf_append(ctx->om, copy, sizeof(copy))
             ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
}
static const struct ble_gatt_svc_def services[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &svc.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {.uuid = &chr.u,
              .access_cb = access_cb,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                       BLE_GATT_CHR_F_READ_AUTHEN | BLE_GATT_CHR_F_WRITE_AUTHEN,
              .val_handle = &handle},
             {0}}},
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &app_svc.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {.uuid = &app_chr.u,
              .access_cb = app_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_AUTHEN},
             {.uuid = &cockpit_chr.u,
              .access_cb = cockpit_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_AUTHEN},
             {.uuid = &lighting_chr.u,
              .access_cb = lighting_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_AUTHEN},
             {0}}},
    {0}};
static int gap(struct ble_gap_event *, void *);
static void seek(void) {
  if (central) {
    struct ble_gap_disc_params p = {
        .passive = 0, .itvl = 0x80, .window = 0x30, .filter_duplicates = 1};
    ble_gap_disc(0, 5000, &p, gap, NULL);
  } else {
    struct ble_hs_adv_fields f = {.flags = BLE_HS_ADV_F_DISC_GEN |
                                           BLE_HS_ADV_F_BREDR_UNSUP,
                                  .name = (uint8_t *)"BMW-Gateway",
                                  .name_len = 11,
                                  .name_is_complete = 1};
    ble_gap_adv_set_fields(&f);
    struct ble_gap_adv_params p = {.conn_mode = BLE_GAP_CONN_MODE_UND,
                                   .disc_mode = BLE_GAP_DISC_MODE_GEN};
    ble_gap_adv_start(0, NULL, BLE_HS_FOREVER, &p, gap, NULL);
  }
}
static int services_found(uint16_t, const struct ble_gatt_error *,
                          const struct ble_gatt_svc *, void *);
static int chars(uint16_t c, const struct ble_gatt_error *e,
                 const struct ble_gatt_chr *v, void *a) {
  if (!e->status) {
    if (ble_uuid_cmp(&v->uuid.u, &chr.u) == 0)
      handle = v->val_handle;
    else if (ble_uuid_cmp(&v->uuid.u, &app_chr.u) == 0)
      app_handle = v->val_handle;
    else if (ble_uuid_cmp(&v->uuid.u, &cockpit_chr.u) == 0)
      cockpit_handle = v->val_handle;
    else if (ble_uuid_cmp(&v->uuid.u, &lighting_chr.u) == 0)
      lighting_handle = v->val_handle;
  } else if (e->status == BLE_HS_EDONE && !a)
    ble_gattc_disc_svc_by_uuid(c, &app_svc.u, services_found, (void *)1);
  else if (e->status == BLE_HS_EDONE && a)
    app_discovered = true;
  return 0;
}
static int services_found(uint16_t c, const struct ble_gatt_error *e,
                          const struct ble_gatt_svc *v, void *a) {
  if (!e->status)
    ble_gattc_disc_all_chrs(c, v->start_handle, v->end_handle, chars, a);
  return 0;
}
static int gap(struct ble_gap_event *e, void *a) {
  (void)a;
  switch (e->type) {
  case BLE_GAP_EVENT_DISC: {
    struct ble_hs_adv_fields f;
    if (!ble_hs_adv_parse_fields(&f, e->disc.data, e->disc.length_data) &&
        f.name_len == 11 && !memcmp(f.name, "BMW-Gateway", 11)) {
      ble_gap_disc_cancel();
      ble_gap_connect(0, &e->disc.addr, 5000, NULL, gap, NULL);
    }
    break;
  }
  case BLE_GAP_EVENT_CONNECT:
    if (!e->connect.status) {
      conn = e->connect.conn_handle;
      connected_at = demo_ms();
      connection_nonce = esp_random();
      if (!connection_nonce)
        connection_nonce = 1;
      if (central)
        ble_gap_security_initiate(conn);
    } else
      conn = BLE_HS_CONN_HANDLE_NONE;
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    conn = BLE_HS_CONN_HANDLE_NONE;
    secure = false;
    connection_nonce = 0;
    recovery_message_t discarded;
    while (xQueueReceive(incoming, &discarded, 0) == pdTRUE)
      demo_clear(&discarded, sizeof(discarded));
    portENTER_CRITICAL(&lock);
    app_pending = false;
    app_discovered = false;
    app_request++;
    memset(&telemetry_tracker, 0, sizeof(telemetry_tracker));
    memset(&lighting_tracker, 0, sizeof(lighting_tracker));
    next_lighting = false;
    portEXIT_CRITICAL(&lock);
    app_handle = cockpit_handle = lighting_handle = 0;
    handle = central ? 0 : handle;
    demo_clear(&assembly, sizeof(assembly));
    demo_edit(publish, NULL);
    break;
  case BLE_GAP_EVENT_PASSKEY_ACTION: {
    struct ble_sm_io io = {.action = e->passkey.params.action,
                           .passkey = pair_passkey};
    if (!pair_passkey || (!have_peer && demo_ms() > 120000)) {
      ble_gap_terminate(conn, BLE_ERR_AUTH_FAIL);
      break;
    }
    if (io.action == BLE_SM_IOACT_INPUT || io.action == BLE_SM_IOACT_DISP)
      ble_sm_inject_io(conn, &io);
    else
      ble_gap_terminate(conn, BLE_ERR_AUTH_FAIL);
    break;
  }
  case BLE_GAP_EVENT_REPEAT_PAIRING:
    return BLE_GAP_REPEAT_PAIRING_IGNORE;
  default:
    break;
  }
  return 0;
}
static void host_tick(struct ble_npl_event *event) {
  (void)event;
  portENTER_CRITICAL(&lock);
  host_heartbeat = demo_ms();
  host_cycles++;
  portEXIT_CRITICAL(&lock);
  recovery_fragment_expire(&assembly, demo_ms());
  ble_npl_callout_reset(&heartbeat_callout, ble_npl_time_ms_to_ticks32(250));
}
static void synced(void) {
  ble_hs_util_ensure_addr(0);
  ready = true;
  ble_npl_callout_init(&heartbeat_callout, nimble_port_get_dflt_eventq(),
                       host_tick, NULL);
  ble_npl_callout_reset(&heartbeat_callout, ble_npl_time_ms_to_ticks32(250));
}
static void host(void *a) {
  (void)a;
  nimble_port_run();
  nimble_port_freertos_deinit();
}
static void supervise(void *a) {
  (void)a;
  nvs_handle_t n;
  ESP_ERROR_CHECK(nvs_open("recovery", NVS_READWRITE, &n));
  size_t len = sizeof(approved);
  have_peer = nvs_get_blob(n, "peer", &approved, &len) == ESP_OK &&
              len == sizeof(approved);
  uint64_t next_seek = 0;
  for (;;) {
    if (ready && conn == BLE_HS_CONN_HANDLE_NONE && demo_ms() >= next_seek) {
      seek();
      next_seek = demo_ms() + 1000;
    }
    if (recovery_auth_expired(conn != BLE_HS_CONN_HANDLE_NONE,
                              secure && (!central || handle), connected_at,
                              demo_ms())) {
      ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    if (conn != BLE_HS_CONN_HANDLE_NONE && !secure) {
      struct ble_gap_conn_desc d;
      if (!ble_gap_conn_find(conn, &d) && d.sec_state.encrypted &&
          d.sec_state.authenticated && d.sec_state.bonded) {
        if (!have_peer && demo_ms() < 120000) {
          approved = d.peer_id_addr;
          if (nvs_set_blob(n, "peer", &approved, sizeof(approved)) == ESP_OK &&
              nvs_commit(n) == ESP_OK)
            have_peer = true;
        }
        if (have_peer &&
            !memcmp(&d.peer_id_addr, &approved, sizeof(approved))) {
          secure = true;
          if (central)
            ble_gattc_disc_svc_by_uuid(conn, &svc.u, services_found, NULL);
          demo_edit(publish, NULL);
        } else
          ble_gap_terminate(conn, BLE_ERR_AUTH_FAIL);
      }
    }
    if (!central) {
      uint32_t now = (uint32_t)demo_ms();
      cockpit_sample_t sample;
      cockpit_demo(now, &sample);
      lighting_sample_t lights;
      lighting_demo(now, &lights);
      portENTER_CRITICAL(&lock);
      telemetry_v1_encode(telemetry, ++telemetry_sequence, sample.rpm, now);
      cockpit_encode(cockpit_packet, telemetry_sequence, now, &sample);
      lighting_encode(lighting_packet, telemetry_sequence, now, &lights);
      portEXIT_CRITICAL(&lock);
    }
    demo_edit(publish, NULL);
    if (central)
      ble_link_poll_telemetry();
    vTaskDelay(pdMS_TO_TICKS(central && lighting_handle ? 50 : 100));
  }
}
void ble_link_start(bool wheel) {
  central = wheel;
  nvs_handle_t identity;
  if (nvs_open("pairing", NVS_READONLY, &identity) == ESP_OK) {
    nvs_get_u32(identity, "passkey", &pair_passkey);
    nvs_close(identity);
  }
  incoming = xQueueCreate(4, sizeof(recovery_message_t));
  completed = xSemaphoreCreateBinary();
  ESP_ERROR_CHECK(nimble_port_init());
  ble_hs_cfg.sync_cb = synced;
  ble_hs_cfg.sm_io_cap =
      wheel ? BLE_HS_IO_KEYBOARD_ONLY : BLE_HS_IO_DISPLAY_ONLY;
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_mitm = 1;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_our_key_dist =
      BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist = ble_hs_cfg.sm_our_key_dist;
  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_store_config_init();
  if (!wheel) {
    assert(!ble_gatts_count_cfg(services));
    assert(!ble_gatts_add_svcs(services));
  }
  nimble_port_freertos_init(host);
  configASSERT(xTaskCreatePinnedToCore(supervise, "ble_supervise", 4096, NULL,
                                       3, NULL, 0) == pdPASS);
}
bool ble_link_secure(void) {
  return conn != BLE_HS_CONN_HANDLE_NONE && authenticated(conn) &&
         (!central || handle);
}
static int written(uint16_t c, const struct ble_gatt_error *e,
                   struct ble_gatt_attr *v, void *a) {
  (void)c;
  (void)v;
  if ((uint32_t)(uintptr_t)a != operation)
    return 0;
  result = e->status;
  xSemaphoreGive(completed);
  return 0;
}
/* Serialize ATT application reads against recovery procedures. Set the gate
 * before waiting so a 100ms poll cannot start between observing idle and the
 * first recovery request. Existing recovery callers remain one worker owner. */
static bool recovery_enter(void) {
  portENTER_CRITICAL(&lock);
  recovery_busy = true;
  portEXIT_CRITICAL(&lock);
  uint64_t until = demo_ms() + 2500;
  for (;;) {
    portENTER_CRITICAL(&lock);
    bool pending = app_pending;
    portEXIT_CRITICAL(&lock);
    if (!pending) return true;
    if (demo_ms() >= until) {
      portENTER_CRITICAL(&lock);
      recovery_busy = false;
      portEXIT_CRITICAL(&lock);
      ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
static void recovery_leave(void) {
  portENTER_CRITICAL(&lock);
  recovery_busy = false;
  portEXIT_CRITICAL(&lock);
}
bool ble_link_send(const recovery_message_t *m) {
  if (!central || !ble_link_secure() || m->size > 152)
    return false;
  if (!recovery_enter()) return false;
  for (size_t off = 0; off < m->size;) {
    uint8_t p[20] = {(uint8_t)off, (uint8_t)m->size};
    size_t n = m->size - off;
    if (n > 18)
      n = 18;
    memcpy(p + 2, m->bytes + off, n);
    xSemaphoreTake(completed, 0);
    if (ble_gattc_write_flat(conn, handle, p, n + 2, written,
                             (void *)(uintptr_t)(++operation)) ||
        !xSemaphoreTake(completed, pdMS_TO_TICKS(2500)) || result) {
      operation++;
      secure = false;
      ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
      recovery_leave();
      return false;
    }
    off += n;
  }
  recovery_leave();
  return true;
}
bool ble_link_receive(recovery_message_t *m) {
  return xQueueReceive(incoming, m, 0) == pdTRUE;
}
void ble_link_status(const uint8_t *d, size_t n) {
  if (n > sizeof(status_data))
    return;
  portENTER_CRITICAL(&lock);
  memcpy(status_data, d, n);
  status_n = n;
  portEXIT_CRITICAL(&lock);
}
static int read_done(uint16_t c, const struct ble_gatt_error *e,
                     struct ble_gatt_attr *v, void *a) {
  (void)c;
  if ((uint32_t)(uintptr_t)a != operation)
    return BLE_HS_EAPP;
  if (!e->status && v) {
    uint16_t n = 0;
    if (v->offset != received_n ||
        ble_hs_mbuf_to_flat(v->om, received + received_n,
                            sizeof(received) - received_n, &n)) {
      result = -1;
      xSemaphoreGive(completed);
      return BLE_HS_EINVAL;
    }
    received_n += n;
    return 0;
  }
  result = e->status == BLE_HS_EDONE ? 0 : e->status;
  xSemaphoreGive(completed);
  return 0;
}
static bool read_attribute(uint16_t attribute, uint8_t *d, size_t *n) {
  if (!central || !ble_link_secure())
    return false;
  received_n = 0;
  xSemaphoreTake(completed, 0);
  if (ble_gattc_read_long(conn, attribute, 0, read_done,
                          (void *)(uintptr_t)(++operation)) ||
      !xSemaphoreTake(completed, pdMS_TO_TICKS(2500)) || result ||
      received_n > *n) {
    operation++;
    secure = false;
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    return false;
  }
  memcpy(d, received, received_n);
  *n = received_n;
  return true;
}

bool ble_link_read(uint8_t *d, size_t *n) {
  if (!recovery_enter()) return false;
  bool ok = read_attribute(handle, d, n);
  recovery_leave();
  return ok;
}
static void telemetry_state(demo_state_t *s, void *a) {
  const telemetry_v1_sample_t *p = a;
  if (!s->link_secure || s->maintenance || s->writing)
    return;
  s->app_compatible = true;
  s->telemetry_source = DEMO_SOURCE_GATEWAY_DEMO;
  s->telemetry_received = p->received;
  s->ble_sequence = p->sequence;
  s->ble_rpm = p->rpm;
  s->telemetry_valid = COCKPIT_VALID_RPM;
  s->speed_dkph = 0; s->gear = 0; s->coolant_c = s->oil_c = 0;
  s->closure_open = 0;
  s->telemetry_fresh = telemetry_is_fresh(true, true, p->received, demo_ms());
}
static void cockpit_state(demo_state_t *s, void *a) {
  const cockpit_sample_t *p = a;
  if (!s->link_secure || s->maintenance || s->writing) return;
  s->app_compatible = true;
  s->telemetry_source = p->source == 2 ? DEMO_SOURCE_VEHICLE : DEMO_SOURCE_GATEWAY_DEMO;
  s->telemetry_received = p->received;
  s->ble_sequence = p->sequence; s->ble_rpm = p->rpm;
  s->telemetry_valid = p->valid; s->speed_dkph = p->speed_dkph;
  s->gear = p->gear; s->coolant_c = p->coolant_c; s->oil_c = p->oil_c;
  s->closure_open = p->closure_open;
  s->telemetry_fresh = telemetry_is_fresh(true, true, p->received, demo_ms());
}
static void lighting_state(demo_state_t *s, void *a) {
  const lighting_sample_t *p = a;
  if (!s->link_secure || s->maintenance || s->writing) return;
  s->lights_valid = p->valid; s->lights_on = p->on; s->lights_source = p->source;
  s->lights_sequence = p->sequence; s->lights_received = p->received;
}
static void telemetry_incompatible(demo_state_t *s, void *a) {
  (void)a;
  s->app_compatible = false;
  s->telemetry_fresh = false;
  s->telemetry_source = DEMO_SOURCE_NONE;
  s->telemetry_valid = 0;
}
/* Application reads have their own callback/token; they never touch the
 * recovery operation, completion semaphore or response buffers. */
static int app_read_done(uint16_t c, const struct ble_gatt_error *e,
                         struct ble_gatt_attr *v, void *a) {
  uint32_t token = (uint32_t)(uintptr_t)a;
  uint8_t packet[COCKPIT_PACKET_SIZE];
  uint16_t length = 0;
  bool wire_ok = !e->status && v && v->offset == 0 &&
                 !ble_hs_mbuf_to_flat(v->om, packet, sizeof(packet), &length);
  telemetry_v1_sample_t sample;
  cockpit_sample_t cockpit;
  lighting_sample_t lighting;
  bool accepted = false, extended = false, lights = false, incompatible = false;
  portENTER_CRITICAL(&lock);
  if (app_pending && token == app_request) {
    app_pending = false;
    if (c == conn && app_session == connection_nonce && secure && wire_ok) {
      extended = app_cockpit;
      lights = app_lighting;
      if (lights) {
        accepted = lighting_accept(&lighting_tracker, app_session, packet,
                                   length, app_requested, demo_ms(), &lighting);
      } else if (extended) {
        incompatible = length == COCKPIT_PACKET_SIZE &&
                       (packet[0] != 1 || (packet[1] != 1 && packet[1] != 2));
        accepted = cockpit_accept(&telemetry_tracker, app_session, packet,
                                  length, app_requested, demo_ms(), &cockpit);
      } else {
        incompatible = length == 16 &&
                       (packet[0] != 1 || packet[1] || packet[2] || packet[3]);
        accepted = telemetry_v1_accept(&telemetry_tracker, app_session,
                                      packet, length, app_requested, demo_ms(),
                                      &sample);
      }
    }
  }
  portEXIT_CRITICAL(&lock);
  if (accepted && lights)
    demo_edit(lighting_state, &lighting);
  else if (accepted && extended)
    demo_edit(cockpit_state, &cockpit);
  else if (accepted)
    demo_edit(telemetry_state, &sample);
  else if (incompatible)
    demo_edit(telemetry_incompatible, NULL);
  return 0;
}
void ble_link_poll_telemetry(void) {
  if (!central || !app_handle || !app_discovered || !ble_link_secure())
    return;
  demo_state_t state;
  demo_get(&state);
  /* Recovery/OTA owns the link in maintenance; no new application operation. */
  if (state.maintenance || state.writing)
    return;
  uint64_t now = demo_ms();
  uint32_t token = 0;
  uint16_t attribute = 0;
  bool expired = false;
  portENTER_CRITICAL(&lock);
  if (app_pending) {
    expired = !recovery_busy && now - app_requested > 2500;
  } else if (!recovery_busy && now - app_polled >= (lighting_handle ? 50u : 100u)) {
    app_pending = true;
    app_requested = app_polled = now;
    app_session = connection_nonce;
    app_lighting = lighting_handle && next_lighting;
    next_lighting = lighting_handle && !app_lighting;
    app_cockpit = !app_lighting && cockpit_handle != 0;
    attribute = app_lighting ? lighting_handle : app_cockpit ? cockpit_handle : app_handle;
    token = ++app_request;
    if (!token) token = ++app_request;
  }
  portEXIT_CRITICAL(&lock);
  if (expired) {
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
  } else if (token && ble_gattc_read(conn, attribute, app_read_done,
                                   (void *)(uintptr_t)token)) {
    portENTER_CRITICAL(&lock);
    if (token == app_request) app_pending = false;
    portEXIT_CRITICAL(&lock);
  }
}
bool ble_link_ready(void) { return ready; }

uint32_t ble_link_session(void) { return connection_nonce; }
void ble_link_health(uint64_t *heartbeat, uint32_t *cycles) {
  portENTER_CRITICAL(&lock);
  *heartbeat = host_heartbeat;
  *cycles = host_cycles;
  portEXIT_CRITICAL(&lock);
}
