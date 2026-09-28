# Gateway telemetry and Ribbon UI integration, revision 1

This increment implements the existing gateway demo data path and the 320 x 172 Ribbon interface. It does not enable a vehicle decoder or CAN transmission. Recovery/control OTA v1 is unchanged and remains independent of this application telemetry.

## Implemented application boundary

The existing application characteristic remains exactly 16 bytes: four little-endian uint32 values, application major (1), sequence, synthetic RPM, gateway uptime milliseconds. Legacy v1 is explicitly a demo source; it cannot prove vehicle signal validity. A future vehicle schema must add explicit signal validity, sample age and session semantics through a separately reviewed application extension, without breaking recovery v1.

The new producer target is 100 ms. The gateway owns the 18-second RPM demonstration (800 to 7000, hold, return); the wheel has no independent simulated fallback. The wheel accepts only authenticated, compatible, well-formed, advancing samples, with bounded read latency. Sequence and uptime wrap use serial arithmetic; a new authenticated connection resets tracking. Duplicate samples do not refresh their timestamp. Receiver freshness remains 2500 ms for compatibility with the existing one-second producer. This is receiver-side demo freshness, not proof of absolute vehicle sample age.

Local shared state gains demo_telemetry_source_t / telemetry_source: NONE, GATEWAY_DEMO, VEHICLE. VEHICLE is reserved and never emitted by v1. Valid current zero displays 0; unavailable data displays --. RPM effects clear when data becomes invalid; explicit Service LED tests remain independent. Button pixel 0 on each chain remains button illumination.

## Execution and resource ownership

- BLE supervisor on core 0: bounded asynchronous application read, at most one in flight; session checks, maintenance suppression and no rendering. Recovery procedures retain their existing API and wire format.
- Wheel core 0 telemetry_view task: derives the LED RPM from the received snapshot and performs the existing ten-second boot-health validation. No local waveform.
- Wheel core 1 UI task: sole LVGL owner; precreated/reused objects and bounded transitions. Display buffers, SPI speed and existing task priorities unchanged.
- Built-in Montserrat 40 is enabled for the primary numeral, alongside existing 14. No embedded mockup images, additional framebuffer, glow canvas or PSRAM requirement.

## Navigation

Engine -> Shift -> Gateway -> Service is the main K1 cycle. Engine emphasizes the numeric RPM; Shift emphasizes the segmented band and shift cue. Both consume the same received value and show its Demo provenance. K2 enters Service; K1 cycles service entries and K2 opens the chosen detail. Hold K1 returns a hierarchy level. Diagnostics stay available without occupying the main cycle. Update retains deliberate K2 confirmation, candidate generation/digest binding and the existing Wi-Fi QR quiet zone. No UI action changes the permanent recovery contract.

## Assignments and acceptance

Designer owns docs/design/synthwave-v1; telemetry developer owns components/ble_link and its native tests; UI developer owns wheel_ui and its native model/view tests. Integrator owns shared headers, app composition, dependency configuration, build and this contract. One writer per file. No specialist may flash, erase, change eFuses or transmit vehicle frames.

Acceptance: native malformed/duplicate/wrap/session telemetry cases; real UI model navigation/freshness cases; screenshots from the actual LVGL renderer at native resolution; both pinned IDF 6.1 builds; existing recovery/OTA and LED regression checks. Physical BLE cadence, panel responsiveness and heap under OTA must be reported separately from desktop evidence. Existing r4 hardware success does not validate this new revision.
