# Wheel firmware foundation (M1)

This is a buildable bring-up foundation, **not the complete v0.1 demo**.
It initializes the motor control to OFF and emits synthetic RPM/heap logs on native USB.
No LCD, LEDs, buttons, motion sensor, BLE service, provisioning or OTA transport is active yet.

The shared C component contains a bounded recovery-envelope/credential validator,
a transaction-bound gateway-first update eligibility predicate and an RPM mask that excludes pixel 23.
The validator is not a complete authentication implementation: the future BLE adapter must
supply verified authorized-peer identity, encryption and bonding state, and enforce session/transaction
freshness before calling command handlers. The update predicate is not a transaction journal.
Do not connect these helpers directly to flash/actuator operations.

## Build

Use ESP-IDF **v6.1** and the committed component lock. On this Windows workstation:

```powershell
.\tools\build-wheel.ps1
```

Or activate your own v6.1 installation and run:

```text
idf.py -C firmware/wheel build
```

The wrapper accepts -IdfPath and -IdfToolsPath. Its default Python environment matches
the installation used here; other installations should activate IDF themselves.

Build products remain ignored in firmware/wheel/build. Two 6 MiB application slots
and rollback support are configured. This image is not a release or a safe initial-flash
package: board checks, signing, first-boot confirmation and recovery procedures are still
required. Do not infer permission to flash from a successful build. PSRAM is intentionally
disabled pending board-mode verification; the original backup is unchanged.

## Native behavior tests

Install a project-local development compiler (Windows x64):

```text
python -m pip install --target .test-build/zig ziglang==0.14.1
python tests/host/test_core.py
```

HOST_CC may point to a compatible Zig executable. Tests compile and load the production C
as a host DLL and exercise real functions; no radio or device is accessed.
Firmware compilation and native tests do not establish hardware correctness.

## Next milestones

- M2: LCD/DMA and LVGL UI on CPU1.
- M3/M4: buttons, RMT LEDs, bounded haptics, sensor and animated menus.
- M5/M6: phone provisioning, signed GitHub OTA and boot confirmation.
- Paired OTA additionally needs the permanent authenticated recovery BLE service,
  gateway implementation, transaction journal and mixed-version recovery tests.

See ../../docs/architecture/wheel-demo-v0.1.md and ../../docs/architecture/ota-recovery-v1.md.
