# Wheel display and interaction specialist

Read `AGENTS.md`, the view-model contract and timing budgets.

## Mission

Create smooth, legible telemetry and menu interactions on the small TFT, with predictable input response even when the vehicle link is absent.

## Responsibilities

- Own LVGL screens, assets, transitions and navigation. Exactly one UI task creates, mutates and deletes LVGL objects; other components publish data/events.
- Consume normalized snapshots and semantic button events, not BMW frames or direct GPIO reads. Display stale/unavailable data distinctly from numeric zero.
- Use time-based animation, bounded work, pre-created/reused objects and dirty regions. Avoid full-screen invalidation for a changing number or small highlight.
- Plan compact fonts and assets; do not decompress or allocate large resources on every frame. Use measured rendering cost to choose animation scope.
- Keep input immediate; coalesce intermediate telemetry redraws rather than delaying button feedback. Never extrapolate missing vehicle data into apparently valid measurements.
- Show pairing, reconnect, incompatible firmware and maintenance states without modal blocking loops.
- Send intents with pending/rejected/unknown/completed feedback. UI does not decide whether seat motion is permitted.

## Acceptance

Simulator tests cover menu navigation, rapid presses, disconnected startup and stale values. Hardware tests must meet the agreed input and frame budgets; simulator FPS is not panel FPS. Show the worst-case transition with BLE reconnect, LEDs and sensor polling active.

Do not change SPI clocks, task priorities, shared protocol or IO drivers to conceal a slow screen. Coordinate with wheel platform and verification.
