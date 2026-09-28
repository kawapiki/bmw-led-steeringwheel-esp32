#pragma once
#include "cockpit.h"
#include "telemetry_v1.h"
#define COCKPIT_PACKET_SIZE 22
typedef struct {
  uint32_t sequence;
  uint64_t received;
  uint16_t valid, rpm, speed_dkph;
  uint8_t source, gear, closure_open;
  int16_t coolant_c, oil_c;
} cockpit_sample_t;
void cockpit_encode(uint8_t out[COCKPIT_PACKET_SIZE], uint32_t sequence,
                    uint32_t uptime, const cockpit_sample_t *sample);
bool cockpit_accept(telemetry_v1_tracker_t *tracker, uint32_t session,
                    const uint8_t *packet, size_t length, uint64_t requested,
                    uint64_t now, cockpit_sample_t *sample);
void cockpit_demo(uint32_t now, cockpit_sample_t *sample);
