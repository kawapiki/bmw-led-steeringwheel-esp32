# Specialized agent roster

Ten reusable roles support two firmware images. They are instructions to load into an agent task, not ten tasks on either ESP32 and not automatically installed Codex agents.

The first development increment is the [standalone wheel demo](../docs/architecture/wheel-demo-v0.1.md), including Wi-Fi provisioning and GitHub Releases OTA. The CAN/gateway and BLE roles are not prerequisites for its simulated telemetry demo. Planned shared `components/settings/` belongs to the Wi-Fi/update role; wheel `demo_source/` and `tools/release/` belong to the integrator.

| Role | Description | Exclusive implementation ownership (planned) |
|---|---|---|
| [01 Integrator](01-integrator.md) | Architecture, contracts, builds, board profiles, integration | Root build/CI, `components/contracts/`, `boards/`, firmware app entry points |
| [02 CAN analyst](02-can-analyst.md) | Capture analysis, validated descriptors, golden vectors | `vehicle_profiles/`, `tools/can/`, `tests/fixtures/can/` |
| [03 Vehicle gateway](03-vehicle-gateway.md) | CAN runtime, diagnostics, vehicle state, seat automation, power state | `firmware/gateway/components/vehicle/`, `firmware/gateway/components/gateway_platform/` |
| [04 BLE link](04-ble-link.md) | Both BLE endpoints, pairing, reconnect, protocol codec | `components/ble_link/`, `components/protocol_codec/` |
| [05 Wi-Fi and updates](05-wifi-updates.md) | Service mode, configuration, authenticated OTA | `components/service_wifi/`, `components/update_manager/` |
| [06 Wheel platform](06-wheel-platform.md) | ESP32-S3 runtime, LCD transport, DMA, task/resource integration | `firmware/wheel/components/wheel_platform/`, `firmware/wheel/components/display_port/` |
| [07 Wheel UI](07-wheel-ui.md) | Screens, animation, input navigation, data presentation | `firmware/wheel/components/wheel_ui/`, `assets/ui/` |
| [08 Wheel peripherals](08-wheel-peripherals.md) | LEDs, buttons, haptics, BNO055 interface | `firmware/wheel/components/wheel_io/`, `firmware/wheel/components/motion/` |
| [09 Verification](09-verification.md) | Integration tests, timing, fault injection, endurance | `tests/integration/`, `tests/performance/`, `tools/bench/`, `docs/validation/` |
| [10 Automotive UI designer](10-automotive-ui-designer.md) | Professional synthwave visual system, native-size layouts, interactions and implementation handoff | `docs/design/`; firmware implementation remains with07 |

Each implementation role owns its component-local tests. Analyst owns CAN fixtures; verification requests fixture changes rather than overwriting them. Integrator owns `AGENTS.md`, this roster, `docs/architecture/` and dependency locks. Research contributions go to a specifically assigned document to avoid conflicts.

## Work waves

1. **Contracts and foundation:** integrator fixes the first interface revision and build versions. Analyst inventories captures; verification defines acceptance harnesses. No hardware-dependent claims.
2. **Independent components:** gateway with replay input; BLE with two endpoint test apps; wheel platform with test patterns; wheel IO with mocks. UI can begin in a simulator once its view model is agreed. Wi-Fi may be developed with its radio policy stubbed.
3. **Cross-device integration:** BLE + gateway + wheel platform/UI using the same codec revision. Only the integrator changes application composition. Verify UI under reconnect and telemetry load.
4. **Hardware validation:** one worker holds the device/serial-port lease at a time. Physical CAN suitability is resolved before vehicle connection; seat output remains disabled until its backend is validated and the hardware action authorized.

The current environment supports four simultaneous agents total, including the coordinator. A practical wave is one integrator plus three specialists; rotate roles as dependencies clear. Do not start ten workers or duplicate hardware ownership.

## Task brief template

```text
Role: agents/<role>.md
Objective: one reviewable outcome
Owned files: exact paths
Contract revision: agreed revision/commit
Inputs: captures, fixtures, hardware availability and confidence
Dependencies: contracts or artifacts that must exist first
Acceptance: relevant tests and measured targets
Hardware authorization: none, read-only, or explicitly scoped operation
Handoff: use AGENTS.md format
```

The profiles do not select model IDs. Use the user's configured model and available concurrency. Agent parallelism is a development workflow; FreeRTOS tasks and CPU cores are a separate runtime design.
