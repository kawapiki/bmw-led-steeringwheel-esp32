# First physical custom-firmware bench session

Both devices were flashed after the user approved real tests in response to the concrete initial-flash proposal and confirmed that both boards were on USB, disconnected from the vehicle. Device access was exclusively owned by the coordinator. No CAN transmission, eFuse operation, whole-chip erase or release publication was performed.

## Tested configuration

- Source: 8c5fcdea25261bab14aada65b2e536529b354405, ESP-IDF v6.1, local Windows USB/pyserial and esptool 5.4.0.
- Wheel: COM6, ESP32-S3 QFN56 revision0.2, 32MiB flash, internal USB Serial/JTAG; PSRAM disabled in this build.
- Gateway: COM7, ESP32-D0WDQ6-V3 revision3.1, 4MiB flash, CH9102 UART bridge.
- Exact initial write set from initial-custom-flash-plan.md, including private pairing NVS and fresh otadata. Both apps run from ota_0 at0x20000.
- Preflight tools/check-system-build.py passed; immutable original wheel backup SHA remains unchanged. Full private pre-custom snapshots remain in back/local.

## Results

| Check | Result | Evidence / limit |
|---|---|---|
| Correct device identity | PASS | ROM chip identification before write |
| USB write integrity | PASS | Five regions per target, each esptool hash verified |
| First boot | PASS | Both entered app_main and continued;45-second capture per target |
| First authenticated BLE connection | PASS | Gateway logged recovery authenticated at14109ms boot uptime |
| Gateway-only restart | PASS, one trial | Reconnected automatically; authenticated at14052ms new gateway uptime |
| Wheel-only restart | PASS, one trial | Gateway resumed advertising at57061ms, authenticated by64052ms; sampled logging, not exact reconnect latency |
| Crash/reset observation | PASS within captures | No panic/assert/abort or unexpected boot sequence found; three45-second capture windows, not endurance qualification |
| Application telemetry | PARTIAL | Recurring GATT reads of application characteristic observed; displayed CURRENT/sequence/payload correctness needs UI observation |
| Display, LED, buttons, haptics | NOT VERIFIED | Drivers did not abort boot; visual/physical confirmation requested from user |
| Motion sensor | NOT VERIFIED | Physical identity, calibration and values not yet confirmed |
| Runtime FPS, heap/stack margin | NOT MEASURED | Performance page exists; no numeric observation obtained |
| Phone provisioning and HTTPS | NOT TESTED | Requires physical Update-menu action and phone/network interaction |
| Paired OTA and power-failure recovery | NOT TESTED | No release published and no live OTA performed |
| Vehicle functionality | NOT TESTED | CAN remains disabled and vehicle disconnected |

## Observations

COM6 initially failed to open with Windows error31 before any wheel write. A user USB reconnect restored ROM access. Subsequent two wheel boot captures, including software USB reset, succeeded without another reconnect.

Initial RF calibration warnings occurred with fresh NVS; the stack performed and saved full calibration. The RMT GPIO21 warning corresponds to the firmware configuring the motor output low with gpio_config before handing the same output to RMT. ESP-IDF gpio_config reserves the output; RMT sees that previous reservation and emits the warning while continuing setup. This explains the warning in source; physical haptic correctness remains unverified. No source change was made merely to hide the warning.

No timing distribution or high-load stability claim follows from these short captures. The secure-link flag follows the firmware authentication gate; this is not a security penetration test. Raw logs stay ignored locally and are not published.

## Artifact hashes

- `firmware/wheel/build/wheel_demo.bin`: `92c85c8fdbc0fa5845ecbe319c003a1f4261c0bea8b81dfffac955b5373254db`
- `firmware/gateway/build/gateway_demo.bin`: `e9ae5f1460eed96d7e555bed4534cadf2864600f5d38a594c905c64e1e940fc5`

## Local capture hashes

- `.test-build/gateway-first-flash.log`: `2da220e79d9ef25d47886968e69581ce3cbe3501802f0216417aaeb96bb23219`
- `.test-build/wheel-first-flash.log`: `99eaddee2660fbde889588aa22761f2b8cbd111e462b54075505fd6ed181ba83`
- `.test-build/COM6-first-boot.log`: `2e2768b4434a33afa883e54d0521243f8f3215ce12b0821e45bc962a69b2e47b`
- `.test-build/COM7-first-boot.log`: `60f29bd66e85e86f519e9b0eec7a50df9c35bfd74297523ab3cb1c4f2f80c541`
- `.test-build/COM6-gateway-restart.log`: `33a4e965ead277e76f112e2f21a6e92b8f32a303ebe290a4c55ca714964b0fbd`
- `.test-build/COM7-gateway-restart.log`: `b0907084de57769c7659803b79f0d2e620cc0e97ba345dc88c469fdceae96803`
- `.test-build/COM6-wheel-restart.log`: `5782d04165332789b5dd0c90296e152c5a3bc7bc7f830c345a9166ec846d901f`
- `.test-build/COM7-wheel-restart.log`: `f60201c91e2879a91ca143f571302e2f45d7c83a141abfb25d14ef48e675c79d`


