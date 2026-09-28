# Information cockpit v2 — 2026-09-28

## Delivered scope

Research and design replaced the simple RPM-only visual treatment with a dual-arc Drive instrument cluster and angular Sport alternative. Both show speed, RPM, selector/actual gear, coolant and oil temperature simultaneously. Original vector vehicle graphics identify four doors, trunk and hood automatically, preserving the underlying page and allowing acknowledgement. Missing/stale information is explicit; maintenance/OTA screens retain priority.

References and native design/implementation captures: [design study](../design/information-cockpit-v2/README.md). The study cites official Haltech and Audi material; Porsche's official quick guide also informed separate coolant/oil presentation. No vendor artwork was embedded. Shared contract: [information cockpit extension](../architecture/information-cockpit-v1.md).

All new values currently come from the gateway's explicitly marked synthetic Demo scenario. This is not a BMW CAN decoder or evidence of actual door/temperature reception from the car. CAN transmission remains disabled.

## Interface and verification

A new optional22-byte application characteristic fits a default-MTU23 ATT read. Existing16-byte RPM telemetry remains available, and OTA recovery v1 is unchanged. Each numeric signal and each of six closures has an independent validity bit. The source is Demo only. Selector and actual forward gear are distinct packed fields, displayed as D or D5 as appropriate.

Both ESP-IDF6.1 builds passed, as did all affected native harnesses:27 core cases,7 release authenticity/target cases, telemetry codec and fake-ATT tests, UI navigation/field validity/gear/alert/acknowledgement/stale/maintenance tests, LED transport and HTTP/release/OTA-retry regressions. Independent review found two issues (maintenance overlay suppression and packet freshness versus RPM validity), both fixed before deployment; final review reported no blocker.

Actual LVGL9.4 desktop renders include Drive/Sport at108km/h/D5, every closure, multiple closures, unknown last-known state, valid zero and RPM-only fallback. QR decoding passes. With24 objects in a32KiB64-bit host pool, used memory was19,752bytes and peak21,712bytes. These are host figures, not MCU heap. No framebuffer or PSRAM requirement was added.

## Exact bench applications

- Final optimized wheel 1,789,696 bytes, SHA256 068cf5f5c132031ca332b66e202479c0e36ec3c694918e6fb907bc50092e750b.
- Gateway1,266,736bytes, SHA256 d7b4c0793732027bcd4adf065826a7ff2e97c12f50248522e1b0cd2d072f5d99.

Writes were restricted to the known active application slots: wheel0x620000, gateway0x20000. Esptool verified written data. No NVS, otadata, bootloader, partition table or eFuse write was performed. Original stock backup hash remains c8f796755d535f5eb5bdb820e3499595982c86fcfb986f1eb7ec6508f27d3e45. These remain USB development builds with the last signed-OTA release4 metadata; no new signed release was published.

## Physical mixed-version evidence

1. New cockpit gateway with preceding Ribbon wheel: authenticated v1 telemetry remained active, sequences41,91,141,192 across approximately5-second intervals; stationary demo RPM800 then driving RPM6407/1566. Wheel boot self-test passed.
2. New cockpit wheel with original r4 gateway: authenticated sequences5,10,15 (r4's1Hz producer); validity remained001, so only RPM was available. Optional raw fields were0 but their validity was false; the view maps them to --, not measured zero. New wheel boot self-test passed. Minimum free heap80,220bytes in this window. Initial full-screen handler peak113,432us; later measured windows around43–45ms maximum. These are task-handler timings, not FPS/input-to-photon latency.

The mixed-version and initial complete runs used wheel SHA256 f66431d0ff631113e85f941ed9866762252ec1bd767413621cde30bb2b64b27a (1,788,832 bytes), before the visual-only optimization. The final cockpit gateway was restored after the reverse-compatibility test. Private logs and binaries stay in ignored .test-build; no credentials or raw capture logs are committed.

## First complete two-device cockpit run

Both new applications passed their boot self-tests. All fields were received with validity7ff and Demo source, including speed_dkph, packed gear, coolant/oil and closures. The first95-second capture recorded each mask01,02,04,08,10,20 as visible with known3f, followed by mask00/visible0. The owner confirmed the automatic car diagram and all requested values on the physical screen.

Normal minimum free heap was80,196bytes. The initial Drive renderer had approximately28–32ms mean loop periods during moving values and around100–105ms handler maxima; transition/startup maxima reached155,475us. This prompted a scoped dirty-region optimization instead of treating the initial rendering cost as the final result. Subsequent results follow below.


## Final renderer verification

The optimized renderer invalidates the changed quantized arc segment with antialias padding and complete intersected tick bounds; large discontinuities retain a conservative full-dial redraw. No persistent firmware buffer or LVGL object was added. Actual LVGL RGB565 incremental/full-redraw comparisons passed all 1,258 deterministic cases, including 1,024 random adjacent steps, rising/falling sweeps, wrap, clamp, unavailable/recovery and layout/overlay transitions. Two sampled updates transferred 5,588 and 15,370 pixels instead of 34,336. These are host draw-area counts, not measured device FPS. Final host pool: used19,728bytes, peak21,736bytes, largest free7,240bytes,24 objects. Independent read-only review found no blocker; root reran the renderer harness successfully.

The optimized wheel IDF6.1 build and both-image slot/chip/security audit passed. Esptool verified the application write; both devices again passed their boot self-tests. Recovery, BLE schema and gateway binary are unchanged by this optimization.

## Final physical timing window

A 75-second USB-only capture with the final hashes above recorded authenticated full-validity Demo telemetry and both boot self-tests. Moving Drive windows (approximately20–61s after boot) had mean UI loop periods7,957–9,110us and mean handler times2,669–3,755us, compared with the initial renderer's approximately28–32ms/22–26ms. Moving-window maximum handlers were59,221–77,478us. Full-page/overlay transitions remain costlier: observed128,563us leaving the first closure sequence and137,928us entering the next. These window statistics do not establish FPS or button-to-photon latency and are not an endurance qualification. Minimum free heap remained80,196bytes; no panic/assert was observed in this capture.
