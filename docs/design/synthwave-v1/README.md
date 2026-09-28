# Synthwave wheel UI v1 — design handoff

Status: two proposed directions, neither owner-approved at creation. Root may select a reversible implementation baseline while obtaining preference. These are desktop design previews, not actual LVGL panel captures or performance measurements.

## Scope and honest data

320×172 wheel, ESP-IDF6.1/LVGL9.4, recovery protocol v1 unchanged. Only gateway-supplied synthetic RPM is available. Every operational RPM view carries `Demo`; transport authentication does not turn synthetic data into real vehicle data. No invented speed, gear, temperature, battery voltage or seat state. When the snapshot is stale/invalid, show `--`, empty the band, show `No data`; distinguish disconnected `Reconnecting` from connected-but-stale `Stale`. A genuine current zero is `0`.

Reviewed AGENTS.md, roles06/07/10, paired-r4 bench and LED transport investigation. r4 demonstrated OTA completion; lowest observed heap minimum was9732bytes. This is existing peak pressure, not permission to consume that amount. First LED on each chain is button lighting; graph represents remaining23, and does not drive peripherals itself.

## Two directions

**A / Ribbon — recommended.** A straight segmented RPM rail echoes the physical LED strips. Large left-aligned numeral and quiet gateway provenance. Rectangular changing regions are economical on the SPI panel. Strongest hierarchy at native size and least rendering risk.

**B / Apex.** Angular open instrument frame around a centered numeral, cyan rising flank and restrained magenta end. More distinctive cockpit silhouette, but a moving angled indicator makes a larger dirty rectangle and needs custom drawing. Keep this as alternate treatment, not an extra operational page.

Preview files `a-*.png` and `b-*.png` are native320×172; `comparison.png` shows them at1:1 in two columns. Menu mockup shows hierarchy style, not a commitment to flatten Service tools into the main navigation. Root/UI implementer own exact agreed navigation.

Design critique: removed horizon grids, decorative glow and duplicate secondary measurements; they compete with the only trustworthy value. Avoid an animated sun or moving background. The inactive B end accent is decorative only; implementation should dim the entire rail for stale data so no bright accent can imply a live shift warning.

## Palette and typography

| Token | Value | Purpose |
|---|---|---|
| background | #090D1C | Opaque screen |
| surface | #141B32 | Selected row/status capsule |
| text | #F1F5FF | Main reading |
| secondary | #A9B7D0 | Units/instructions |
| cyan | #35E4FF | Selection, valid demo data |
| magenta | #F35AC8 | Restrained accent, not error |
| warning | #FFC857 | Stale/maintenance caution |
| error | #FF445E | Explicit error state |
| inactive | #222B42 | Empty rail |
| divider | #26304B | Layout separator |

Mockups use Windows Bahnschrift installed locally; no font file is copied or redistributed. It is a layout approximation, not the firmware font license selection. Implementation uses existing LVGL Montserrat14 for labels, integrator-approved built-in Montserrat40 or48 for RPM. Built-in Montserrat is distributed with LVGL under its accompanying font license; preserve upstream licensing and confirm the pinned dependency license inventory when packaging. Font changes occupy read-only flash plus glyph cache/working RAM, which must be measured. Avoid dynamic rasterized value bitmaps. Labels in this increment are English ASCII; Hungarian localization is separate glyph/width work.

## A geometry / widget mapping

All coordinates are pixel positions relative to320×172 root. Root opaque background;8–12px horizontal safe margins except the existing QR quiet-zone frame.

| Element | Geometry | LVGL mapping |
|---|---|---|
| Header | (12,8), maximum230×20 | Reused label,14–16px |
| Demo/status | (260,8),48×18 | Label, opaque surface, small radius |
| Separator | (12,33),296×1 | Root custom draw or reused line |
| RPM rail | (12,44),296×14 | One bar/custom draw,23 logical divisions, no23 child objects |
| Optional scale | y61; x12 /146 /299 | `0`, `4`, `8`, not required for first implementation |
| RPM | (12,80),160×56 | Reused label;40/48px, `--` on invalid |
| Unit | (175,107),40×20 | Reused14px label |
| Provenance | (230,88),78×44 | Two14px lines: Gateway / Demo or No data |
| Footer | Divider(12,146),296×1; hints y153 | One/reused labels,14px if12 unavailable |
| Menu | Rows(12,44/75/106),296×27 | Reused labels; selected surface +3px cyan left stripe |
| QR | (0,4),164×164 total; existing148px QR inside8px extra white border | Preserve current QR object and actual credential handling |
| QR text | x173, width147; y10/44/68/92/149 | Header/setup steps/address/back hint |
| OTA progress | (12,110),296×10 | Reuse bar with distinct progress semantic |
| Error body | x26,y46/78/101, width282 | Concise title, retained-firmware status, retry action |

