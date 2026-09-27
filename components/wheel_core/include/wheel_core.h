#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef CORE_HOST_TEST
#define CORE_API __declspec(dllexport)
#else
#define CORE_API
#endif
/* Recovery ABI candidate v1. No deployed endpoint yet. */
#define RECOVERY_HEADER_SIZE 24u
#define RECOVERY_MAX_PAYLOAD 128u
CORE_API int recovery_validate(const uint8_t *frame, size_t len, bool encrypted,
                               bool bonded, bool trusted_peer);
typedef struct {
  bool standalone, gateway_online, gateway_image_valid, authenticated_status,
      status_fresh;
  uint32_t confirmed_release, requested_release;
  uint64_t confirmed_transaction, requested_transaction;
  uint8_t confirmed_digest[32], requested_digest[32];
} update_gate_t;
/* Caller supplies a snapshot from the authenticated recovery transaction
 * journal. */
CORE_API bool update_wheel_allowed(const update_gate_t *gate);
CORE_API uint32_t rpm_mask(uint32_t rpm);

/* Bounded ATT fragment decoder shared by firmware and native tests. */
typedef struct {
  uint8_t data[152];
  size_t size, expected;
  uint64_t started;
} recovery_assembly_t;
CORE_API int recovery_fragment(recovery_assembly_t *state,
                               const uint8_t *fragment, size_t length,
                               uint64_t now_ms);
CORE_API bool manifest_layout_valid(const uint8_t *data, size_t length);
