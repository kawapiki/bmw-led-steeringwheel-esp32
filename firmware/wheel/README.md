# Wheel demo firmware

Build target ESP32-S3, ESP-IDF6.1, LVGL9.4.0, led_strip3.0.3 and cJSON1.7.19. The firmware now implements the LCD port, animated two-button menu, simulated RPM, LED diagnostics, finite haptics, optional BNO055, authenticated BLE, phone Wi-Fi setup and authenticated paired/standalone OTA. It has not been flashed or runtime-qualified.

From repository root:
1. Run ESP-IDF Python on tools/provision-development.py to create private local signing material and initial pairing-NVS source.
2. Run tools/build-wheel.ps1; gateway uses tools/build-gateway.ps1.
3. Run tools/check-system-build.py, tests/host/test_core.py and tests/host/test_release.py.
4. Review docs/validation/initial-custom-flash-plan.md before any hardware action.

K1 short cycles pages; K2 activates page action; K1 hold returns home/cancels; K2 hold confirms a newly offered update after button release. LED page K2 cycles RPM, first chain, second chain, chasing pixel and red test. Buttons page K2 requests bounded haptic. Update mode page explicitly selects paired(default) or standalone. Update page K2 opens service AP or checks Releases. After successful Wi-Fi provisioning the worker starts a release check.

Phone connects to BMW-Wheel using the random password shown on TFT, then opens http://192.168.4.1. Scan lists up to12 networks; enter SSID/password manually, including hidden SSIDs. The portal is bound logically to the AP destination, protected by the AP WPA2 password and request token, and expires after5 minutes. Credentials persist only after confirmed association/IP and can be forgotten from the portal.

UI is single-owner core1, partial RGB565 double DMA buffers(30KiB), LVGL16ms refresh target,32KiB LVGL heap. No claim of measured FPS. RMT allocation: motor48-symbol hardware pulse; first LED chain DMA on S3's only DMA-capable TX channel; second chain96-symbol non-DMA channel. Initial LED limit10%. IO quiesces before OTA flash operations. Display offsets and orientation remain firmware-derived candidates in board header.

Normal wheel demo does not consume CAN frames. BLE application telemetry is explicitly synthetic gateway data. Recovery is separate and remains available when application-major compatibility fails.

OTA assets are release-tag system-rN with signed manifest.bin and target wheel.bin/gateway.bin. tools/release/package.py validates chip/project, hashes exact images and signs the common envelope. Public verification key is compiled in; pairing secret is only in private provisioned NVS. Never publish private provisioning images or signing keys.

Hardware acceptance remains: LCD orientation/color, button polarity, motor direction/driver, LED physical order, sensor identity, pairing/reconnect, phone browsers, OTA/rollback power-fault tests, heap/latency/endurance.
