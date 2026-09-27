# Immutable OTA recovery channel v1

Developer requirement, 2026-09-27: application telemetry may evolve, but a protocol mismatch must not strand the hidden gateway. OTA control must remain backward compatible. Breaking this channel requires explicit developer authorization and a reviewed migration/recovery procedure; ordinary feature approval is insufficient.

## Separation

Both devices retain a dedicated BLE recovery service independent of application protocol negotiation. Its UUIDs, framing, authentication, commands, status codes and credential encoding become permanent v1 contracts. An incompatible application peer still discovers, authenticates and uses recovery. Do not gate recovery on application Ready, telemetry schema or UI version.

BLE pairing/bond identity and recovery keys survive normal OTA and configuration migration. Credentials are sent only to the authenticated, encrypted, bonded partner; never broadcast, logged, placed in URLs or accepted from an unpaired client. Pairing/recovery must not silently fall back to unauthenticated access.

## Coordinated update

1. Wheel connects to configured Wi-Fi, obtains a verified release manifest containing both targets and their application protocol ranges.
2. Wheel queries gateway recovery status even when application versions disagree.
3. Following the user's update confirmation, wheel transfers network credentials and a transaction-bound release reference through the recovery channel. The gateway validates bounds, independently authenticates its manifest/image and joins Wi-Fi.
4. Gateway downloads to its inactive slot, verifies, reboots and confirms its self-test. The wheel waits for recovery status confirming the intended image is running and valid.
5. Only then may the wheel update itself. No simultaneous blind reboot. Failed/absent gateway stops the paired update; standalone wheel mode must be explicitly selected and cannot be presented as a successful paired update.
6. If either side disconnects, rebooted peers query transaction status; they do not restart or repeat an installation blindly. Persist transaction ID, target digest and phase without persisting plaintext credentials in the transaction journal.

This is recoverable sequencing, not an atomic two-device transaction. If the gateway update succeeds but the wheel fails, application functions may pause while recovery remains available. Rollback keeps compatible settings and the v1 recovery service. A successful GATT write is not confirmation of successful installation.

## Wire contract and extensibility

Use a fixed independent service and characteristic set, explicit little-endian encoding and bounded payloads. Freeze the first published UUIDs and golden byte vectors before both production endpoints are released. The first implementation may develop this ABI but must not label an untested transport deployed/frozen.

Required operations: HELLO, STATUS, WIFI_CONFIG, PREPARE_UPDATE, START_UPDATE, CANCEL_BEFORE_COMMIT and RESULT. Include session ID, request ID, transaction ID and explicit lengths. Credentials support SSIDs up to 32 bytes and WPA2 passphrases up to 63 bytes; their lifetime and zeroization are explicit. Bound fragmented messages, authenticate before assembly of sensitive content, reject unsupported mandatory fields, and return stable errors. Additions use optional capabilities/TLV fields; existing messages retain their meaning.

Recovery major v1 remains supported by all future images. New application protocols cannot repurpose its UUIDs or opcodes. A proposal to change v1 must include developer approval, mixed-version tests and a bridge release that preserves reachability before any old path is removed. No eFuse or irreversible security-policy migration is part of this demo.

## Required tests

- Old wheel/new gateway and new wheel/old gateway, with intentionally incompatible application protocols: recovery discovery, authenticated credentials and update control still function.
- Reset/disconnect at each update phase; status query resolves success, failure or unknown without replaying a motion command.
- Gateway validates wrong-device, invalid-signature, rollback and stale-transaction cases independently of wheel claims.
- Credentials never appear in logs/fixtures; spoofed or unbonded peers cannot receive them.
- Release CI freezes v1 byte vectors and service IDs. Changes to those fixtures require explicit developer approval; tests are not rewritten to hide a break.

The first wheel-only milestone can implement and host-test shared codec/policy before a gateway is available. It must report paired OTA as unavailable until both actual endpoints, authenticated transport, journal and rollback have passed the tests above. Never replace this with simulated success.
