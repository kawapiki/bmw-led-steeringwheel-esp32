#pragma once
#include <stdbool.h>
#include <stdint.h>
void service_wifi_init(void);
void service_wifi_open(void);
bool service_wifi_connect(const char *ssid, const char *password);
bool service_wifi_ready(void);
bool service_wifi_credentials(char ssid[33], char pass[64]);

void service_wifi_close(void);

void service_wifi_forget(void);
