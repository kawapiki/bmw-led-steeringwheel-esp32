# E90 model presentation — bounded runtime

Implemented in the UI component; host tests below are evidence for rendering and state logic only. Device timing, linked firmware size and OTA memory qualification remain integrator tasks. Cockpit v1 and permanent OTA recovery v1 are unchanged.

## Asset contract and ownership

The integrator prepares and renders the downloaded E90 geometry offline. Source, attribution and editable scene are under `assets/ui/e90`; render/packing tools are under `tools/assets`. Firmware uses opaque RGB565 little-endian images with the existing display transport byte swap. Background is #090D1C. The covered dark cabin and each camera require visual review; the firmware does not run a 3D engine.

`ui_vehicle_assets.c` exposes `const lv_image_dsc_t *ui_vehicle_asset(bool boot, unsigned frame)`. It returns NULL for invalid indices. There are 28 intro frames and 64 closure-mask frames, each 192x104, stride384 and39,936bytes. The complete `ui_vehicle_frames.bin` is3,674,112bytes, embedded by the component CMake `EMBED_FILES` directive. Const descriptors address flash directly through the linker symbol. The header validates magic, format, flags, dimensions, stride, size and non-null data before selection.

No PNG decoder, canvas, new framebuffer, image scaling or runtime mesh is introduced. One reusable LVGL image object holds the selected immutable descriptor. Native uncompressed LVGL image decoding references the source pixels directly. Existing partial DMA buffers, stack, task ownership, core and SPI settings remain unchanged.

## Files and hooks

- `ui_vehicle_scene.c/.h`: pure elapsed-time intro state, no task, I/O, delays or frame queue.
- `ui_vehicle_assets.h`: bounded descriptor interface/validation. Integrator owns generated `.c` and `.bin`.
- `wheel_ui.c`: advances the scene in the existing owner loop after state acquisition, consumes an intro-skip key, preserves heartbeat, validation and LVGL scheduling.
- `ui_view.c/.h`: image selection and placement; intro at(64,34), closure at(128,48). Service, QR and confirmation widgets retain priority. Missing closure imagery retains vector fallback.
- `ui_graphics.c/.h`: suppresses the existing vector vehicle only while a valid model image covers it. Gauge drawing and partial invalidation remain unchanged.
- `tests/host/test_ui_vehicle_scene.py`: pure lifecycle test.
- `tests/host/render_ui.py` and `ui_render.c`: compile actual pinned LVGL plus production rendering with the real binary embedded through a test-only assembly wrapper.

## Playback and priority

The approved target is20fps: one of28frames every50ms, total1.4seconds. Frames0–5 roll in with wheel spin;6–23 orbit;24–27 show the final front lighting effect. This is a target cadence, not measured display FPS. The headlamp flash is decorative intro content, never live lamp telemetry. Live lighting uses the independent lighting-v1 characteristic and header icons.

Selection is elapsed/50, with missed frames skipped and at most one source change per UI iteration. Only the image region invalidates between frames. No catch-up burst or blocking playback loop is used. A192x104 frame has a7.99ms pixel-wire lower bound at40MHz SPI; actual rendering, transfer and scheduling must be measured.

Maintenance, writing, update offer, Gateway/Service navigation and any fresh closure alerts terminate the intro permanently for that boot. Gateway-simulated and vehicle-originated inputs have identical operational behavior. The gateway scenario may start closed to permit an uninterrupted intro; the wheel never delays a fresh opening according to provenance. The first key during an active intro skips it and is consumed; it cannot activate a menu or confirm an update. BLE reconnection or returning to instruments never replays the intro. Missing/invalid boot imagery also terminates it. Monotonic clock reversal fails closed to the normal UI.

The UI continues processing telemetry, keys, haptics and heartbeat throughout. The existing10second boot self-test is neither delayed nor redefined.

## Closure semantics

The existing six-bit closure state remains authoritative: FL, FR, RL, RR, trunk, hood. The64-frame atlas uses the retained mask directly. Each frame has its baked camera; switching masks is an immediate camera cut, not continuous interpolation. The integrator selects low side/front/rear or multi-opening overview compositions from actual model geometry.

Unknown input does not imply closed. A retained last-known-open mask uses its existing image under the explicit `Last known / state unknown` caption. No invented all-open or all-closed overview is substituted. The opening caption remains visible. Source provenance appears only in Gateway diagnostics; ordinary Drive/Sport and closure views do not display Demo or Live badges. Acknowledgement, new-open-bit rearming, stale behavior and the underlying instrument page remain governed by `ui_door_state_t`. Service, provisioning QR and OTA confirmation cannot be covered by a closure alert. There is no new vehicle-control field. Lamp status is handled independently according to operational-wheel-lighting-v1.md.

## Size and memory gates

Previous app1,789,696bytes plus raw payload3,674,112 gives a projected5,463,808bytes before added code/descriptors, leaving827,648bytes in the6,291,456-byte slot. This is arithmetic, not a linked-image measurement. Acceptance remains linked app<=5,767,168bytes (512KiB slot reserve), no partition change. OTA payload growth and download/retry behavior require integrator validation.

Actual64-bit host renderer with32KiB LVGL pool:26objects,20,208bytes used,22,328-byte peak,6,744-byte largest free block after adding lighting icons. Earlier cockpit was24objects,19,728used/21,736peak. The measured host increase is480bytes steady/592peak; these are not MCU memory values. No39,936-byte decoded frame fits the pool. Existing device HTTPS low-water mark was7,424bytes, so physical heap/stack and release discovery checks remain required.

## Verification

Native lifecycle assertions pass for first/middle/last frame, late skip, completion, interruption, permanent cancellation and clock reversal. Actual LVGL tests validate every pixel of all28intro and64closure images against embedded RGB565 source bytes, plus invalid descriptor/index fallback and Service priority. Incremental-versus-full rendering passes1,481cases, including prior gauge/validity/layout coverage. Model views, unknown captions, intro phases and QR takeover have native320x172 captures under `.test-build/ui-render`.

Host success is not device smoothness, boot validation or OTA proof. Next integrator checks: actual linked size, input-to-skip latency, normal and transition handler timings, BLE/heartbeat continuity, final flash visibility, saved-Wi-Fi discovery heap and eventual enlarged-image OTA. No hardware operation, firmware build, flash, commit or push was performed by this UI worker.
