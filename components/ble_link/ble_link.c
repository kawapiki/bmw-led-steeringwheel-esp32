#include "ble_link.h"
#include "demo.h"
#include "wheel_core.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
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
static uint16_t app_handle;
static uint32_t telemetry[4] = {1, 0, 800, 0};
static bool central, secure, have_peer, ready;
static ble_addr_t approved;
static uint16_t conn = BLE_HS_CONN_HANDLE_NONE, handle;
static QueueHandle_t incoming;
static SemaphoreHandle_t completed;
static volatile int result;
static uint32_t operation;
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
  uint32_t copy[4];
  portENTER_CRITICAL(&lock);
  memcpy(copy, telemetry, sizeof(copy));
  portEXIT_CRITICAL(&lock);
  return os_mbuf_append(ctx->om, copy, sizeof(copy))
             ? BLE_ATT_ERR_INSUFFICIENT_RES
             : 0;
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
  } else if (e->status == BLE_HS_EDONE && !a)
    ble_gattc_disc_svc_by_uuid(c, &app_svc.u, services_found, (void *)1);
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
      if (central)
        ble_gap_security_initiate(conn);
    } else
      conn = BLE_HS_CONN_HANDLE_NONE;
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    conn = BLE_HS_CONN_HANDLE_NONE;
    secure = false;
    recovery_message_t discarded;
    while (xQueueReceive(incoming, &discarded, 0) == pdTRUE)
      demo_clear(&discarded, sizeof(discarded));
    app_handle = 0;
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
static void synced(void) {
  ble_hs_util_ensure_addr(0);
  ready = true;
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
  for (;;) {
    if (ready && conn == BLE_HS_CONN_HANDLE_NONE)
      seek();
    if (conn != BLE_HS_CONN_HANDLE_NONE && central && (!secure || !handle) &&
        demo_ms() - connected_at > 15000) {
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
    portENTER_CRITICAL(&lock);
    telemetry[1]++;
    telemetry[2] = 800 + (demo_ms() % 12000) * 6200 / 12000;
    telemetry[3] = demo_ms();
    portEXIT_CRITICAL(&lock);
    vTaskDelay(pdMS_TO_TICKS(1000));
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
bool ble_link_send(const recovery_message_t *m) {
  if (!central || !ble_link_secure() || m->size > 152)
    return false;
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
      return false;
    }
    off += n;
  }
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
  return read_attribute(handle, d, n);
}
static void telemetry_state(demo_state_t *s, void *a) {
  uint32_t *p = a;
  s->app_compatible = p[0] == 1;
  s->ble_sequence = p[1];
  s->ble_rpm = p[2];
}
void ble_link_poll_telemetry(void) {
  if (!central || !app_handle || !ble_link_secure())
    return;
  uint32_t t[4];
  size_t n = sizeof(t);
  if (read_attribute(app_handle, (uint8_t *)t, &n) && n == sizeof(t))
    demo_edit(telemetry_state, t);
}
bool ble_link_ready(void) { return ready; }
