#pragma once
#include "lighting.h"
#include "telemetry_v1.h"
#define LIGHTING_PACKET_SIZE 16
typedef struct {
  uint32_t sequence;
  uint64_t received;
  uint8_t source, valid, on;
} lighting_sample_t;
void lighting_encode(uint8_t out[LIGHTING_PACKET_SIZE], uint32_t sequence,
                     uint32_t uptime, const lighting_sample_t *sample);
bool lighting_accept(telemetry_v1_tracker_t *tracker, uint32_t session,
                     const uint8_t *packet, size_t length, uint64_t requested,
                     uint64_t now, lighting_sample_t *sample);
void lighting_demo(uint32_t now, lighting_sample_t *sample);
