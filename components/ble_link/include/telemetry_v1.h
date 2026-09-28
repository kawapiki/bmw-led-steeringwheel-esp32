#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Deployed application v1: four LE uint32 fields; synthetic gateway RPM only.
 * Never reinterpret v1 RPM as vehicle evidence. Recovery has a separate ABI. */
typedef struct {
  uint32_t session, sequence, uptime;
  bool seen;
} telemetry_v1_tracker_t;
typedef struct {
  uint32_t sequence, rpm;
  uint64_t received;
} telemetry_v1_sample_t;
void telemetry_v1_encode(uint8_t out[16], uint32_t sequence, uint32_t rpm,
                         uint32_t uptime);
bool telemetry_v1_accept(telemetry_v1_tracker_t *tracker, uint32_t session,
                         const uint8_t *data, size_t length,
                         uint64_t requested, uint64_t now,
                         telemetry_v1_sample_t *sample);
