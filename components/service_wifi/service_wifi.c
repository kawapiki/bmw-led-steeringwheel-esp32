#include "service_wifi.h"
#include "demo.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
static bool started, online, ap, persisted;
static uint64_t attempt;
static char saved_ssid[33], saved_pass[64], token[33];
static httpd_handle_t server;
static uint64_t deadline;
typedef struct {
  char ssid[33], pass[64];
} credentials_t;
static QueueHandle_t configs;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static void msg(demo_state_t *s, void *a) {
  snprintf(s->network, sizeof(s->network), "%s", (char *)a);
}
static void event(void *a, esp_event_base_t b, int32_t id, void *d) {
  (void)a;
  (void)d;
  if (b == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    online = true;
    demo_edit(msg, "Wi-Fi connected");
  } else if (b == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    online = false;
    demo_edit(msg, "Wi-Fi disconnected");
  }
}
static bool on_ap(httpd_req_t *r) {
  struct sockaddr_in addr;
  socklen_t n = sizeof(addr);
  return ap && demo_ms() < deadline &&
         getsockname(httpd_req_to_sockfd(r), (struct sockaddr *)&addr, &n) ==
             0 &&
         addr.sin_addr.s_addr == inet_addr("192.168.4.1");
}
static esp_err_t page(httpd_req_t *r) {
  if (!on_ap(r))
    return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "AP only");
  char html[2200];
  snprintf(
      html, sizeof(html),
      "<!doctype html><meta name=viewport "
      "content='width=device-width'><title>BMW wheel Wi-Fi</title><h2>Wheel "
      "service</h2><p>Credentials stay on your paired devices.</p><form "
      "id=f><label>Network <input id=s maxlength=32 "
      "required></label><p><label>Password <input id=p type=password "
      "minlength=8 maxlength=63 "
      "required></label><p><button>Connect</button></form><button "
      "id=b>Scan</button><button id=q>Forget saved network</button><pre "
      "id=o></pre><script>const t='%s';f.onsubmit=async "
      "e=>{e.preventDefault();const a=new TextEncoder().encode(s.value),c=new "
      "TextEncoder().encode(p.value);if(a.length>32||c.length>63)return;const "
      "d=new "
      "Uint8Array(2+a.length+c.length);d[0]=a.length;d[1]=c.length;d.set(a,2);"
      "d.set(c,2+a.length);o.textContent=await(await "
      "fetch('/"
      "configure',{method:'POST',headers:{'X-Service-Token':t},body:d})).text()"
      ";p.value='';};b.onclick=async()=>{o.textContent=await(await "
      "fetch('/"
      "scan',{headers:{'X-Service-Token':t}})).text();};q.onclick=async()=>{o."
      "textContent=await(await "
      "fetch('/"
      "forget',{method:'POST',headers:{'X-Service-Token':t}})).text();};</"
      "script>",
      token);
  httpd_resp_set_type(r, "text/html");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return httpd_resp_sendstr(r, html);
}
static bool auth(httpd_req_t *r) {
  char t[40];
  return on_ap(r) &&
         httpd_req_get_hdr_value_str(r, "X-Service-Token", t, sizeof(t)) ==
             ESP_OK &&
         !strcmp(t, token);
}
static esp_err_t configure(httpd_req_t *r) {
  if (!auth(r))
    return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Denied");
  uint8_t data[97] = {0};
  if (r->content_len < 11 || r->content_len > 97)
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid lengths");
  int used = 0;
  while (used < r->content_len) {
    int n = httpd_req_recv(r, (char *)data + used, r->content_len - used);
    if (n <= 0) {
      demo_clear(data, sizeof(data));
      return ESP_FAIL;
    }
    used += n;
  }
  size_t sl = data[0], pl = data[1];
  if (sl < 1 || sl > 32 || pl < 8 || pl > 63 || sl + pl + 2 != used) {
    demo_clear(data, sizeof(data));
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid credentials");
  }
  for (size_t i = 0; i < sl; i++)
    if (data[2 + i] == 0) {
      demo_clear(data, sizeof(data));
      return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid SSID");
    }
  for (size_t i = 0; i < pl; i++)
    if (data[2 + sl + i] < 32 || data[2 + sl + i] > 126) {
      demo_clear(data, sizeof(data));
      return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                                 "WPA2 passphrase must be printable ASCII");
    }
  credentials_t c = {0};
  memcpy(c.ssid, data + 2, sl);
  memcpy(c.pass, data + 2 + sl, pl);
  bool ok = xQueueSend(configs, &c, 0) == pdTRUE;
  demo_clear(data, sizeof(data));
  demo_clear(&c, sizeof(c));
  return httpd_resp_sendstr(r,
                            ok ? "Connecting. Check wheel display." : "Busy");
}
static volatile bool scan_requested, forget_requested;
static char scan_result[1024] = "Press scan, wait, then scan again.";
static esp_err_t scan(httpd_req_t *r) {
  if (!auth(r))
    return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Denied");
  scan_requested = true;
  char copy[1024];
  portENTER_CRITICAL(&lock);
  memcpy(copy, scan_result, sizeof(copy));
  portEXIT_CRITICAL(&lock);
  httpd_resp_set_type(r, "text/plain");
  return httpd_resp_sendstr(r, copy);
}
static esp_err_t forget(httpd_req_t *r) {
  if (!auth(r))
    return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Denied");
  forget_requested = true;
  return httpd_resp_sendstr(r, "Stored network will be removed.");
}
static void worker(void *a) {
  (void)a;
  credentials_t c;
  for (;;) {
    if (forget_requested) {
      forget_requested = false;
      online = false;
      attempt = 0;
      esp_wifi_disconnect();
      portENTER_CRITICAL(&lock);
      demo_clear(saved_ssid, sizeof(saved_ssid));
      demo_clear(saved_pass, sizeof(saved_pass));
      portEXIT_CRITICAL(&lock);
      nvs_handle_t store;
      if (nvs_open("wifi_good", NVS_READWRITE, &store) == ESP_OK) {
        nvs_erase_all(store);
        nvs_commit(store);
        nvs_close(store);
      }
      demo_edit(msg, "Stored network forgotten");
    }
    if (xQueueReceive(configs, &c, pdMS_TO_TICKS(100)) == pdTRUE) {
      service_wifi_connect(c.ssid, c.pass);
      demo_clear(&c, sizeof(c));
    }
    if (online && !persisted) {
      wifi_ap_record_t actual;
      if (esp_wifi_sta_get_ap_info(&actual) == ESP_OK &&
          !strcmp((char *)actual.ssid, saved_ssid)) {
        nvs_handle_t store;
        if (nvs_open("wifi_good", NVS_READWRITE, &store) == ESP_OK) {
          if (nvs_set_str(store, "ssid", saved_ssid) == ESP_OK &&
              nvs_set_str(store, "password", saved_pass) == ESP_OK &&
              nvs_commit(store) == ESP_OK) {
            persisted = true;
            demo_state_t state;
            demo_get(&state);
            if (state.maintenance) {
              intent_t intent = {.kind = INTENT_CHECK};
              xQueueSend(demo_intents, &intent, 0);
            }
          }
          nvs_close(store);
        }
      }
    }
    if (attempt && !online && demo_ms() - attempt > 30000) {
      attempt = 0;
      esp_wifi_disconnect();
      demo_edit(msg, "Wi-Fi connection timeout");
    }
    if (scan_requested) {
      scan_requested = false;
      wifi_scan_config_t cfg = {.show_hidden = true};
      if (esp_wifi_scan_start(&cfg, true) == ESP_OK) {
        uint16_t n = 12;
        wifi_ap_record_t records[12];
        char text[1024] = "";
        esp_wifi_scan_get_ap_records(&n, records);
        size_t pos = 0;
        for (int i = 0; i < n && pos < 900; i++)
          pos += snprintf(text + pos, sizeof(text) - pos, "%s (%d dBm)\n",
                          records[i].ssid, records[i].rssi);
        portENTER_CRITICAL(&lock);
        memcpy(scan_result, text, sizeof(text));
        portEXIT_CRITICAL(&lock);
      }
    }
    if (ap && demo_ms() > deadline) {
      ap = false;
      if (server) {
        httpd_stop(server);
        server = NULL;
      }
      esp_wifi_set_mode(WIFI_MODE_STA);
      demo_clear(token, sizeof(token));
    }
  }
}
void service_wifi_init(void) {
  configs = xQueueCreate(2, sizeof(credentials_t));
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(
      esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, NULL));
  ESP_ERROR_CHECK(
      esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, NULL));
  configASSERT(xTaskCreatePinnedToCore(worker, "wifi_service", 6144, NULL, 3,
                                       NULL, 0) == pdPASS);
}
static void apstate(demo_state_t *s, void *a) {
  snprintf(s->ap_password, sizeof(s->ap_password), "%s", (char *)a);
  s->maintenance = true;
  snprintf(s->network, sizeof(s->network), "BMW-Wheel / 192.168.4.1");
}
void service_wifi_open(void) {
  wifi_config_t cfg = {0};
  strcpy((char *)cfg.ap.ssid, "BMW-Wheel");
  char password[17];
  for (int i = 0; i < 16; i++)
    password[i] = "abcdefghjkmnpqrstuvwxyz23456789"[esp_random() % 30];
  password[16] = 0;
  memcpy(cfg.ap.password, password, 17);
  cfg.ap.ssid_len = 9;
  cfg.ap.channel = 1;
  cfg.ap.max_connection = 2;
  cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
  for (int i = 0; i < 32; i++)
    token[i] = "0123456789abcdef"[esp_random() % 16];
  token[32] = 0;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
  demo_clear(&cfg, sizeof(cfg));
  if (!started) {
    ESP_ERROR_CHECK(esp_wifi_start());
    started = true;
  }
  ap = true;
  deadline = demo_ms() + 300000;
  demo_edit(apstate, password);
  demo_clear(password, sizeof(password));
  if (!server) {
    httpd_config_t h = HTTPD_DEFAULT_CONFIG();
    h.core_id = 0;
    h.max_uri_handlers = 4;
    h.stack_size = 6144;
    ESP_ERROR_CHECK(httpd_start(&server, &h));
    httpd_uri_t p = {.uri = "/", .method = HTTP_GET, .handler = page},
                c = {.uri = "/configure",
                     .method = HTTP_POST,
                     .handler = configure},
                s = {.uri = "/scan", .method = HTTP_GET, .handler = scan};
    httpd_register_uri_handler(server, &p);
    httpd_register_uri_handler(server, &c);
    httpd_register_uri_handler(server, &s);
    httpd_uri_t f = {.uri = "/forget", .method = HTTP_POST, .handler = forget};
    httpd_register_uri_handler(server, &f);
  }
  nvs_handle_t store;
  if (nvs_open("wifi_good", NVS_READONLY, &store) == ESP_OK) {
    credentials_t known = {0};
    size_t sl = sizeof(known.ssid), pl = sizeof(known.pass);
    if (nvs_get_str(store, "ssid", known.ssid, &sl) == ESP_OK &&
        nvs_get_str(store, "password", known.pass, &pl) == ESP_OK)
      xQueueSend(configs, &known, 0);
    demo_clear(&known, sizeof(known));
    nvs_close(store);
  }
}
bool service_wifi_connect(const char *ssid, const char *pass) {
  size_t sl = strlen(ssid), pl = strlen(pass);
  if (!sl || sl > 32 || pl < 8 || pl > 63)
    return false;
  wifi_config_t cfg = {0};
  memcpy(cfg.sta.ssid, ssid, sl);
  memcpy(cfg.sta.password, pass, pl);
  cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  online = false;
  persisted = false;
  attempt = demo_ms();
  esp_wifi_disconnect();
  esp_wifi_set_mode(ap ? WIFI_MODE_APSTA : WIFI_MODE_STA);
  esp_err_t configured = esp_wifi_set_config(WIFI_IF_STA, &cfg);
  demo_clear(&cfg, sizeof(cfg));
  if (configured != ESP_OK)
    return false;
  if (!started) {
    if (esp_wifi_start() != ESP_OK)
      return false;
    started = true;
  }
  portENTER_CRITICAL(&lock);
  strcpy(saved_ssid, ssid);
  strcpy(saved_pass, pass);
  portEXIT_CRITICAL(&lock);
  return esp_wifi_connect() == ESP_OK;
}
bool service_wifi_ready(void) { return online; }
bool service_wifi_credentials(char ssid[33], char pass[64]) {
  portENTER_CRITICAL(&lock);
  memcpy(ssid, saved_ssid, 33);
  memcpy(pass, saved_pass, 64);
  portEXIT_CRITICAL(&lock);
  wifi_ap_record_t actual;
  return online && ssid[0] && esp_wifi_sta_get_ap_info(&actual) == ESP_OK &&
         !strcmp((char *)actual.ssid, ssid);
}

static void closed(demo_state_t *s, void *a) {
  (void)a;
  s->maintenance = false;
  demo_clear(s->ap_password, sizeof(s->ap_password));
}
void service_wifi_close(void) {
  ap = false;
  online = false;
  attempt = 0;
  if (server) {
    httpd_stop(server);
    server = NULL;
  }
  if (started) {
    esp_wifi_stop();
    started = false;
  }
  demo_clear(token, sizeof(token));
  demo_edit(closed, NULL);
}
