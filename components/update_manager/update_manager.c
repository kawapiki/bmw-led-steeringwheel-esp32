#include "update_manager.h"
#include "ble_link.h"
#include "cJSON.h"
#include "demo.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_netif_sntp.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_system.h"
#include "nvs.h"
#include "psa/crypto.h"
#include "release_public.h"
#include "service_wifi.h"
#include "wheel_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define BASE                                                                   \
  "https://github.com/kawapiki/bmw-led-steeringwheel-esp32/releases/download/" \
  "system-r"
typedef struct {
  uint32_t release, size[2];
  uint8_t hash[2][32];
} manifest_t;
typedef struct {
  uint32_t schema, phase, release, size;
  uint64_t tx;
  uint8_t hash[32];
  uint32_t target_address;
} journal_t;
static bool wheel, offer, cancelled, journal_ready;
static uint32_t session, request, last_request, remote_session;
static uint64_t transaction;
static manifest_t candidate;
static journal_t journal;
static nvs_handle_t nvs;
static void textstate(demo_state_t *s, void *a) {
  snprintf(s->update, sizeof(s->update), "%s", (char *)a);
  s->offer = offer;
  s->candidate_release = offer ? candidate.release : 0;
}
static void say(const char *s) { demo_edit(textstate, (void *)s); }
static void progress(demo_state_t *s, void *a) { s->progress = *(unsigned *)a; }
static void writing(demo_state_t *s, void *a) { s->writing = *(bool *)a; }
static uint32_t le32(const uint8_t *p) {
  return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = v >> (8 * i);
}
static uint64_t le64(const uint8_t *p) {
  return le32(p) | ((uint64_t)le32(p + 4) << 32);
}
static void put64(uint8_t *p, uint64_t v) {
  put32(p, v);
  put32(p + 4, v >> 32);
}
static bool save(void) {
  return nvs_set_blob(nvs, "journal", &journal, sizeof(journal)) == ESP_OK &&
         nvs_commit(nvs) == ESP_OK;
}
static bool tls_time(void) {
  if (time(NULL) > 1735689600)
    return true;
  esp_sntp_config_t c = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&c);
  return esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) == ESP_OK &&
         time(NULL) > 1735689600;
}
static esp_http_client_handle_t open_url(const char *url) {
  if (strncmp(url, "https://", 8))
    return NULL;
  esp_http_client_config_t c = {.url = url,
                                .crt_bundle_attach = esp_crt_bundle_attach,
                                .timeout_ms = 12000,
                                .disable_auto_redirect = true,
                                .buffer_size = 2048,
                                .user_agent = "BMW-Demo/1"};
  esp_http_client_handle_t h = esp_http_client_init(&c);
  if (!h)
    return NULL;
  for (int i = 0; i < 5; i++) {
    if (esp_http_client_open(h, 0) != ESP_OK ||
        esp_http_client_fetch_headers(h) < 0)
      break;
    int status = esp_http_client_get_status_code(h);
    if (status == 200)
      return h;
    if (status < 300 || status > 399)
      break;
    /* IDF validates HTTPS redirect schemes; independently refuse a non-HTTPS
     * Location. */
    if (esp_http_client_set_redirection(h) != ESP_OK) {
      break;
    }
    esp_http_client_close(h);
  }

  esp_http_client_cleanup(h);
  return NULL;
}
static int fetch(const char *url, uint8_t *out, size_t cap) {
  esp_http_client_handle_t h = open_url(url);
  if (!h)
    return -1;
  size_t used = 0;
  int n;
  while (used < cap &&
         (n = esp_http_client_read(h, (char *)out + used, cap - used)) > 0)
    used += n;
  char extra;
  bool ok = used < cap || esp_http_client_read(h, &extra, 1) == 0;
  ok = ok && esp_http_client_is_complete_data_received(h);
  esp_http_client_close(h);
  esp_http_client_cleanup(h);
  return ok ? (int)used : -1;
}
static bool get_manifest(uint32_t release, manifest_t *m) {
  char url[180];
  snprintf(url, sizeof(url), BASE "%lu/manifest.bin", (unsigned long)release);
  uint8_t data[480];
  if (fetch(url, data, sizeof(data)) != 480 ||
      !manifest_layout_valid(data, sizeof(data)) || le32(data + 4) != release)
    return false;
  psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_type(&attr, PSA_KEY_TYPE_RSA_PUBLIC_KEY);
  psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_VERIFY_HASH);
  psa_set_key_algorithm(&attr, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256));
  psa_key_id_t key;
  uint8_t hash[32];
  size_t len;
  if (psa_import_key(&attr, release_public_key, sizeof(release_public_key),
                     &key) != PSA_SUCCESS)
    return false;
  bool good = psa_hash_compute(PSA_ALG_SHA_256, data, 96, hash, sizeof(hash),
                               &len) == PSA_SUCCESS &&
              psa_verify_hash(key, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256),
                              hash, 32, data + 96, 384) == PSA_SUCCESS;
  psa_destroy_key(key);
  if (!good || le32(data + 80) != 1 || le32(data + 84) != 1 ||
      le32(data + 88) != 1 || le32(data + 92) == 0)
    return false;
  m->release = release;
  m->size[0] = le32(data + 8);
  m->size[1] = le32(data + 12);
  memcpy(m->hash[0], data + 16, 32);
  memcpy(m->hash[1], data + 48, 32);
  return m->size[0] > 256 && m->size[0] <= 0x600000 && m->size[1] > 256 &&
         m->size[1] <= 0x1e0000;
}
static bool installed_hash(const journal_t *j) {
  const esp_partition_t *p = esp_ota_get_running_partition();
  if (!p || p->address != j->target_address || j->size > p->size)
    return false;
  psa_hash_operation_t h = PSA_HASH_OPERATION_INIT;
  if (psa_hash_setup(&h, PSA_ALG_SHA_256) != PSA_SUCCESS)
    return false;
  uint8_t b[1024], digest[32];
  size_t size;
  for (uint32_t off = 0; off < j->size;) {
    size_t n = j->size - off;
    if (n > sizeof(b))
      n = sizeof(b);
    if (esp_partition_read(p, off, b, n) != ESP_OK ||
        psa_hash_update(&h, b, n) != PSA_SUCCESS) {
      psa_hash_abort(&h);
      return false;
    }
    off += n;
  }
  return psa_hash_finish(&h, digest, sizeof(digest), &size) == PSA_SUCCESS &&
         !memcmp(digest, j->hash, 32);
}
void update_boot_validate(bool passed) {
  passed = passed && journal_ready && ble_link_ready();
  esp_ota_img_states_t state;
  const esp_partition_t *p = esp_ota_get_running_partition();
  if (esp_ota_get_state_partition(p, &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    if (journal.phase == 3 && !installed_hash(&journal))
      passed = false;
    if (passed)
      ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    else
      esp_ota_mark_app_invalid_rollback_and_reboot();
  }
}
static void publish_status(void) {
  uint8_t b[64] = {0};
  memcpy(b, "BST1", 4);
  put32(b + 4, journal.phase);
  put32(b + 8, journal.release);
  put64(b + 12, journal.tx);
  memcpy(b + 20, journal.hash, 32);
  put32(b + 52, session);
  put32(b + 56, request);
  put32(b + 60, 1);
  ble_link_status(b, sizeof(b));
}
static recovery_message_t packet(uint8_t op, size_t n) {
  recovery_message_t m = {.size = 24 + n};
  m.bytes[0] = 0xe9;
  m.bytes[1] = 0x90;
  m.bytes[2] = 1;
  m.bytes[3] = op;
  m.bytes[4] = n;
  put32(m.bytes + 8, session);
  put32(m.bytes + 12, ++request);
  put64(m.bytes + 16, transaction);
  return m;
}
static bool install(const manifest_t *m, uint64_t tx) {
  int index = wheel ? 0 : 1;
  const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
  if (!p || m->size[index] > p->size)
    return false;
  bool active = true;
  demo_edit(writing, &active);
  uint64_t start = demo_ms();
  if (wheel) {
    demo_state_t s;
    do {
      vTaskDelay(pdMS_TO_TICKS(10));
      demo_get(&s);
    } while (
        (s.input_heartbeat <= start || !s.io_quiet || demo_ms() - start < 25) &&
        demo_ms() - start < 1000);
    if (s.input_heartbeat <= start || !s.io_quiet) {
      active = false;
      demo_edit(writing, &active);
      return false;
    }
  }
  char url[180];
  snprintf(url, sizeof(url), BASE "%lu/%s.bin", (unsigned long)m->release,
           wheel ? "wheel" : "gateway");
  esp_http_client_handle_t http = open_url(url);
  if (!http)
    goto failure;
  int64_t total = esp_http_client_get_content_length(http);
  if (total != m->size[index]) {
    esp_http_client_cleanup(http);
    goto failure;
  }
  journal = (journal_t){.schema = 1,
                        .phase = 2,
                        .release = m->release,
                        .size = m->size[index],
                        .tx = tx,
                        .target_address = p->address};
  memcpy(journal.hash, m->hash[index], 32);
  if (!save()) {
    esp_http_client_cleanup(http);
    goto failure;
  }
  publish_status();
  esp_ota_handle_t ota;
  if (esp_ota_begin(p, OTA_WITH_SEQUENTIAL_WRITES, &ota) != ESP_OK) {
    esp_http_client_cleanup(http);
    goto failure;
  }
  psa_hash_operation_t hash = PSA_HASH_OPERATION_INIT;
  psa_hash_setup(&hash, PSA_ALG_SHA_256);
  uint8_t data[2048], digest[32];
  size_t digest_n;
  uint32_t count = 0;
  bool good = true;
  cancelled = false;
  while (count < m->size[index]) {
    intent_t intent;
    if (wheel && xQueueReceive(demo_intents, &intent, 0) == pdTRUE &&
        intent.kind == INTENT_CANCEL)
      cancelled = true;
    if (!wheel) {
      recovery_message_t control;
      if (ble_link_receive(&control)) {
        if (control.bytes[3] == 6 && control.size == 24 &&
            le64(control.bytes + 16) == tx)
          cancelled = true;
        demo_clear(&control, sizeof(control));
      }
    }
    if (cancelled) {
      good = false;
      break;
    }
    int n = esp_http_client_read(http, (char *)data, sizeof(data));
    if (n <= 0 || count + n > m->size[index] ||
        psa_hash_update(&hash, data, n) != PSA_SUCCESS ||
        esp_ota_write(ota, data, n) != ESP_OK) {
      good = false;
      break;
    }
    count += n;
    unsigned percent = (uint64_t)count * 100 / m->size[index];
    demo_edit(progress, &percent);
  }
  esp_http_client_close(http);
  esp_http_client_cleanup(http);
  good = good && count == m->size[index] &&
         psa_hash_finish(&hash, digest, sizeof(digest), &digest_n) ==
             PSA_SUCCESS &&
         !memcmp(digest, m->hash[index], 32);
  psa_hash_abort(&hash);
  if (!good) {
    esp_ota_abort(ota);
    goto failure;
  }
  if (esp_ota_end(ota) != ESP_OK)
    goto failure;
  esp_app_desc_t desc;
  if (esp_ota_get_partition_description(p, &desc) != ESP_OK ||
      strcmp(desc.project_name, wheel ? "wheel_demo" : "gateway_demo"))
    goto failure;
  journal.phase = 3;
  publish_status();
  if (!save() || esp_ota_set_boot_partition(p) != ESP_OK)
    goto failure;
  say("Verified; restarting");
  vTaskDelay(pdMS_TO_TICKS(150));
  esp_restart();
failure:
  active = false;
  demo_edit(writing, &active);
  journal.phase = 5;
  save();
  say("Update failed; original boot retained");
  return false;
}
static void search(void) {
  offer = false;
  say("Checking signed GitHub releases");
  if (!service_wifi_ready() || !tls_time()) {
    say("Network or time unavailable");
    return;
  }
  uint32_t floor = 0;
  nvs_get_u32(nvs, "release", &floor);
  manifest_t best = {0};
  uint32_t releases[15];
  size_t release_count = 0;
  char *json = malloc(24577);
  if (!json) {
    say("Not enough memory");
    return;
  }
  for (int page = 1; page <= 3; page++) {
    char url[160];
    snprintf(url, sizeof(url),
             "https://api.github.com/repos/kawapiki/"
             "bmw-led-steeringwheel-esp32/releases?per_page=5&page=%d",
             page);
    int n = fetch(url, (uint8_t *)json, 24576);
    if (n < 0) {
      free(json);
      say("Release listing failed / too large");
      return;
    }
    json[n] = 0;
    cJSON *list = cJSON_ParseWithLength(json, n);
    if (!cJSON_IsArray(list)) {
      cJSON_Delete(list);
      free(json);
      say("Invalid release listing");
      return;
    }
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list) {
      cJSON *draft = cJSON_GetObjectItemCaseSensitive(entry, "draft"),
            *pre = cJSON_GetObjectItemCaseSensitive(entry, "prerelease"),
            *tag = cJSON_GetObjectItemCaseSensitive(entry, "tag_name");
      if (!cJSON_IsFalse(draft) || !cJSON_IsBool(pre) || !cJSON_IsString(tag))
        continue;
      const char *t = tag->valuestring;
      if (strncmp(t, "system-r", 8))
        continue;
      char *tail;
      unsigned long release = strtoul(t + 8, &tail, 10);
      if (*tail || tail == t + 8 || release > UINT32_MAX || release <= floor ||
          release <= best.release)
        continue;
      if (release_count < 15)
        releases[release_count++] = release;
    }
    cJSON_Delete(list);
  }

  free(json);
  for (size_t i = 0; i < release_count; i++) {
    manifest_t m;
    if (releases[i] > best.release && get_manifest(releases[i], &m))
      best = m;
  }
  if (best.release) {
    candidate = best;
    offer = true;
    char text[96];
    snprintf(text, sizeof(text), "Release %lu ready. Hold K2.",
             (unsigned long)best.release);
    say(text);
  } else
    say("No newer authenticated release");
}

