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
#include "wheel_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool started, online, ap, persisted;
static uint32_t request_id;
static unsigned request_status;
static uint64_t close_after;
static uint64_t attempt;
static char saved_ssid[33], saved_pass[64], token[33];
static httpd_handle_t server;
static uint64_t deadline;
typedef struct {
  char ssid[33], pass[64];
} credentials_t;
typedef enum {
  OPEN,
  CLOSE,
  CONNECT,
  FORGET,
  TOUCH,
  SCAN,
  GOT_IP,
  DISCONNECTED
} command_kind_t;
typedef struct {
  command_kind_t kind;
  credentials_t credentials;
  uint32_t id;
} command_t;
static QueueHandle_t configs;
static credentials_t confirmed, last_request;
static void open_owned(void);
static void close_owned(void);
static bool connect_owned(const char *, const char *);
static bool enqueue(command_kind_t kind) {
  command_t c = {.kind = kind};
  return xQueueSend(configs, &c, 0) == pdTRUE;
}

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static struct {
  bool ap, online;
  uint64_t deadline;
  uint32_t id;
  unsigned status;
  char token[33];
  credentials_t good;
} visible;
static void publish_view(void) {
  portENTER_CRITICAL(&lock);
  visible.ap = ap;
  visible.online = online;
  visible.deadline = deadline;
  visible.id = request_id;
  visible.status = request_status;
  memcpy(visible.token, token, 33);
  visible.good = confirmed;
  portEXIT_CRITICAL(&lock);
}
static void ap_closed(demo_state_t *s, void *a) {
  (void)a;
  demo_clear(s->ap_password, sizeof(s->ap_password));
}

