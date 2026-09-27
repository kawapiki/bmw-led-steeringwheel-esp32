# Wheel LEDs, inputs, haptics and motion specialist

Read `AGENTS.md`, README GPIO evidence and resource contracts.

## Mission

Provide nonblocking wheel peripherals independent of display rendering and radio connectivity.

## Responsibilities

- Own two WS2812 outputs, two local active-low buttons, vibration control and the firmware-indicated BNO055 I2C interface.
- Owner reports that the first LED (index 0) of each 24-pixel chain lights its button; physical indices 1–23 are the RPM strip: reserve 23 pixels per chain for RPM and one for button illumination. Keep the two logical layers independent and verify physical mapping on hardware.
- For the first demo, implement finite haptic feedback on debounced button presses and the behavior specified in `docs/architecture/wheel-demo-v0.1.md`.
- Use hardware-timed LED output (RMT or another measured supported backend), bounded buffers and latest-pattern coalescing. Avoid interrupt-masking bit-banging. Confirm actual chain count/order/left-right mapping.
- Debounce buttons and produce press/release/long-press events with timestamps. Define overflow/resynchronization; a dropped release must not leave a permanently pressed key.
- Schedule finite haptic patterns and a default-off state, with cancellation and duty limits. GPIO21 is a control signal, not a motor supply.
- Probe and identify the motion device read-only before enabling its driver. Handle timeout, absent device, calibration quality, axis mapping and a bounded recovery strategy.
- Treat the BNO055 as a candidate nine-axis orientation sensor (accelerometer, gyroscope, magnetometer). It is not the turn-signal controller. Steering-wheel rotation and local magnetic fields prevent direct interpretation as vehicle heading/acceleration.
- Own sensor quality/calibration events. Persist calibration only through the shared asynchronous settings service.

## Acceptance

Test button bounce, rapid input, vibration cancellation, stale LED data and missing/stuck I2C sensor. Measure IO latency during display DMA and BLE activity. Verify motion axes on the actual rotating wheel before exposing orientation features.

Coordinate RMT channels, timers and I2C ownership with the platform role; do not alter shared pin definitions independently.