static bool status_matches(const uint8_t *s, size_t n, uint64_t tx) {
  return n == 64 && !memcmp(s, "BST1", 4) && le32(s + 8) == candidate.release &&
         le64(s + 12) == tx && !memcmp(s + 20, candidate.hash[1], 32);
}
static bool confirmed_status(const uint8_t *s, size_t n) {
  update_gate_t g = {.gateway_online = ble_link_secure(),
                     .gateway_image_valid = n == 64 && le32(s + 4) == 4,
                     .authenticated_status = ble_link_secure(),
                     .status_fresh = true,
                     .requested_release = candidate.release,
                     .requested_transaction = transaction};
  if (n != 64 || memcmp(s, "BST1", 4))
    return false;
  g.confirmed_release = le32(s + 8);
  g.confirmed_transaction = le64(s + 12);
  memcpy(g.confirmed_digest, s + 20, 32);
  memcpy(g.requested_digest, candidate.hash[1], 32);
  return update_wheel_allowed(&g);
}
static bool paired(void) {
  if (!ble_link_secure()) {
    say("Paired gateway unavailable; update stopped");
    return false;
  }
  bool resume = journal.phase == 1 && journal.release == candidate.release &&
                !memcmp(journal.hash, candidate.hash[1], 32);
  transaction =
      resume ? journal.tx : ((uint64_t)esp_random() << 32) | esp_random();
  if (!transaction)
    transaction = 1;
  uint8_t status[128];
  size_t size = sizeof(status);
  bool queried = ble_link_read(status, &size);
  if (queried && confirmed_status(status, size))
    return true;
  bool already_started = queried && status_matches(status, size, transaction) &&
                         (le32(status + 4) == 2 || le32(status + 4) == 3);
  /* Durable local intent precedes every remote mutation. User confirmation is
   * required to enter here. */
  journal = (journal_t){
      .schema = 1, .phase = 1, .release = candidate.release, .tx = transaction};
  memcpy(journal.hash, candidate.hash[1], 32);
  if (!save())
    return false;
  if (!already_started) {
    char ssid[33] = {0}, pass[64] = {0};
    if (!service_wifi_credentials(ssid, pass)) {
      demo_clear(pass, sizeof(pass));
      return false;
    }
    recovery_message_t m = packet(3, 2 + strlen(ssid) + strlen(pass));
    m.bytes[24] = strlen(ssid);
    m.bytes[25] = strlen(pass);
    memcpy(m.bytes + 26, ssid, strlen(ssid));
    memcpy(m.bytes + 26 + strlen(ssid), pass, strlen(pass));
    bool sent = ble_link_send(&m);
    demo_clear(&m, sizeof(m));
    demo_clear(pass, sizeof(pass));
    demo_clear(ssid, sizeof(ssid));
    if (!sent)
      return false;
    m = packet(4, 36);
    put32(m.bytes + 24, candidate.release);
    memcpy(m.bytes + 28, candidate.hash[1], 32);
    if (!ble_link_send(&m))
      return false;
    m = packet(5, 0);
    if (!ble_link_send(&m))
      return false;
  }
  say("Gateway first; waiting for verified boot");
  uint64_t until = demo_ms() + 300000;
  while (demo_ms() < until) {
    intent_t action;
    if (xQueueReceive(demo_intents, &action, 0) == pdTRUE &&
        action.kind == INTENT_CANCEL) {
      recovery_message_t stop = packet(6, 0);
      ble_link_send(&stop);
      journal.phase = 5;
      save();
      say("Paired update cancelled");
      return false;
    }
    size = sizeof(status);
    if (ble_link_read(status, &size) && confirmed_status(status, size))
      return true;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  say("Gateway pending; confirm again to resume");
  return false;
}
static void gateway_commands(void) {
  recovery_message_t m;
  while (ble_link_receive(&m)) {
    uint32_t s = le32(m.bytes + 8), r = le32(m.bytes + 12);
    uint64_t tx = le64(m.bytes + 16);
    uint8_t op = m.bytes[3];
    size_t n = m.size - 24;
    if (!s || !r || !tx || (s == remote_session && r <= last_request)) {
      demo_clear(&m, sizeof(m));
      continue;
    }
    if (s != remote_session) {
      remote_session = s;
      last_request = 0;
    }
    last_request = r;
    request = r;
    if (op == 3 && n >= 2) {
      char ssid[33] = {0}, pass[64] = {0};
      memcpy(ssid, m.bytes + 26, m.bytes[24]);
      memcpy(pass, m.bytes + 26 + m.bytes[24], m.bytes[25]);
      service_wifi_connect(ssid, pass);
      demo_clear(ssid, sizeof(ssid));
      demo_clear(pass, sizeof(pass));
    } else if (op == 4 && n == 36) {
      uint32_t release = le32(m.bytes + 24), floor = 0;
      nvs_get_u32(nvs, "release", &floor);
      if (release == floor && journal.phase == 4 &&
          !memcmp(journal.hash, m.bytes + 28, 32)) {
        journal.tx = tx;
        save();
      } else if (release > floor && tx != journal.tx) {
        journal =
            (journal_t){.schema = 1, .phase = 1, .release = release, .tx = tx};
        memcpy(journal.hash, m.bytes + 28, 32);
        save();
      }
    } else if (op == 5 && n == 0 && journal.phase == 1 && journal.tx == tx) {
      uint64_t end = demo_ms() + 30000;
      while (!service_wifi_ready() && demo_ms() < end)
        vTaskDelay(pdMS_TO_TICKS(200));
      manifest_t next;
      if (service_wifi_ready() && tls_time() &&
          get_manifest(journal.release, &next) &&
          !memcmp(next.hash[1], journal.hash, 32)) {
        demo_clear(&m, sizeof(m));
        install(&next, tx);
      } else {
        journal.phase = 5;
        save();
      }
    } else if (op == 6 && n == 0 && tx == journal.tx && journal.phase == 1) {
      journal.phase = 5;
      save();
    }
    demo_clear(&m, sizeof(m));
    publish_status();
  }
}
static void run(void *a) {
  (void)a;
  ESP_ERROR_CHECK(nvs_open("updates", NVS_READWRITE, &nvs));
  size_t size = sizeof(journal);
  if (nvs_get_blob(nvs, "journal", &journal, &size) != ESP_OK ||
      size != sizeof(journal) || journal.schema != 1)
    demo_clear(&journal, sizeof(journal));
  journal_ready = true;
  vTaskDelay(pdMS_TO_TICKS(12000));
  if (journal.phase == 3) {
    esp_ota_img_states_t state;
    if (installed_hash(&journal) &&
        esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) ==
            ESP_OK &&
        state == ESP_OTA_IMG_VALID) {
      journal.phase = 4;
      if (nvs_set_u32(nvs, "release", journal.release) != ESP_OK || !save())
        journal.phase = 5;
    } else {
      journal.phase = 5;
      save();
    }
  }
  publish_status();
  for (;;) {
    if (wheel) {
      intent_t intent;
      if (xQueueReceive(demo_intents, &intent, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (intent.kind == INTENT_SERVICE)
          service_wifi_open();
        else if (intent.kind == INTENT_CHECK)
          search();
        else if (intent.kind == INTENT_CANCEL) {
          offer = false;
          service_wifi_close();
          say("Cancelled");
        } else if (intent.kind == INTENT_CONFIRM && offer &&
                   intent.release == candidate.release) {
          offer = false;
          demo_state_t state;
          demo_get(&state);
          if (state.standalone) {
            transaction = ((uint64_t)esp_random() << 32) | esp_random();
            install(&candidate, transaction);
          } else if (paired())
            install(&candidate, transaction);
          else
            say("Paired update not confirmed");
        }
      } else {
        static uint64_t polled;
        if (demo_ms() - polled > 1000) {
          polled = demo_ms();
          ble_link_poll_telemetry();
        }
      }
    } else {
      gateway_commands();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}
void update_manager_start(bool is_wheel) {
  wheel = is_wheel;
  session = esp_random();
  psa_crypto_init();
  configASSERT(xTaskCreatePinnedToCore(run, "update_worker", 12288, NULL, 2,
                                       NULL, 0) == pdPASS);
}
