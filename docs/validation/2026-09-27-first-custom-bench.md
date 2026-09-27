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
