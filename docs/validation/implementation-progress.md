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