Do not display generic `Demo` badge over OTA progress in firmware: use maintenance state text. Long raw backend errors belong in details, not small clipped title strings. OTA percent follows the active device; `Gateway verified` is allowed only after the actual authenticated result, never inferred from elapsed time. Mock screenshot32% is illustrative.

## Navigation / interaction contract proposal

Main cycle: Engine → Gateway → Service. K1 short moves to next; K2 on Service enters diagnostic/service list. K1 short cycles rows, K2 opens selected detail. K1 hold returns one hierarchy level, eventually Engine; one press must not trigger both select and long-hold action. Gateway page shows link state, age/provenance and compatibility, not raw driver logs.

Service list retains LED, buttons/haptics, optional motion, performance, Update, About and explicit maintenance settings. Operational view must not force users through every development tool. K2 on Update performs the existing authenticated release check. Once an offer exists, deliberate K2 hold keeps the existing confirmation semantics; no new automatic update confirmation. QR provisioning remains within existing AP lifetime; hold K1 exits the visual page without inventing a new shutdown rule. Credentials clearing and standalone update keep existing explicit protections. Do not change recovery protocol, freshness thresholds or intent handling through visual code.

Input feedback: existing short bounded motor pulse and blue button pixels remain. Transition target120–140ms small horizontal position change, no full-screen crossfade; disable/cancel animation in maintenance if it raises load. No idle motion. RPM may render latest snapshot at the current UI cadence; do not extrapolate stale data or interpolate across disconnect. For reduced motion baseline, no animated background and no mandatory transition dependency.

## Resource / dirty-region contract

- No new screen-sized buffer, canvas, image asset, runtime blur, gradient or shadow. PNGs are design documents and must not enter firmware.
- Retain existing two15360-byte draw buffers and QR allocation; exact LVGL owner stays core1. Do not change SPI clock, task priorities or GPIOs.
- Target14–18 reusable objects including existing QR/frame; collapse scale labels or provenance before increasing memory. This is a proposed ceiling, not a measured memory budget.
- Numeral changes invalidate its bounded label box; rail changes its14px strip; status changes only its label. Never reset root style/title/opacity unchanged each tick. Reuse objects across Service detail pages.
- Static assets RAM/flash addition:0bytes (no PNG/TTF embedding). Font flash delta and LVGL heap delta are not yet measured. New objects/styles must be measured in the real build.
- Compare boot free heap, largest block, minimum through GitHub discovery and paired OTA to r4. Collect UI loop distributions, flush timing and input observations under BLE/Wi-Fi. No FPS claim from loop durations.

## Acceptance / handoff

Designer owned only `docs/design/synthwave-v1/`. Contract revision: design-only v1, no wire/shared header changes. Generated12 native previews and comparison using Pillow/local font and the already-built production LVGL QR encoder DLL, with a deliberately fictional Wi-Fi name/password. Visually inspected comparison at native scale: no clipping; primary values and all actions fit. QR desktop raster uses established164px dimensions; this new preview is not phone/panel qualification.

Next: root obtains direction preference; role07 implements selected geometry against normalized freshness-aware gateway snapshot, role06 verifies UI ownership/resource deltas, role09 captures actual panel and exercises disconnect/reconnect/current zero/stale/rapid-button/OTA transitions. No device access, flash, CAN action, build, commit or push performed by this designer.

Generation: `render_previews.py` requires Pillow, the local Windows Bahnschrift font and previously built `.test-build/qr-codegen.dll`. Native previews are intentionally checked in for review; regeneration is optional and Windows-specific. The dependency DLL is not copied into the design directory.

## Implemented LVGL views

The owner selected multiple switchable instruments. The implementation uses Engine -> Shift -> Gateway -> Service, with a nested nine-tool service menu. `implemented-*.png` are captures from the actual LVGL9.4 production view compiled on the desktop, not photographs of the panel. Shift emphasizes the same gateway RPM with a larger rail and a demo shift cue. Both views clear unavailable values immediately. The 140 ms menu motion is bounded; the RPM rail uses the latest sample without interpolation. The magenta rail starts at the demonstration threshold of 6500 RPM, not a calibrated recommendation for this engine.

Native navigation/data/confirmation tests pass. The actual QR capture decodes to a fictional test credential. The host LVGL allocator peaked at 19,376 bytes in a 64-bit host process; this must not be represented as ESP32 heap usage. No mockup PNG is compiled into the firmware.
