# 2026-09-28 paired OTA bench validation

## Final outcome: paired r4 OTA passed

Both devices completed real GitHub HTTPS OTA to release4, with gateway-first authenticated sequencing, valid boot confirmation and bonded reconnect. Owner confirmed the menu and both buttons work after OTA and no unexpected LED flicker is observed. CAN remains disabled. This establishes the successful paired path and observed recovery from interrupted downloads; deliberate bad-boot rollback, mixed-application-major recovery, endurance and in-vehicle qualification remain open.

Earlier pending/failure entries below are the chronological investigation, not the final outcome.

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

## r3 discovery and gateway OTA (live)

- Published immutable signed `system-r3` from `838233e`; public downloads match all three local binary hashes.
- Corrected wheel discovery fetched pages of8758,9001,8949 and2 bytes, then authenticated the manifest: `Release 3 ready. Hold K2.` Minimum observed wheel heap after HTTPS discovery:13196 bytes. This remains tight headroom, not a future resource budget.
- User physically confirmed paired installation. Wheel logged `Gateway first; waiting for verified boot` at96950ms, with gateway download progress to100%.
- Gateway validated its image and restarted. Boot selected `ota_1` at0x200000; at4529ms its boot self-test passed, then at4980ms it logged `Installed release 3 successfully`.
- Wheel waited for that authenticated result and only then logged `Downloading authenticated image` at163650ms. This proves the gateway-first ordering on these devices.
- Gateway reconnected with authenticated recovery after reboot. One early NimBLE advertising validation error (`rc=6`) was observed during bootstrap/reconnect, so these logs are not claimed entirely warning-free.
- Wheel transfer continues. Two HTTP read-wait warnings were observed; UI task continues and progress redraws occur. Wheel success is not yet claimed.

## First wheel download failed safely; retry pending

- Owner reported wheel progress at32%. The transfer later emitted another HTTP read-wait/errno11 warning and logged `Update failed; confirm again to retry` at354826ms (approximately191s after wheel download start).
- No wheel OTA reboot/slot switch occurred. Its existing ota_0 application continues running; the incomplete inactive-slot write was aborted. Gateway remains on validated release3 with authenticated recovery connection.
- Source review confirms idempotent gateway reuse: `update_prepare` accepts the already-valid image at the installed release floor when its SHA-256 matches, rebinding the new transaction without installing again. Existing native journal tests cover this state.
- Owner asked to recheck and confirm release3 again. End-to-end paired completion remains pending; do not label r3 fully hardware-validated yet.
- Performance follow-up: both target defaults use4 static Wi-Fi RX buffers while IDF's default RX BA window is6. IDF6.1 Kconfig recommends static buffers >= the BA window for throughput/compatibility. This is a source-level tuning concern, not a proven cause of this timeout; no radio setting was changed during the live OTA.

## Retry behavior and network limitation

- On user retry, wheel logged gateway-first wait at605699ms and began its own download at606500ms. Gateway did not download/reboot again: the already-valid release3 image was accepted for the new transaction. This idempotent reuse is now observed on the devices.
- The wheel's second attempt failed opening the redirected server connection (TLS connection timeout, no certificate-verification error). At failure it reported68880 bytes free heap /31744 largest block, so this was not the earlier response-allocation failure.
- Last Wi-Fi association reported RSSI -76dBm. Owner asked to bring the access point and wheel closer and retry; weak RF is a candidate cause, not conclusively proven.
- The current wheel application continues running on ota_0. Gateway remains validated on ota_1/release3. Full paired completion remains pending.

## Third attempt and bounded-network correction

- Owner moved the access point/wheel closer; new association RSSI -68dBm. Discovery succeeded and the valid gateway was reused again. Wheel download still timed out and aborted at877125ms. Stronger RSSI alone did not resolve it.
- Added a native regression harness that compiles the actual OTA read/write decision block. Data -> `-ESP_ERR_HTTP_EAGAIN` -> data failed with the original implementation and passes after the bounded retry. Permanent error, EOF, oversized chunk and repeated-timeout cases still fail without writing invalid bytes.
- Retry permits at most two consecutive EAGAIN responses while time since last data is under30s; a blocking read can cross that threshold before it returns. Final SHA-256/signature/identity and boot validation remain mandatory.
- Corrected Wi-Fi RX BA window to4 to match the four configured static RX buffers in both targets, per pinned IDF6.1 Kconfig recommendation. Added a build audit enforcing this relationship. This tuning and retry improve known software behavior; attribution of all RF/network stalls remains unproven.
- Added10% progress, bytes, free/largest heap and stopped-read diagnostics. No credentials or signed download URLs are logged.


## Successful r4 hardware run

- Source `66a5887`; both target builds, image/partition/Wi-Fi configuration audit, original backup SHA, core/LED/HTTP/retry regressions and7 real-image release tests passed. Published signed `system-r4` and compared downloaded manifest/wheel/gateway SHA-256 to the local package.
- Wheel application was USB-bootstrapped at0x20000 with the network correction; gateway remained on its previously OTA-installed r3. USB preparation is not counted as OTA.
- Wheel boot confirmed RX BA window4 and valid ota_0 baseline. User initiated r4 through the physical menu; discovery/authentication took about23.4s for four release objects plus the empty terminal page.
- Gateway downloaded/verified r4, rebooted into ota_0 at0x20000, passed boot self-test at4523ms, and reported `Installed release 4 successfully` at4983ms. It had been running r3 from ota_1, so this also exercised the opposite slot.
- Wheel began its own download only afterward at130797ms. All1642192 bytes arrived by178565ms; verification/restart was logged at180100ms (49.3s from download start through validation).
- Wheel bootloader selected ota_1 at0x620000, boot self-test passed at11210ms and `Installed release 4 successfully` was logged at12144ms. The release counter was committed only after image/boot confirmation.
- Post-OTA gateway logs repeatedly show authenticated recovery. No panic/assert, LED TX failure or HTTP read-timeout warning occurred in this r4 capture. Earlier r3 errors remain documented above.
- Wheel download progress samples showed34272–43292 free bytes and14336-byte largest block. Lowest observed heap minimum across discovery/transfer was9732 bytes; future UI/network growth needs renewed resource measurement. UI-loop statistics are not FPS or measured physical button latency.
- Owner confirmed after OTA: menu works, both buttons work, no unexpected LED flashes. This is a live observation, not an electrical waveform/endurance guarantee.
- Recovery v1 wire format, UUIDs, credentials/bond retention and original backup were preserved. No eFuse changes or vehicle CAN transmissions.

Release4 wheel SHA-256: `164cab08eb9db3ae543a9a04730ff0e241160a3f6a1cf951834579750ece035f`.
Release4 gateway SHA-256: `38c76e7b80a8309b3a2fdfa51ff25d70a13ae775f17c7d4a01dbc61201299126`.

Private raw captures: `.test-build/ota-r4-COM6.log` and `ota-r4-COM7.log` (not committed).