static void msg(demo_state_t *s, void *a) {
  snprintf(s->network, sizeof(s->network), "%s", (char *)a);
}
static void event(void *a, esp_event_base_t b, int32_t id, void *d) {
  (void)a;
  (void)d;
  if (b == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    enqueue(GOT_IP);
  else if (b == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    enqueue(DISCONNECTED);
}
static bool on_ap(httpd_req_t *r) {
  struct sockaddr_in addr;
  socklen_t n = sizeof(addr);
  bool enabled;
  portENTER_CRITICAL(&lock);
  enabled = visible.ap && demo_ms() < visible.deadline;
  portEXIT_CRITICAL(&lock);
  return enabled &&
         getsockname(httpd_req_to_sockfd(r), (struct sockaddr *)&addr, &n) ==
             0 &&
         addr.sin_addr.s_addr == inet_addr("192.168.4.1");
}
static esp_err_t page(httpd_req_t *r) {
  if (!on_ap(r))
    return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "AP only");
  enqueue(TOUCH);
  char page_token[33];
  portENTER_CRITICAL(&lock);
  memcpy(page_token, visible.token, 33);
  portEXIT_CRITICAL(&lock);
  char html[3000];
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
      "id=o></pre><script>const t='%s';let "
      "rid=Date.now()>>>0;setInterval(async()=>{try{o.textContent=await(await "
      "fetch('/"
      "status',{headers:{'X-Service-Token':t}})).text()}catch{}},2000);f."
      "onsubmit=async "
      "e=>{e.preventDefault();const a=new TextEncoder().encode(s.value),c=new "
      "TextEncoder().encode(p.value);if(a.length>32||c.length>63)return;const "
      "d=new "
      "Uint8Array(2+a.length+c.length);d[0]=a.length;d[1]=c.length;d.set(a,2);"
      "d.set(c,2+a.length);o.textContent=await(await "
      "fetch('/"
      "configure',{method:'POST',headers:{'X-Service-Token':t,'X-Request-ID':"
      "String(++rid)},body:d})).text()"
      ";p.value='';};b.onclick=async()=>{o.textContent=await(await "
      "fetch('/"
      "scan',{headers:{'X-Service-Token':t}})).text();};q.onclick=async()=>{o."
      "textContent=await(await "
      "fetch('/"
      "forget',{method:'POST',headers:{'X-Service-Token':t}})).text();};</"
      "script>",
      page_token);
  demo_clear(page_token, sizeof(page_token));
  httpd_resp_set_type(r, "text/html");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return httpd_resp_sendstr(r, html);
}
static bool auth(httpd_req_t *r) {
  char t[40], expected[33];
  portENTER_CRITICAL(&lock);
  memcpy(expected, visible.token, sizeof(expected));
  portEXIT_CRITICAL(&lock);
  bool ok = on_ap(r) &&
            httpd_req_get_hdr_value_str(r, "X-Service-Token", t, sizeof(t)) ==
                ESP_OK &&
            !strcmp(t, expected);
  demo_clear(t, sizeof(t));
  demo_clear(expected, sizeof(expected));
  if (ok && strcmp(r->uri, "/status"))
    enqueue(TOUCH);
  return ok;
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
  command_t c = {.kind = CONNECT};
  char id[16];
  char *end;
  unsigned long parsed = 0;
  if (httpd_req_get_hdr_value_str(r, "X-Request-ID", id, sizeof(id)) ==
      ESP_OK) {
    parsed = strtoul(id, &end, 10);
    if (*end)
      parsed = 0;
  }
  if (!parsed || parsed > UINT32_MAX) {
    demo_clear(data, sizeof(data));
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Request ID required");
  }
  c.id = (uint32_t)parsed;
  memcpy(c.credentials.ssid, data + 2, sl);
  memcpy(c.credentials.pass, data + 2 + sl, pl);
  bool ok = xQueueSend(configs, &c, 0) == pdTRUE;
  demo_clear(data, sizeof(data));
  demo_clear(&c, sizeof(c));
  return httpd_resp_sendstr(r,
                            ok ? "Connecting. Check wheel display." : "Busy");
}
static bool scan_requested, forget_requested;
static char scan_result[1024] = "Press scan, wait, then scan again.";
static esp_err_t scan(httpd_req_t *r) {
  if (!auth(r))
    return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Denied");
  enqueue(SCAN);
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
  enqueue(FORGET);
  return httpd_resp_sendstr(r, "Stored network will be removed.");
}
static esp_err_t status_page(httpd_req_t *r) {
  if (!auth(r))
    return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Denied");
  char result[160];
  portENTER_CRITICAL(&lock);
  snprintf(result, sizeof(result),
           "{\"request_id\":%lu,\"state\":%u,\"online\":%s}",
           (unsigned long)visible.id, visible.status,
           visible.online ? "true" : "false");
  portEXIT_CRITICAL(&lock);
  httpd_resp_set_type(r, "application/json");
  return httpd_resp_sendstr(r, result);
}
static void worker(void *a) {
  (void)a;
  command_t c;
  for (;;) {
    if (xQueueReceive(configs, &c, pdMS_TO_TICKS(100)) == pdTRUE) {
      switch (c.kind) {
      case OPEN:
        open_owned();
        break;
      case CLOSE:
        close_owned();
        break;
      case FORGET:
        forget_requested = true;
        break;
      case SCAN:
        scan_requested = true;
        break;
      case TOUCH:
        if (ap)
          deadline = demo_ms() + 300000;
        break;
      case DISCONNECTED:
        online = false;
        demo_edit(msg, "Wi-Fi disconnected");
        break;
      case GOT_IP: {
        wifi_ap_record_t actual;
        if (attempt && esp_wifi_sta_get_ap_info(&actual) == ESP_OK &&
            !strcmp((char *)actual.ssid, saved_ssid)) {
          online = true;
          demo_edit(msg, "Wi-Fi connected");
        }
        break;
      }
      case CONNECT: {
        int disposition = portal_request_accept(
            request_id, c.id,
            !memcmp(&last_request, &c.credentials, sizeof(last_request)));
        if (c.id && disposition <= 0) {
          if (disposition < 0)
            demo_edit(msg, "Rejected stale/conflicting request");
          break;
        }
        if (c.id) {
          request_id = c.id;
          last_request = c.credentials;
        }
        request_status = 1;
        if (!connect_owned(c.credentials.ssid, c.credentials.pass)) {
          request_status = 3;
          demo_edit(msg, "Wi-Fi driver error");
        }
        break;
      }
      }
      demo_clear(&c, sizeof(c));
    }
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
      esp_err_t opened = nvs_open("wifi_good", NVS_READWRITE, &store);
      if (opened != ESP_OK) {
        demo_edit(msg, "Forget storage open failed");
        publish_view();
        continue;
      }
      if (opened == ESP_OK) {
        esp_err_t e = nvs_erase_all(store);
        if (e == ESP_OK)
          e = nvs_commit(store);
        if (e != ESP_OK) {
          nvs_close(store);
          demo_edit(msg, "Forget storage failed");
          publish_view();
          continue;
        }
        nvs_close(store);
      }
      demo_clear(&confirmed, sizeof(confirmed));
      demo_clear(&last_request, sizeof(last_request));
      request_id = 0;
      request_status = 0;
      demo_edit(msg, "Stored network forgotten");
    }
    if (online && !persisted) {
      wifi_ap_record_t actual;
      if (esp_wifi_sta_get_ap_info(&actual) == ESP_OK &&
          !strcmp((char *)actual.ssid, saved_ssid)) {
        nvs_handle_t store;
        if (nvs_open("wifi_good", NVS_READWRITE, &store) == ESP_OK) {
          credentials_t valid = {0};
          strcpy(valid.ssid, saved_ssid);
          strcpy(valid.pass, saved_pass);
          if (nvs_set_blob(store, "network", &valid, sizeof(valid)) == ESP_OK &&
              nvs_commit(store) == ESP_OK) {
            persisted = true;
            attempt = 0;
            request_status = 2;
            close_after = demo_ms() + 10000;
            portENTER_CRITICAL(&lock);
            strcpy(confirmed.ssid, saved_ssid);
            strcpy(confirmed.pass, saved_pass);
            portEXIT_CRITICAL(&lock);
            demo_state_t state;
            demo_get(&state);
            if (state.maintenance) {
              intent_t intent = {.kind = INTENT_CHECK};
              xQueueSend(demo_intents, &intent, 0);
            }
          }
          demo_clear(&valid, sizeof(valid));
          nvs_close(store);
        }
        if (!persisted) {
          request_status = 4;
          demo_edit(msg, "Connected; saving failed");
          persisted = true;
        }
      }
    }
    if (attempt && !online && demo_ms() - attempt > 30000) {
      attempt = 0;
      esp_wifi_disconnect();
      request_status = 3;
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
          pos +=
              snprintf(text + pos, sizeof(text) - pos, "%s (%d dBm, auth %u)\n",
                       records[i].ssid, records[i].rssi, records[i].authmode);
        portENTER_CRITICAL(&lock);
        memcpy(scan_result, text, sizeof(text));
        portEXIT_CRITICAL(&lock);
      }
    }
    if (ap &&
        (demo_ms() > deadline || (close_after && demo_ms() > close_after))) {
      ap = false;
      publish_view();
      demo_edit(ap_closed, NULL);
      if (server) {
        httpd_stop(server);
        server = NULL;
      }
      esp_wifi_set_mode(WIFI_MODE_STA);
      demo_clear(token, sizeof(token));
    }
    publish_view();
  }
}
void service_wifi_init(void) {
  configs = xQueueCreate(12, sizeof(command_t));
  configASSERT(configs);
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
static void open_owned(void) {
  if (ap)
    return;
  close_after = 0;
  request_id = 0;
  demo_clear(&last_request, sizeof(last_request));
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
  publish_view();
  if (!server) {
    httpd_config_t h = HTTPD_DEFAULT_CONFIG();
    h.core_id = 0;
    h.max_uri_handlers = 5;
    h.stack_size = 6144;
    ESP_ERROR_CHECK(httpd_start(&server, &h));
    httpd_uri_t p = {.uri = "/", .method = HTTP_GET, .handler = page},
                c = {.uri = "/configure",
                     .method = HTTP_POST,
                     .handler = configure},
                s = {.uri = "/scan", .method = HTTP_GET, .handler = scan};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &p));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &c));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &s));
    httpd_uri_t f = {.uri = "/forget", .method = HTTP_POST, .handler = forget};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &f));
    httpd_uri_t st = {
        .uri = "/status", .method = HTTP_GET, .handler = status_page};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &st));
  }
  nvs_handle_t store;
  if (nvs_open("wifi_good", NVS_READONLY, &store) == ESP_OK) {
    credentials_t known = {0};
    size_t size = sizeof(known);
    if (nvs_get_blob(store, "network", &known, &size) == ESP_OK &&
        size == sizeof(known) && known.ssid[32] == 0 && known.pass[63] == 0)
      connect_owned(known.ssid, known.pass);
    demo_clear(&known, sizeof(known));
    nvs_close(store);
  }
}
static bool connect_owned(const char *ssid, const char *pass) {
  size_t sl = strlen(ssid), pl = strlen(pass);
  if (!sl || sl > 32 || pl < 8 || pl > 63)
    return false;
  wifi_config_t cfg = {0};
  memcpy(cfg.sta.ssid, ssid, sl);
  memcpy(cfg.sta.password, pass, pl);
  cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  online = false;
  demo_clear(&confirmed, sizeof(confirmed));
  publish_view();
  persisted = false;
  attempt = demo_ms();
  esp_wifi_disconnect();
  esp_err_t mode = esp_wifi_set_mode(ap ? WIFI_MODE_APSTA : WIFI_MODE_STA);
  if (mode != ESP_OK) {
    demo_clear(&cfg, sizeof(cfg));
    return false;
  }
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
bool service_wifi_ready(void) {
  bool value;
  portENTER_CRITICAL(&lock);
  value = visible.online;
  portEXIT_CRITICAL(&lock);
  return value;
}
bool service_wifi_credentials(char ssid[33], char pass[64]) {
  portENTER_CRITICAL(&lock);
  bool ok = visible.online && visible.good.ssid[0];
  if (ok) {
    memcpy(ssid, visible.good.ssid, 33);
    memcpy(pass, visible.good.pass, 64);
  } else {
    demo_clear(ssid, 33);
    demo_clear(pass, 64);
  }
  portEXIT_CRITICAL(&lock);
  return ok;
}
void service_wifi_open(void) {
  if (!enqueue(OPEN))
    demo_edit(msg, "Wi-Fi queue busy");
}
void service_wifi_close(void) {
  if (!enqueue(CLOSE))
    demo_edit(msg, "Wi-Fi queue busy");
}
void service_wifi_forget(void) {
  if (!enqueue(FORGET))
    demo_edit(msg, "Wi-Fi queue busy");
}
bool service_wifi_connect(const char *ssid, const char *pass) {
  if (!ssid || !pass || !strlen(ssid) || strlen(ssid) > 32 ||
      strlen(pass) < 8 || strlen(pass) > 63)
    return false;
  command_t c = {.kind = CONNECT};
  strcpy(c.credentials.ssid, ssid);
  strcpy(c.credentials.pass, pass);
  bool ok = xQueueSend(configs, &c, 0) == pdTRUE;
  demo_clear(&c, sizeof(c));
  return ok;
}

static void closed(demo_state_t *s, void *a) {
  (void)a;
  s->maintenance = false;
  demo_clear(s->ap_password, sizeof(s->ap_password));
}
static void close_owned(void) {
  ap = false;
  online = false;
  attempt = 0;
  publish_view();
  if (server) {
    httpd_stop(server);
    server = NULL;
  }
  if (started) {
    esp_wifi_stop();
    started = false;
  }
  demo_clear(token, sizeof(token));
  demo_clear(&last_request, sizeof(last_request));
  demo_clear(&confirmed, sizeof(confirmed));
  demo_clear(saved_ssid, sizeof(saved_ssid));
  demo_clear(saved_pass, sizeof(saved_pass));
  publish_view();
  demo_edit(closed, NULL);
}
