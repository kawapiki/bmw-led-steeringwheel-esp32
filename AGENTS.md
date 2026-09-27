# Repository agent instructions

## Purpose and current stage

Build a two-device BMW E-series accessory: a vehicle gateway on a LILYGO ESP32 CAN board, and a responsive steering-wheel interface on the existing ESP32-S3 board. Initial vehicle: 2007 E90, N52B25, without iDrive.

Implementation has started with the wheel firmware build foundation. Track completed and pending milestones in docs/validation/implementation-progress.md; do not confuse a successful build with a complete demo or hardware validation. See [architecture](docs/architecture/system-design.md), [contracts](docs/architecture/contracts.md) and the [agent roster](agents/README.md). These Markdown roles are repository instructions, not registered desktop agents or an automatic scheduler.

## Constraints

- Vehicle connection location is flexible. Prefer the existing LILYGO without hardware modification; all required functions through a compatible bus remain a feasibility question.
- Treat README GPIO assignments as firmware-derived, not a continuity-tested schematic. Keep unknown physical wiring and sensor identity explicit.
- Never modify `back/steering-wheel-original.bin`. Expected SHA-256: `c8f796755d535f5eb5bdb820e3499595982c86fcfb986f1eb7ec6508f27d3e45`.
- Ordinary development tasks do not authorize erasing/flashing devices, changing eFuses, or transmitting vehicle-control frames. Establish authorization for the actual hardware action; do not ask again for an action already authorized in the session.
- Research IDs, synthetic frames and unverified DBC fields must not become enabled vehicle commands.
- No claim of measured performance, successful pairing, working seat recall or electrical compatibility without relevant evidence.
- User instructions take precedence over these repository conventions.

## Assignment and parallel work

Choose the relevant role from `agents/README.md` and read its file before starting specialized work. Role files are additive to this file. One worker may cover several roles sequentially; a role is not a permanent running process.

Every parallel assignment specifies objective, owned files, interface revision, dependencies, acceptance criteria and prohibited hardware actions. Only one worker writes a given file at a time. Existing user changes must be preserved.

Use the integrator for shared contracts, dependency versions, board definitions, task/core allocation and root build configuration. Other roles propose changes to these through a handoff instead of editing concurrently. Backend and display agents may work against agreed fake interfaces while hardware availability is unresolved.

New source directories listed in the roster are planned ownership boundaries; do not create an entire firmware scaffold merely to satisfy the roster. Break work into runnable, reviewable increments. No background cross-chat coordination is implied.

## Engineering rules

- Permanent requirement: keep the OTA recovery/control protocol backward compatible and independent of application protocol compatibility. Breaking changes require explicit developer authorization. Follow `docs/architecture/ota-recovery-v1.md`; authenticated Wi-Fi credential sharing and recoverable gateway-first updates are part of the two-device architecture.

- Keep BMW-specific decode logic out of the steering-wheel UI.
- Use bounded queues, explicit ownership, timestamps and invalid/stale states. Unknown speed is not zero speed.
- Network callbacks and interrupts enqueue minimal work. They do not render, write flash, perform blocking I/O or drive vehicle actions directly.
- Exactly one UI task owns LVGL objects. DMA completion follows the pinned LVGL/ESP-IDF version's supported mechanism.
- Seat automation executes locally on the gateway after validating current inputs. BLE reconnect must not replay old motion requests.
- Operational BLE traffic has priority over optional Wi-Fi service work. OTA is a separate maintenance state.
- User requires ESP-IDF 6.x for custom firmware. Pin v6.1, verified as the latest stable release on 2026-09-27. Adapt dependencies to its APIs; do not silently downgrade to 5.x.
- Pin dependency versions when the build is introduced. Do not inherit a library version merely because the original binary used it.
- Test at the appropriate boundary: decoder vectors, state machines, endpoint interoperability, then hardware timing. Mock success is not hardware validation.

## Handoff format

Report: changed files; contract revision; evidence and confidence; tests actually run and their results; resource/performance measurements; open limitations; next integration step. Distinguish proposed targets from measurements. Keep logs and secrets out of public commits unless deliberately reviewed for publication.