## Owner correction: button LED is first

The owner corrected the physical order during bench testing: index0 on both chains illuminates the button, while indices1–23 form the RPM strip. Updated wheel_io applies that mapping to RPM and all diagnostic animation modes; the logical23-bit RPM mask is unchanged. README, architecture and peripheral-agent instructions now agree.

The corrected wheel application built successfully (1,621,504 bytes), passed the target/partition/backup audit and was flashed at0x20000 only. NVS, pairing, partitions and gateway firmware were preserved. Write hash verification passed; the25-second post-flash capture showed app_main completion and resumed application-characteristic reads without panic/assert/abort. Physical appearance after correction still awaits owner confirmation.

Corrected wheel SHA256: `e5fb47c611c05ac6a712dbbee6ca786591c16762a3ab249ce07caaec510dcded`. Local logs: `.test-build/wheel-led-order-flash.log`, `.test-build/COM6-led-order-boot.log`, `.test-build/COM7-led-order-boot.log`.


## Owner-observed peripheral and runtime checks

After the LED correction, the owner confirmed blue button illumination, K1 on the left, menu-page changes and short haptic feedback. The Bluetooth page showed Bonded link / app compatible, CURRENT, and a changing sequence value. These are owner observations, not instrumented latency measurements.

Performance-page readings reported by the owner: last flush approximately3500us, UI period approximately45000us, minimum heap82304 bytes (80.375KiB). Input-drop count was not supplied. Workload: Performance page with the paired demo running; no Wi-Fi provisioning/HTTPS/OTA stress had been established. Values are individual approximate displayed observations, not averages or percentiles.

Code interpretation: flush_us measures one partial-buffer transfer callback interval, not an entire frame. ui_period_us is the last interval between UI-loop heartbeat updates, quantized through millisecond demo_ms(); it includes LVGL handler work and scheduling. Thus45000us does not establish22FPS, although it is longer than the16ms responsiveness target and requires profiling. The Performance label refresh is itself throttled to33ms and snapshots preceding state, so the displayed value is not an unbiased sample distribution. Minimum heap is the historical total free-heap low-water mark, not the largest contiguous allocation or proof of TLS/OTA headroom. Performance acceptance remains open.


## UI-loop profiling and redraw correction

Owner supplied the missing input result: drops0, displayed UI period varying42000–47000us. Added UI-owned LVGL event counters and one serial summary every5 seconds (handler time, flush-wait time, flush count, loop mean and heap minimum). No ISR logging. This diagnostic logging remains enabled for bench work; timing includes instrumentation overhead, and handler measurements exclude the later serial log call.

A120-second pre-fix capture reproduced the delay on the default RPM page. The owner did not navigate to Performance during these captures, so the following comparison is RPM-page only. The LVGL handler averaged36.1ms while direct flush-wait averaged0.432ms per loop, indicating substantial rendering/refresh work rather than direct waiting for SPI completion. Source inspection confirmed unchanged titles and opacity styles were set every33ms and the body label invalidated a fixed296x102 area even with only two lines.

Changed page-only properties only when the page changes; unchanged body strings no longer invalidate the label. Body height now follows content with a102-pixel maximum, retaining the footer boundary. The animation duration, simulation rate, SPI clock, CPU clock and core ownership are unchanged. Also corrected the previously unused board button-pixel constant to0 and used it at the existing button-output call; this preserves the already confirmed physical mapping.

| Measurement | Before | After |
|---|---:|---:|
| Complete five-second windows |23|11|
| UI loop samples |2651|4126|
| Weighted mean loop period |43573us|13362us|
| Weighted mean LVGL handler |36113us|7773us|
| Mean direct flush-wait per loop |432us|20us|
| Range of five-second mean loop periods |43236–43823us|8904–17052us|
| Max handler after first startup window |41566us|27762us|
| Minimum free heap reported |82272 bytes|82272 bytes|

After capture lasted60 seconds; differing RPM phases and the two-second constant-RPM plateau affect these means. Approximately3.26x faster average UI-loop cadence is observed in this comparison, not3.26x FPS. This is not a p95/p99 result or proof of16ms worst-case frames, physical input latency or high-load Wi-Fi/OTA performance. The isolated startup handler maximum was46856us after the change. Performance-page values after the fix and visual layout acceptance still need owner confirmation.

Both diagnostic and corrected applications built and flashed with verified hashes. Final target/partition/backup audit passed. Only the wheel app at0x20000 was rewritten, preserving NVS/pairing and gateway. Final image1622240 bytes, SHA256 `3e1b5d04998057762ffe658926a47f7afe807d3752ff07fe9290b932956ac08d`. The final board-constant cleanup produced an identical binary to the measured after-build.

Local capture hashes:
- `.test-build/ui-profile-before.log`: `43c79d0929d27b53ee6b9a58b9c46e49fccad113a355e0ff80f6e564867928b3`
- `.test-build/ui-profile-after.log`: `7ffe5bf4bae8eb64a1e9c5d2578a7d9a08865e0c877dce755ac2dd42caf593db`
