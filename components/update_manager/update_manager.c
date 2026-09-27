#include "update_manager.h"
#include "ble_link.h"
#include "cJSON.h"
#include "demo.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
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
  uint8_t hash[2][32], identity[32];
} manifest_t;
typedef update_journal_t journal_t;
static bool wheel, offer, cancelled, journal_ready, boot_decided, boot_ok,
    active_work;
static uint32_t candidate_generation, last_result, installed_release,
    worker_cycles;
static uint64_t worker_heartbeat;
static portMUX_TYPE health_lock = portMUX_INITIALIZER_UNLOCKED;
static recovery_session_t dispatch_guard;
static uint32_t session, request;
static uint64_t transaction;
static manifest_t candidate;
static journal_t journal;
static nvs_handle_t nvs;
static void textstate(demo_state_t *s, void *a) {
  snprintf(s->update, sizeof(s->update), "%s", (char *)a);
  s->offer = offer;
  s->candidate_release = offer ? candidate.release : 0;
  s->candidate_generation = offer ? candidate_generation : 0;
  memcpy(s->candidate_digest, candidate.identity, 32);
  s->installed_release = installed_release;
  s->update_phase = journal.phase;
}
static void say(const char *s) {
  ESP_LOGI("update", "%s", s);
  demo_edit(textstate, (void *)s);
}
static void progress(demo_state_t *s, void *a) {
  s->progress = *(unsigned *)a;
  s->update_phase = journal.phase;
}
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
  bool ok = nvs_set_blob(nvs, "journal", &journal, sizeof(journal)) == ESP_OK &&
            nvs_commit(nvs) == ESP_OK;
  if (!ok)
    last_result = RECOVERY_STORAGE;
  return ok;
}
static bool tls_time(void) {
  if (time(NULL) > 1735689600)
    return true;
  esp_sntp_config_t c = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&c);
  return esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) == ESP_OK &&
         time(NULL) > 1735689600;
}
static char fetch_error[96];
static esp_http_client_handle_t open_url(const char *url) {
  snprintf(fetch_error, sizeof(fetch_error), "HTTPS connection failed");
  if (strncmp(url, "https://", 8))
    return NULL;
  esp_http_client_config_t c = {.url = url,
                                .crt_bundle_attach = esp_crt_bundle_attach,
                                .timeout_ms = 12000,
                                .disable_auto_redirect = true,
                                .buffer_size = 2048,
                                /* GitHub asset redirects exceed IDF default TX 512. */
                                .buffer_size_tx = 2048,
                                .user_agent = "BMW-Demo/1"};
  esp_http_client_handle_t h = esp_http_client_init(&c);
  if (!h)
    return NULL;
  for (int i = 0; i < 5; i++) {
    esp_err_t opened = esp_http_client_open(h, 0);
    if (opened != ESP_OK) {
      int tls_error = 0, tls_flags = 0;
      esp_http_client_get_and_clear_last_tls_error(h, &tls_error, &tls_flags);
      snprintf(fetch_error, sizeof(fetch_error),
               tls_error == PSA_ERROR_INSUFFICIENT_MEMORY || opened == ESP_ERR_NO_MEM
                 ? "HTTPS memory exhausted" : "HTTPS connection failed");
      ESP_LOGE("update_http", "open=%s tls=%d flags=%d free=%u largest=%u",
               esp_err_to_name(opened), tls_error, tls_flags,
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
      break;
    }
    if (esp_http_client_fetch_headers(h) < 0) {
      snprintf(fetch_error, sizeof(fetch_error), "HTTPS headers unavailable");
      break;
    }
    int status = esp_http_client_get_status_code(h);
    if (status == 200)
      return h;
    if (status < 300 || status > 399) {
      snprintf(fetch_error, sizeof(fetch_error), "GitHub HTTP %d", status);
      break;
    }
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
/* Establish TLS before allocating the response; empty release lists must not
 * reserve24KiB while TLS is allocating its handshake/record buffers. */
static int fetch_listing(const char *url, char **out) {
  const size_t limit = 24576;
  *out = NULL;
  esp_http_client_handle_t h = open_url(url);
  if (!h) return -1;
  int64_t declared = esp_http_client_get_content_length(h);
  char *data = NULL;
  size_t used = 0, capacity = 0;
  int result = -1;
  if (declared > 0 && (uint64_t)declared > limit) {
    snprintf(fetch_error, sizeof(fetch_error), "Release page exceeds 24 KiB");
    goto done;
  }
  capacity = declared > 0 ? (size_t)declared : 1024;
  data = malloc(capacity + 1);
  if (!data) {
    snprintf(fetch_error, sizeof(fetch_error), "Release response: no memory");
    goto done;
  }
  for (;;) {
    /* Complete means received into the client's internal buffer, not consumed
       by this reader. Always drain read() before checking completeness. */
    char chunk[512];
    int n = esp_http_client_read(h, chunk, sizeof(chunk));
    if (n > 0) {
      size_t needed = used + (size_t)n;
      if (needed > limit) {
        snprintf(fetch_error, sizeof(fetch_error), "Release page exceeds 24 KiB");
        break;
      }
      if (needed > capacity) {
        size_t next = capacity + 1024;
        if (next < needed) next = needed;
        if (next > limit) next = limit;
        char *grown = realloc(data, next + 1);
        if (!grown) {
          snprintf(fetch_error, sizeof(fetch_error), "Release response: no memory");
          break;
        }
        data = grown; capacity = next;
      }
      memcpy(data + used, chunk, n);
      used += n;
      continue;
    }
    if (n == 0 && esp_http_client_is_complete_data_received(h)) {
      data[used] = 0;
      *out = data; data = NULL; result = used;
    } else {
      snprintf(fetch_error, sizeof(fetch_error), "Release response incomplete");
    }
    break;
  }

done:
  free(data);
  esp_http_client_close(h);
  esp_http_client_cleanup(h);
  if (result >= 0) ESP_LOGI("update_http", "release listing: %d bytes", result);
  return result;
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
  memcpy(m->identity, hash, 32);
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

static void heartbeat(void) {
  portENTER_CRITICAL(&health_lock);
  worker_heartbeat = demo_ms();
  worker_cycles++;
  portEXIT_CRITICAL(&health_lock);
}
static bool healthy(void) {
  uint64_t wh, bh;
  uint32_t wc, bc;
  portENTER_CRITICAL(&health_lock);
  wh = worker_heartbeat;
  wc = worker_cycles;
  portEXIT_CRITICAL(&health_lock);
  ble_link_health(&bh, &bc);
  return journal_ready && recovery_health_ready(demo_ms(), wh, wc, bh, bc);
}
void update_boot_validate(bool passed) {
  while (passed && !healthy() && demo_ms() < 15000)
    vTaskDelay(pdMS_TO_TICKS(50));
  passed = passed && healthy();
  esp_ota_img_states_t state;
  const esp_partition_t *p = esp_ota_get_running_partition();
  if (esp_ota_get_state_partition(p, &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    if (journal.phase != UPDATE_BOOT_PENDING || !installed_hash(&journal))
      passed = false;
    if (passed) {
      if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK)
        passed = false;
    }
    if (!passed)
      esp_ota_mark_app_invalid_rollback_and_reboot();
  }
  ESP_LOGI("update", "boot self-test %s; slot=%s address=0x%lx",
           passed ? "passed" : "failed", p->label, (unsigned long)p->address);
  boot_ok = passed;
  boot_decided = true;
}
static void publish_status(void) {
  uint8_t b[80] = {0};
  memcpy(b, "BST1", 4);
  put32(b + 4, journal.phase);
  put32(b + 8, journal.release);
  put64(b + 12, journal.tx);
  memcpy(b + 20, journal.hash, 32);
  put32(b + 52, ble_link_session());
  put32(b + 56, request);
  put32(b + 60, 1);
  put32(b + 64, last_result);
  put32(b + 68,
        active_work || (journal.phase == UPDATE_BOOT_PENDING && !boot_decided));
  demo_state_t state;
  demo_get(&state);
  put32(b + 72, state.progress);
  ble_link_status(b, sizeof(b));
}
static int accept_message(const recovery_message_t *m) {
  int rc = recovery_dispatch_accept(&dispatch_guard, m->bytes, m->size,
                                    ble_link_session());
  if (rc == RECOVERY_OK)
    request = le32(m->bytes + 12);
  last_result = rc;
  return rc;
}
static bool image_identity(const esp_partition_t *p, uint32_t size) {
  uint8_t buf[1087];
  size_t tail = 0;
  unsigned count = 0;
  for (uint32_t off = 0; off < size;) {
    size_t n = size - off;
    if (n > 1024)
      n = 1024;
    if (esp_partition_read(p, off, buf + tail, n) != ESP_OK)
      return false;
    size_t length = tail + n;
    for (size_t i = 0; i + 64 <= length; i++)
      if (!memcmp(buf + i, demo_build_identity(), 64))
        count++;
    tail = length < 63 ? length : 63;
    memmove(buf, buf + length - tail, tail);
    off += n;
  }
  return count == 1;
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
  active_work = true;
  last_result = RECOVERY_OK;
  say("Downloading authenticated image");
  unsigned zero = 0;
  demo_edit(progress, &zero);
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
      active_work = false;
      demo_edit(writing, &active);
      say("Output quiet check failed");
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
  uint64_t last_data = demo_ms();
  unsigned timeouts = 0, logged_percent = 0;
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
        if (accept_message(&control) == RECOVERY_OK) {
          if (control.bytes[3] == 6 && le64(control.bytes + 16) == tx)
            cancelled = true;
          else if (control.bytes[3] != 1 && control.bytes[3] != 2 &&
                   control.bytes[3] != 7)
            last_result = RECOVERY_BUSY;
        }
        publish_status();
        demo_clear(&control, sizeof(control));
      }
    }
    if (cancelled) {
      good = false;
      break;
    }
    int n = esp_http_client_read(http, (char *)data, sizeof(data));
    if (n == -ESP_ERR_HTTP_EAGAIN && ++timeouts <= 2 &&
        demo_ms() - last_data < 30000) {
      ESP_LOGW("update", "temporary read timeout at %lu/%lu; retry %u",
               (unsigned long)count, (unsigned long)m->size[index], timeouts);
      continue;
    }
    if (n <= 0 || count + n > m->size[index] ||
        psa_hash_update(&hash, data, n) != PSA_SUCCESS ||
        esp_ota_write(ota, data, n) != ESP_OK) {
      ESP_LOGE("update", "image read/write stopped: read=%d bytes=%lu/%lu",
               n, (unsigned long)count, (unsigned long)m->size[index]);
      good = false;
      break;
    }
    count += n;
    last_data = demo_ms();
    timeouts = 0;
    unsigned percent = update_progress_percent(count, m->size[index]);
    demo_edit(progress, &percent);
    if (percent >= logged_percent + 10 || percent == 100) {
      logged_percent = percent;
      ESP_LOGI("update", "image %u%%; bytes=%lu/%lu free=%u largest=%u",
               percent, (unsigned long)count, (unsigned long)m->size[index],
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    }
    publish_status();
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
      strcmp(desc.project_name, wheel ? "wheel_demo" : "gateway_demo") ||
      !image_identity(p, m->size[index]))
    goto failure;
  journal.phase = 3;
  publish_status();
  if (!save() || esp_ota_set_boot_partition(p) != ESP_OK)
    goto failure;
  say("Verified; restarting");
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
failure:
  active = false;
  active_work = false;
  demo_edit(writing, &active);
  journal.phase = UPDATE_FAILED;
  last_result = cancelled ? RECOVERY_CANCELLED : RECOVERY_IMAGE;
  if (!save())
    say("Storage failure; update stopped");
  else
    say(cancelled ? "Cancelled; previous image retained"
                  : "Update failed; confirm again to retry");
  publish_status();
  return false;
}
static void search(void) {
  offer = false;
  candidate_generation++;
  say("Checking signed GitHub releases");
  if (!service_wifi_ready() || !tls_time()) {
    say("Network or time unavailable");
    return;
  }
  uint32_t floor = 0;
  esp_err_t floor_error = nvs_get_u32(nvs, "release", &floor);
  if (floor_error != ESP_OK && floor_error != ESP_ERR_NVS_NOT_FOUND) {
    say("Release journal read failed");
    return;
  }
  manifest_t best = {0};
  uint32_t releases[60];
  size_t release_count = 0;
  char *json = NULL;
  /* Release objects include all assets/body; one per page bounds peak RAM.
   * Preserve the 60-release search window without retaining JSON across TLS. */
  for (int page = 1; page <= 60; page++) {
    char url[160];
    snprintf(url, sizeof(url),
             "https://api.github.com/repos/kawapiki/"
             "bmw-led-steeringwheel-esp32/releases?per_page=1&page=%d",
             page);
    int n = fetch_listing(url, &json);
    if (n < 0) {
      free(json);
      say(fetch_error);
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
    int page_count = cJSON_GetArraySize(list);
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
      if (release_count < 60)
        releases[release_count++] = release;
    }
    cJSON_Delete(list);
    free(json);
    json = NULL;
    if (page_count < 1)
      break;
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
  return n == 80 && !memcmp(s, "BST1", 4) && le32(s + 8) == candidate.release &&
         le64(s + 12) == tx && !memcmp(s + 20, candidate.hash[1], 32);
}
static bool confirmed_status(const uint8_t *s, size_t n) {
  update_gate_t g = {.gateway_online = ble_link_secure(),
                     .gateway_image_valid = n == 80 && le32(s + 4) == 4,
                     .authenticated_status = ble_link_secure(),
                     .status_fresh = true,
                     .requested_release = candidate.release,
                     .requested_transaction = transaction};
  if (n != 80 || memcmp(s, "BST1", 4))
    return false;
  g.confirmed_release = le32(s + 8);
  g.confirmed_transaction = le64(s + 12);
  memcpy(g.confirmed_digest, s + 20, 32);
  memcpy(g.requested_digest, candidate.hash[1], 32);
  return update_wheel_allowed(&g);
}

static bool establish_session(const uint8_t *status, size_t size) {
  if (size != 80 || memcmp(status, "BST1", 4) || le32(status + 60) != 1 ||
      !le32(status + 52))
    return false;
  uint32_t nonce = le32(status + 52);
  if (session != nonce) {
    session = nonce;
    request = 0;
  }
  if (request < le32(status + 56))
    request = le32(status + 56);
  recovery_message_t hello = packet(1, 0);
  return ble_link_send(&hello);
}
static bool command(const recovery_message_t *m) {
  if (!ble_link_send(m))
    return false;
  uint32_t wanted = le32(m->bytes + 12);
  uint64_t until = demo_ms() + 5000;
  while (demo_ms() < until) {
    uint8_t status[128];
    size_t n = sizeof(status);
    if (ble_link_read(status, &n) && n == 80 && le32(status + 52) == session &&
        le32(status + 56) == wanted) {
      if (le32(status + 64) != RECOVERY_OK) {
        say("Gateway rejected command; retry available");
        return false;
      }
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  say("Gateway command outcome unknown");
  return false;
}
static bool paired(void) {
  if (!ble_link_secure()) {
    say("Paired gateway unavailable; update stopped");
    return false;
  }
  bool resume = journal.phase == UPDATE_PREPARED &&
                journal.release == candidate.release &&
                !memcmp(journal.hash, candidate.hash[1], 32);
  transaction = resume ? journal.tx : 0;
  uint8_t status[128];
  size_t size = sizeof(status);
  if (!ble_link_read(status, &size) || size != 80) {
    say("Gateway status unavailable");
    return false;
  }
  bool match = status_matches(status, size, transaction);
  int action = update_coordinator_action(le32(status + 4), match,
                                         le32(status + 68) != 0);
  if (action == 3 && confirmed_status(status, size))
    return true;
  if (action == 0) {
    uint64_t previous = transaction;
    do {
      transaction = ((uint64_t)esp_random() << 32) | esp_random();
    } while (!transaction || transaction == previous);
  }
  journal = (journal_t){.schema = 1,
                        .phase = UPDATE_PREPARED,
                        .release = candidate.release,
                        .tx = transaction};
  memcpy(journal.hash, candidate.hash[1], 32);
  if (!save()) {
    say("Cannot persist update intent");
    return false;
  }
  if (!establish_session(status, size))
    return false;
  if (action != 2) {
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
    bool sent = command(&m);
    demo_clear(&m, sizeof(m));
    demo_clear(pass, sizeof(pass));
    demo_clear(ssid, sizeof(ssid));
    if (!sent)
      return false;
    m = packet(4, 36);
    put32(m.bytes + 24, candidate.release);
    memcpy(m.bytes + 28, candidate.hash[1], 32);
    if (!command(&m))
      return false;
    m = packet(5, 0);
    if (!command(&m))
      return false;
  }
  say("Gateway first; waiting for verified boot");
  uint64_t until = demo_ms() + 300000;
  while (demo_ms() < until) {
    intent_t intent;
    if (xQueueReceive(demo_intents, &intent, 0) == pdTRUE &&
        intent.kind == INTENT_CANCEL) {
      size = sizeof(status);
      if (ble_link_read(status, &size) && establish_session(status, size)) {
        recovery_message_t stop = packet(6, 0);
        command(&stop);
      }
      journal.phase = UPDATE_FAILED;
      last_result = RECOVERY_CANCELLED;
      if (!save())
        say("Cancel persistence failed");
      else
        say("Paired update cancelled");
      return false;
    }
    size = sizeof(status);
    if (ble_link_read(status, &size) && size == 80) {
      if (confirmed_status(status, size))
        return true;
      if (status_matches(status, size, transaction) &&
          le32(status + 4) == UPDATE_FAILED) {
        say("Gateway failed/interrupted; confirm to retry");
        return false;
      }
      if (status_matches(status, size, transaction) &&
          le32(status + 4) == UPDATE_DOWNLOADING) {
        unsigned p = le32(status + 72);
        demo_edit(progress, &p);
        char text[96];
        snprintf(text, sizeof(text), "Gateway download %u%%", p);
        say(text);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  say("Gateway outcome unknown; confirm to query");
  return false;
}
static bool release_floor(uint32_t *floor) {
  *floor = 0;
  esp_err_t err = nvs_get_u32(nvs, "release", floor);
  return err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND;
}
static void gateway_commands(void) {
  recovery_message_t m;
  while (ble_link_receive(&m)) {
    if (accept_message(&m) != RECOVERY_OK) {
      demo_clear(&m, sizeof(m));
      publish_status();
      continue;
    }
    uint64_t tx = le64(m.bytes + 16);
    uint8_t op = m.bytes[3];
    if (op == 1 || op == 2 ||
        op == 7) { /* session establishment/status; no mutation */
    } else if (!boot_ok)
      last_result = RECOVERY_BUSY;
    else if (op == 3) {
      char ssid[33] = {0}, pass[64] = {0};
      memcpy(ssid, m.bytes + 26, m.bytes[24]);
      memcpy(pass, m.bytes + 26 + m.bytes[24], m.bytes[25]);
      if (!service_wifi_connect(ssid, pass))
        last_result = RECOVERY_BUSY;
      demo_clear(ssid, sizeof(ssid));
      demo_clear(pass, sizeof(pass));
    } else if (op == 4) {
      uint32_t floor;
      update_journal_t next = journal;
      if (!release_floor(&floor))
        last_result = RECOVERY_STORAGE;
      else {
        last_result =
            update_prepare(&next, le32(m.bytes + 24), tx, m.bytes + 28, floor);
        if (last_result == RECOVERY_OK) {
          journal = next;
          if (!save())
            journal.phase = UPDATE_FAILED;
        }
      }
    } else if (op == 5) {
      if (journal.phase == UPDATE_VALID &&
          journal.tx == tx) { /* already installed; idempotent */
      } else if (journal.phase != UPDATE_PREPARED || journal.tx != tx)
        last_result = RECOVERY_STATE;
      else {
        journal.phase = UPDATE_DOWNLOADING;
        active_work = true;
        if (!save()) {
          active_work = false;
          journal.phase = UPDATE_FAILED;
        } else {
          publish_status();
          demo_clear(&m, sizeof(m));
          uint64_t end = demo_ms() + 30000;
          while (!service_wifi_ready() && demo_ms() < end)
            vTaskDelay(pdMS_TO_TICKS(100));
          manifest_t next;
          if (service_wifi_ready() && tls_time() &&
              get_manifest(journal.release, &next) &&
              !memcmp(next.hash[1], journal.hash, 32)) {
            install(&next, tx);
          } else {
            active_work = false;
            journal.phase = UPDATE_FAILED;
            last_result = RECOVERY_NETWORK;
            if (!save())
              last_result = RECOVERY_STORAGE;
            say("Network/manifest failure; retry available");
          }
        }
      }
    } else if (op == 6) {
      if (tx == journal.tx && journal.phase == UPDATE_PREPARED) {
        journal.phase = UPDATE_FAILED;
        last_result = RECOVERY_CANCELLED;
        if (!save())
          last_result = RECOVERY_STORAGE;
      } else
        last_result = RECOVERY_STATE;
    }
    demo_clear(&m, sizeof(m));
    publish_status();
  }
}
static void boot_outcome(void) {
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  esp_ota_get_state_partition(esp_ota_get_running_partition(), &state);
  uint32_t old = journal.phase;
  if (old == UPDATE_DOWNLOADING) {
    journal.phase = update_reconcile_phase(old, false, false);
    last_result = RECOVERY_IMAGE;
    if (!save())
      say("Interrupted update / storage error");
    else
      say("Interrupted download; retry available");
  } else if (old == UPDATE_BOOT_PENDING &&
             state != ESP_OTA_IMG_PENDING_VERIFY) {
    bool match = installed_hash(&journal);
    journal.phase = update_reconcile_phase(old, false, match);
    if (journal.phase == UPDATE_VALID) {
      installed_release = journal.release;
      if (nvs_set_u32(nvs, "release", installed_release) != ESP_OK || !save()) {
        journal.phase = UPDATE_FAILED;
        last_result = RECOVERY_STORAGE;
        say("Image valid; result persistence failed");
      } else {
        char text[96];
        snprintf(text, sizeof(text), "Installed release %lu successfully",
                 (unsigned long)installed_release);
        say(text);
      }
    } else {
      last_result = RECOVERY_IMAGE;
      if (!save())
        say("Rollback / storage error");
      else
        say("Rollback: previous firmware restored");
    }
  } else if (old == UPDATE_FAILED)
    say("Last update failed; confirm a release to retry");
  else if (old == UPDATE_VALID) {
    char text[96];
    snprintf(text, sizeof(text), "Installed release %lu",
             (unsigned long)installed_release);
    say(text);
  }
  publish_status();
}
static void run(void *a) {
  (void)a;
  if (nvs_open("updates", NVS_READWRITE, &nvs) != ESP_OK) {
    say("Update storage unavailable");
    vTaskDelete(NULL);
    return;
  }
  size_t size = sizeof(journal);
  esp_err_t err = nvs_get_blob(nvs, "journal", &journal, &size);
  if (err == ESP_ERR_NVS_NOT_FOUND)
    demo_clear(&journal, sizeof(journal));
  else if (err != ESP_OK || size != sizeof(journal) || journal.schema != 1) {
    say("Unsupported/corrupt update journal");
    vTaskDelete(NULL);
    return;
  }
  if (!release_floor(&installed_release)) {
    say("Release journal unreadable");
    vTaskDelete(NULL);
    return;
  }
  journal_ready = true;
  boot_outcome();
  bool reconciled = false;
  for (;;) {
    heartbeat();
    if (!boot_decided) {
      if (!wheel)
        gateway_commands();
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (!reconciled) {
      reconciled = true;
      boot_outcome();
      if (!boot_ok)
        say("Boot self-test failed; updates disabled");
    }
    if (!boot_ok) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (wheel) {
      intent_t intent;
      if (xQueueReceive(demo_intents, &intent, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (intent.kind == INTENT_SERVICE)
          service_wifi_open();
        else if (intent.kind == INTENT_FORGET)
          service_wifi_forget();
        else if (intent.kind == INTENT_CHECK)
          search();
        else if (intent.kind == INTENT_CANCEL) {
          offer = false;
          service_wifi_close();
          say("Cancelled");
        } else if (intent.kind == INTENT_CONFIRM && offer &&
                   intent.release == candidate.release &&
                   candidate_matches(candidate_generation, candidate.identity,
                                     intent.generation, intent.digest)) {
          offer = false;
          if (intent.standalone) {
            transaction = ((uint64_t)esp_random() << 32) | esp_random();
            install(&candidate, transaction);
          } else if (paired())
            install(&candidate, transaction);
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
