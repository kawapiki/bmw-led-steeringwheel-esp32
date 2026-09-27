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

typedef enum {
  UPDATE_IDLE = 0,
  UPDATE_PREPARED = 1,
  UPDATE_DOWNLOADING = 2,
  UPDATE_BOOT_PENDING = 3,
  UPDATE_VALID = 4,
  UPDATE_FAILED = 5
} update_phase_t;
typedef enum {
  RECOVERY_OK = 0,
  RECOVERY_SESSION = 1,
  RECOVERY_REPLAY = 2,
  RECOVERY_STATE = 3,
  RECOVERY_STORAGE = 4,
  RECOVERY_NETWORK = 5,
  RECOVERY_IMAGE = 6,
  RECOVERY_CANCELLED = 7,
  RECOVERY_BUSY = 8
} recovery_result_t;
typedef struct {
  uint32_t schema, phase, release, size;
  uint64_t tx;
  uint8_t hash[32];
  uint32_t target_address;
} update_journal_t;
typedef struct {
  uint32_t nonce, last_request;
  bool established;
} recovery_session_t;
CORE_API int recovery_dispatch_accept(recovery_session_t *guard,
                                      const uint8_t *frame, size_t length,
                                      uint32_t connection_nonce);
CORE_API uint32_t update_reconcile_phase(uint32_t phase, bool pending_verify,
                                         bool running_matches);
CORE_API int update_prepare(update_journal_t *journal, uint32_t release,
                            uint64_t tx, const uint8_t digest[32],
                            uint32_t installed_release);
CORE_API int update_coordinator_action(uint32_t remote_phase,
                                       bool same_transaction, bool active);
/* coordinator actions: 0 new transaction/retry, 1 prepare/start existing, 2
 * wait, 3 ready */
CORE_API bool recovery_auth_expired(bool connected, bool authenticated,
                                    uint64_t since, uint64_t now);
CORE_API bool recovery_health_ready(uint64_t now, uint64_t worker_heartbeat,
                                    uint32_t worker_cycles,
                                    uint64_t host_heartbeat,
                                    uint32_t host_cycles);
CORE_API bool telemetry_is_fresh(bool connected, bool compatible,
                                 uint64_t received, uint64_t now);
CORE_API bool candidate_matches(uint32_t current_generation,
                                const uint8_t current_digest[32],
                                uint32_t generation, const uint8_t digest[32]);
CORE_API bool recovery_fragment_expire(recovery_assembly_t *state,
                                       uint64_t now);
CORE_API unsigned update_progress_percent(uint32_t received, uint32_t total);
typedef int (*sensor_write_fn)(void *ctx, uint8_t reg, uint8_t value);
typedef int (*sensor_read_fn)(void *ctx, uint8_t reg, uint8_t *value);
typedef void (*sensor_delay_fn)(void *ctx, unsigned ms);
CORE_API bool sensor_configure(void *ctx, sensor_write_fn write,
                               sensor_read_fn read, sensor_delay_fn delay);

CORE_API int portal_request_accept(uint32_t previous, uint32_t incoming,
                                   bool same_payload);
