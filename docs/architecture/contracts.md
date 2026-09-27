# Shared component contracts

Status: design draft v0. These are logical contracts, not a frozen wire ABI or existing code. The integrator must publish revision v1 before implementations depend on concrete layouts. See [system design](system-design.md) and [ownership](../../agents/README.md).

## CAN evidence and descriptors

The analyst owns a reviewed YAML source profile; generated DBC and firmware tables must be reproducible from the same source. A DBC export must not silently discard validation or command restrictions; keep metadata alongside it. No invented signal or seat command becomes vehicle-validated merely because it parses.

| Record | Required information |
|---|---|
| Capture | Vehicle/variant, bus, bitrate, adapter, capture clock, date, operating scenario, drop count, file hash, provenance |
| Frame definition | CAN identifier, standard/extended format, DLC, direction, profile compatibility, source references |
| Signal | Start bit and explicit bit-numbering convention, length, signedness, byte order, factor/offset, unit, valid range, invalid encodings, multiplexing |
| Integrity | Counter/checksum algorithm and coverage, supported vectors; explicitly unknown if unverified |
| Timing | Expected period, freshness limit, timeout behavior, event versus periodic semantics |
| Confidence | Candidate, source-supported, or vehicle-validated; evidence and RX/TX confidence separately |
| Command | Preconditions, parameter bounds, addressing/session needs, timeout, acknowledgement meaning, cancellation capability, explicit enable status |

Normalize Motorola bit numbering in the generator and test cross-byte signed values. Include known-good vectors, malformed/short frames, invalid values, wraparound counters and independent held-out captures. Raw capture preservation and descriptor edits are separate operations.

## Runtime domain records

| Logical type | Semantics |
|---|---|
| `RawCanFrame` | Bus/profile identity, CAN ID/format, DLC/data, monotonic receive timestamp, capture/drop metadata |
| `SignalValue` | Typed normalized value and unit, valid/invalid/stale/unsupported status, reason, sample time, freshness, source sequence |
| `VehicleSnapshot` | Gateway boot/session identity, generation, independent signal validity and age; missing fields never imply zero |
| `WheelInput` | Button identity, press/release/hold event, sequence and local timestamp; resynchronization state after overflow |
| `UiIntent` | High-level user intent such as requesting a supported memory recall; no raw CAN bytes |
| `CommandRequest` | Authenticated session, request ID, supported operation and bounded parameters, age/expiry information |
| `CommandResult` | Matching session/request ID, accepted/rejected/busy, then completed/failed/unknown when supported, with reason |
| `MotionSample` | Sensor identity, frame of reference, timestamp, calibration/quality, units, valid fields; no implicit vehicle-frame conversion |

Monotonic clocks on separate devices are not directly comparable. Receivers track local receipt time plus reported sample age and a defined conservative transit allowance; mark data stale if freshness cannot be established. For timed commands, v1 must define session time synchronization and uncertainty bounds, or another provable maximum-age mechanism. A receiver-side TTL alone must not make an old queued command fresh again. Reject execution when age cannot be bounded.

Receipt, application acceptance and physical completion are distinct. A GATT acknowledgement or CAN ACK is not an actuator completion report. If the vehicle provides no reliable completion signal, return unknown rather than completed.

## BLE application protocol

- Gateway is GATT server/peripheral; wheel is client/central. The BLE role implements both ends.
- Pairing requires a deliberate mode; reconnect uses the known authenticated peer, including identity resolution where applicable. Authentication strength and first-pairing UX must be fixed in v1 using supported security mechanisms.
- Logical messages: hello/capabilities, state snapshot/update, heartbeat, command request/result and error. Assign service/characteristic UUIDs and numeric fields centrally in v1.
- Major-version incompatibility prevents operational Ready. Minor additions require explicit capabilities and length-delimited optional fields.
- Use explicit binary serialization with defined little-endian fields and lengths, not raw C struct copies. Both devices use the same codec and test vectors.
- Work at the default ATT MTU of 23 as well as negotiated larger MTUs. Bound each logical message to 512 bytes initially, including its application envelope; define fragmentation headers and per-fragment limits in v1.
- Initially allow one bounded in-progress reassembly per direction, with a one-second timeout and validation of length, index, session and duplicate fragments. A fragmented message cannot allocate arbitrary memory.
- Start Ready only after authentication, capability negotiation, subscriptions and a fresh complete snapshot. Link-connected alone is insufficient.
- Coalesce telemetry to the latest value, initially up to 20 updates/s. Do not stream every raw CAN frame over the operational link. Service capture is a separately budgeted feature.
- Allocate a new session on reconnect/reboot. Reject old-session commands and bound the deduplication cache by count and expiry. Never replay an ambiguous motion request automatically after a disconnect or reset.
- A completion lost during disconnect remains unknown until authoritative state resolves it. Exactly-once physical execution across crashes is not promised.

