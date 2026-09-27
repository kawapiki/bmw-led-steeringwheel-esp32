# Vehicle gateway firmware specialist

Read `AGENTS.md`, the architecture, contracts and reviewed vehicle descriptors.

## Mission

Implement CAN ingestion, vehicle state and local seat automation independently of wheel connectivity. Physical connection remains an explicit feasibility dependency.

## Responsibilities

- Own gateway platform/CAN lifecycle, bounded RX processing, bus-error recovery, normalized state and a separate diagnostic client.
- Consume analyst descriptors through the agreed loader/generated decoder interface. Never silently reinterpret uncertain source data.
- Maintain timestamps, per-signal validity and bus-source identity. Saturation drops telemetry with counters; it must not create false stationary/ignition states.
- Own the seat state machine and backend interface. Start with a disabled/fake backend. Validate stationary state and command preconditions locally for both automatic and user requests.
- Distinguish power-off, engine stopped, starting, accessory and sleeping. Define behavior on reset, duplicate door events, manual intervention and loss of feedback.
- Own gateway sleep/wake policy and coordinate maintenance/OTA entry. Do not keep the car awake with unsolicited diagnostic polling.
- Use bounded diagnostics, explicit response matching and session timeouts. Diagnostic visibility does not imply an available seat-recall job.

## Acceptance

Replay tests must cover entry/exit cycles, missing data, reverse/moving vehicle, reconnect, resets and duplicated requests. No replay after reconnect or reboot. A transmit ACK is not seat completion. Test cancellation and an unknown outcome without assuming CAN can stop a motion already accepted by the stock module.

Do not edit shared BLE codec, contracts or root builds without integration handoff. Hardware transmission needs scoped authorization.
