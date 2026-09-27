# Implementation ledger
Plan: docs/architecture/wheel-demo-v0.1.md; recovery supplement: docs/architecture/ota-recovery-v1.md.

- Planning commit: 456c4fb. Implementation explicitly authorized after this commit.
- First increment: M1 build foundation and shared, host-tested recovery policy/codec. Remaining demo milestones are not yet complete.
- Ruling: use installed ESP-IDF v6.1 and a separate native C test compiler; Espressif's clang lacks the Windows x86 backend. Cost: a project-local Zig 0.14.1 development-only compiler dependency (TinyCC download was unreachable).
- Ruling: keep PSRAM disabled for first internal-RAM bring-up until mode is verified. Cost: smaller available memory in this increment; do not claim final animation performance.
- No hardware flash, erase, eFuse changes, GitHub publication or vehicle CAN transmission is performed.
- Pre-flight: recovery is independent of application protocol and paired wheel update requires confirmed gateway success. The codec is not yet a deployed transport; do not label paired OTA functional.

- RED: seven native C behavior tests failed against placeholder implementations; GREEN: 7/7 pass after implementing bounded validation, authenticated-link preconditions, gateway-first predicate and RPM mask.
- Build correction: IDF publishes IDF_VERSION as an environment variable; fixed the CMake guard to read that rather than an unset ordinary variable.

- Review fixes: trusted-partner regression failed then passed; fresh authenticated transaction and digest regressions failed then passed. Journal/transport are still not implemented and these helpers do not establish actual paired OTA.
- Review scope: endpoint BLE/UUIDs, replay transport, secret-buffer lifecycle, update journal, signing, boot confirmation and all display/peripheral/timing behavior remain unimplemented M2-M7 work, not verified results.
- Native setup is documented with pinned project-local Zig 0.14.1; dependencies are re-resolved in the final firmware build.

- M1 foundation verified: ESP-IDF v6.1 build succeeds with locked LVGL 9.4.0 and led_strip 3.0.3 compiled; application 165024 bytes. These libraries are not yet wired to hardware drivers/UI.
- Final checks: 13/13 native behavior tests pass; generated partition/rollback/security-policy audit passes; original backup SHA-256 remains c8f796755d535f5eb5bdb820e3499595982c86fcfb986f1eb7ec6508f27d3e45.
- Fresh independent review completed; all four P2 findings addressed (lock regeneration, trusted peer, transaction/digest/freshness checks, reproducible test setup). No deferred minor findings.
- Remaining: LCD/UI, actual peripheral drivers, BLE endpoints and immutable deployed ABI, Wi-Fi provisioning, signed OTA transport/journal, boot confirmation and hardware validation. Paired OTA is unavailable in this build.

- Hardware read-only checks: both boards identified, complete point-in-time flash snapshots verified; all observed differences confined to NVS. See [device report](2026-09-27-connected-devices.md). No custom firmware flashed.

## Two-device demo implementation,2026-09-27

Both custom applications now compile on ESP-IDF6.1. Wheel LCD/LVGL, buttons/hardware-bounded haptics, dual LED chains, optional BNO055, independent BLE application/recovery channels, Wi-Fi phone portal and signed-manifest paired/standalone OTA are implemented. Gateway CAN remains disabled. Source is awaiting independent review and hardware qualification; no custom image was flashed.

Final builds: wheel1,613,040 bytes in6MiB slot, gateway1,243,808 bytes in0x1e0000-byte slot.18 shared native-C tests and6 release-authenticity/target tests pass. Both-target partition/security/chip audit and immutable backup SHA pass. These are build/host results, not successful physical OTA or performance evidence.

Link-time free DIRAM164,774 bytes on wheel and free DRAM74,101 bytes on gateway are not runtime heap guarantees. PSRAM remains disabled; first boot must measure largest free block, stack margins, TLS coexistence and failure behavior.

Review [candidate wire contract](../architecture/recovery-wire-candidate.md), [initial flash/restore plan](initial-custom-flash-plan.md), and target READMEs. Hardware acceptance M2–M7 remains pending for actual displays, peripherals, pairing/reconnect, phone provisioning, OTA rollback/fault injection and endurance.

## Review round1 source corrections

Interrupted-update retry, replay/session dispatch, both-role auth deadline, boot-task liveness, sensor configuration validity, telemetry age, OTA result UI and reproducible native setup are corrected and await scoped re-review.26 native tests and7 real-image release tests pass, including a new clean pinned toolchain directory. Final images: wheel1,621,520 bytes; gateway1,251,136 bytes. Both build/partition/security/identity audits and original backup hash pass. No hardware write or live OTA evidence. Wheel link-time free DIRAM164,230 bytes and gateway free DRAM73,589 bytes remain pre-runtime figures. Flat diagnostic navigation, tilt graphic and fractional LED refinement are visual follow-ups; all physical acceptance remains pending.


## First authorized physical bench session

Source re-review of8c5fcde resolved all eight Important findings. User authorized real testing and confirmed USB-only, vehicle-disconnected setup. Both targets were flashed with hash-verified initial write sets. Both booted and authenticated BLE; one gateway-only and one wheel-only restart each recovered automatically. Three45-second capture windows recorded no panic/assert or unexpected reset. Display/peripheral observation, runtime resource/timing measurements, phone provisioning and actual OTA remain pending. See [physical bench report](2026-09-27-first-custom-bench.md). Earlier no-flash statements describe previous milestones.