## Ownership, queues and overload

Initial capacities below are tunable budget proposals. The platform owners must calculate bytes including RTOS overhead and prove they fit both boards; do not assume PSRAM on the classic ESP32 gateway.

| Boundary | Initial capacity/policy | Overflow behavior |
|---|---|---|
| CAN receive → decoder | 256 frames, bounded FIFO | Count drops; invalidate any continuity-dependent decoding; no blocking ISR |
| Decoder → published state | Latest snapshot with single-writer ownership | Replace obsolete telemetry |
| Inputs → UI | 32 events plus current button state | Report loss and resynchronize; never leave a stuck-held button |
| Command ingress | 16 requests maximum | Explicit busy/reject; never silently drop an accepted request |
| Command results | 16 pending slots, reserved at acceptance | Backpressure/reject new work if no result capacity |
| Telemetry → BLE/UI | Latest-value mailbox | Replace older unsent state; preserve generation and freshness |
| LED state | Latest desired pattern with expiry | Discard obsolete patterns; enforce expiry |
| Haptic requests | Bounded finite patterns, interruptible | Default off after expiry/fault; no unbounded motor-on command |

Only one writer owns each state object. Use immutable snapshots, queues or a short documented synchronization mechanism; never pass a pointer to mutable temporary storage. Critical sections contain no render, flash or bus operations. Blocking waits have bounded timeouts.

## Wheel component interfaces

- Platform owns display initialization, SPI bus, DMA buffers, flush completion, backlight, task allocation and watchdog integration.
- UI owns LVGL objects and rendering. It consumes a view model and input events; it emits intents and optional local feedback requests.
- A display buffer remains owned by transport until completion. UI must not draw into an in-flight buffer. Follow the pinned LVGL/IDF integration contract for completion context.
- Peripheral service owns button debounce, RMT LED output, finite haptic patterns and sensor transactions. Sensor failure cannot delay buttons or rendering.
- Integrator owns the board profile with GPIOs, active levels, bus ownership and evidence. Component code references that profile rather than duplicating constants.
- Flash, PSRAM and DMA memory have explicit budgets. UI assets may live in flash/PSRAM; DMA buffers use memory proven compatible with the selected driver configuration.

## Gateway and service-mode interfaces

Vehicle backend supports replay, listen-only hardware and separately enabled control modes. The seat policy consumes validated state and requests a supported high-level backend operation; no BLE callback sends a seat frame directly. Diagnostic sessions and polling have explicit bus-load and timeout budgets.

Power management belongs to the gateway platform: detect relevant wake/sleep conditions, bound awake time and radio activity, and measure standby consumption before permanent installation. Never assume vehicle power is removed when ignition turns off.

Wi-Fi service entry is a state transition, not an arbitrary background task. Require a quiescent vehicle-action state, block new motion actions, then start authenticated configuration/update services. A request to enter service mode must not assume an ongoing seat movement can be cancelled. Wi-Fi code cannot directly access CAN transmit APIs or LVGL objects.

OTA verifies target, image authenticity, version compatibility and partition capacity before activation. Define boot self-test, rollback and power-loss behavior for each device. Keep the original backup immutable; introducing custom partitions is a separate reviewed operation.

## Integration acceptance

Before combining independently developed components, verify matching contract revision, shared golden codec vectors, invalid/stale input behavior, default-MTU fragmentation, both boot orders, reconnect under UI load, duplicate/expired commands, result-capacity exhaustion, missing sensor, queue overflow and memory bounds. Hardware tests report their actual setup and measurements. A simulator pass does not establish real bus compatibility, physical display latency or seat operation.
