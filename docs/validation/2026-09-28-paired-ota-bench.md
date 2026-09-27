# 2026-09-28 paired OTA bench validation

## Publication and preflight

- Pushed `codex/wheel-demo` through `944cfc2`; draft PR #1 opened for review (no merge).
- Published `system-r1` from that commit with signed `manifest.bin`, both application images and inspection metadata. Downloaded all three binary assets and compared SHA-256 successfully.
- Fresh host verification: 27 core tests, 7 release tests, HTTP listing harness (9 scenarios), 3 provisioning UI tests; both target/partition/security audits passed. Original backup hash unchanged.
- Both COM6 (wheel) and COM7 (gateway) available; passive simultaneous serial capture confirmed authenticated gateway recovery connection.

## First live attempt: release-asset redirect failure

- Device fetched the populated GitHub release listing (8715 bytes).
- Fetching the manifest then failed with `HTTP_CLIENT: Out of buffer`, `ESP_FAIL`, TLS error 0. No OTA installation was started.
- The unauthenticated GitHub manifest endpoint redirects to `release-assets.githubusercontent.com`; measured GET request line was 880 bytes. The pinned ESP-IDF6.1 defaults `buffer_size_tx` to 512 bytes, independently of the configured 2048-byte receive buffer.
- Set explicit TX buffer 2048 on the shared HTTPS client. Added state/boot-slot logging without URLs or credentials.
- `tests/host/test_http_redirect.py` compiles the actual pinned IDF request-line formatting body: the observed request fails at512, succeeds with the production setting; longer1800-byte query succeeds and oversized2200-byte query is rejected.
- Both current devices need this bootstrap correction before the first complete OTA test. USB correction is not counted as OTA success.

## Pending

- Rebuild and USB bootstrap corrected applications while retaining NVS, bond identity, partitions and backups.
- Publish immutable corrected release and physically test gateway-first OTA, new-slot boot health, wheel OTA and bonded reconnect.
- LED transport investigation is independent; source tests do not establish absence of physical flicker.
- Rollback/fault injection and mixed-application-version recovery remain unqualified until explicitly exercised and recorded.

Private raw bench logs remain ignored under `.test-build`; never commit credentials, NVS dumps or signing private keys.

## Corrected baseline and r2

- Commit `aaae163` fixes HTTP TX capacity and adds non-sensitive update/boot diagnostics.
- Commit `d72f5a0` integrates the separately reviewed complete-frame LED DMA transport; see the LED investigation report for evidence and limitations.
- Wheel and gateway builds succeed, image sizes 1,641,600 and 1,262,992 bytes respectively; target/partition/original-backup audits and signed-release tests pass.
- USB wrote only each application at0x20000, with esptool hash verification; NVS, bonds, partition tables and rollback metadata were retained. Wheel was explicitly reset once afterward. This is bootstrap, not OTA.
- Corrected wheel boot log: `boot self-test passed; slot=ota_0 address=0x20000`. Gateway log confirms authenticated recovery connection after both applications restart.
- Published signed `system-r2` from `d72f5a0`, pushed branch updates, and verified public downloads of all three binary assets against local SHA-256.
- User asked to initiate the r2 paired update via the physical menu while both private serial logs are captured. Live installation outcome is pending.

## Populated release-list memory bound

- Live r2 discovery failed before download with `Release response: no memory`; user confirmed the same message. No OTA partition switch occurred.
- With two release objects, the full response plus TLS working memory exceeded the available allocation budget. Changed listing pagination from five objects/page to one, preserving the60-release search window, and changed unknown-length growth from doubling to1024-byte increments. The24KiB hard response bound remains.
- Both rebuilds and image audits pass; the actual bounded-reader regression scenarios pass. Only wheel needs the additional USB bootstrap because the gateway does not enumerate releases.
- User's first physical LED observation with complete-frame DMA: no unexpected flicker so far. This is an initial observation, not endurance or electrical validation.
