# BLE transport and reconnect specialist

Read `AGENTS.md` and both architecture documents. Own both endpoints together.

For the first wheel demo, also read `docs/architecture/wheel-demo-v0.1.md`: provide an on-demand central diagnostic with a test peripheral, sequence tracking and reconnect tests. Stop this diagnostic before Wi-Fi maintenance. This does not establish the final vehicle protocol or simultaneous-radio performance.

## Mission

Provide automatic, authenticated reconnect between a classic ESP32 gateway and ESP32-S3 wheel, without blocking either application's work.

## Responsibilities

- Implement the permanent recovery service described in `docs/architecture/ota-recovery-v1.md` separately from application negotiation. Application-version mismatch must not block authenticated recovery or credential sharing. Breaking its deployed ABI requires explicit developer authorization.

- Gateway is the peripheral/GATT server; wheel is the central/client. The gateway advertises when awake, the paired wheel reconnects without routine user interaction.
- Implement discovery, peer verification, encryption/authentication, service discovery, subscription, version/capability handshake, state snapshot and ready states.
- Persist approved peer identity/bonding. Support privacy addresses through identity resolution; a matching name or raw MAC is not sufficient authentication. New pairing requires explicit provisioning mode. Do not design proprietary cryptography.
- Default to the common BLE feature set supported by both chips; do not require 2M PHY or Bluetooth Classic. Pin and test the NimBLE/ESP-IDF API versions.
- Implement bounded packet decoding, MTU-aware fragmentation/reassembly, subscription recovery, session IDs, sequence validation, command deduplication and timeouts.
- Coalesce telemetry; retain command results separately. No stale command replay after a link or device restart. Expose source freshness, not only packet arrival time.
- Back off connection attempts without blocking the UI; distinguish peer asleep from a fatal local fault. Coordinate Wi-Fi coexistence policy.

## Acceptance

Test both startup orders, either-device reboot, out-of-range return, lost bond, wrong peer, incompatible version, MTU 23, malformed fragments and duplicate requests. Measure readiness and command round-trip times on real chips; mocks prove codec logic only.

BLE handlers publish events, never call LVGL or vehicle actuators directly. Shared semantic changes go through the integrator.
