# Runtime E90 renderer v1

The operational wheel renders actual E90 geometry with TGX1.1.4 (commit d43f88906ab87f877ea249af2232896b097f15a7). LVGL still owns menus, instruments, captions and textual availability states; separate lamp icons are intentionally absent. No vehicle signals originate on the wheel; the gateway simulator or future validated CAN decoder provides the same normalized state. Application/recovery BLE contracts are unchanged.

## Ownership and memory

The existing UI task on core1 is the sole owner of both TGX rendering and LVGL. Network/core0 callbacks only publish state. The renderer contains no network, flash write or per-frame heap allocation. A192x104 RGB565 image and uint16 depth buffer consume79,872bytes in explicitly requested PSRAM. LVGL copies image pixels through the existing internal DMA draw buffers; those buffers are never repurposed as render storage. Static descriptor/pixel lifetime lasts until shutdown. The image is invalidated only when the renderer generation changes.

Board evidence identifies8MB embedded octal AP_3v3 RAM. Configure auto-detection,40MHz, boot memory test and CAPS-only external allocation; keep ordinary malloc/stacks/DMA, XIP/rodata and Wi-Fi allocation policy unchanged. Missing PSRAM results in a checked unavailable/vector path. The boot health gate explicitly checks internal8bit memory so external RAM cannot hide its20KB minimum.

## Scene behavior

The5second boot scene rolls the car in with wheel rotation, orbits it, and flashes the front lamps. It is decorative and immediately interruptible by input, maintenance or a fresh opening. Runtime door/panel transforms use retained normalized closure state; unknown is never forced closed. Camera yaw/elevation follows one-sided openings or a higher viewpoint for both sides. Closed-state lamps can be inspected on the Vehicle page. An explicit Lights unknown caption represents incomplete lamp validity; unknown signals never produce illuminated mesh parts.

Actual E90 mesh:2498triangles,99,920bytes plus48bytes wheel pivots, generated from the attributed prepared Blender model. Separate groups cover body, four doors, trunk, hood, angel eyes, high beam and four wheels. Sampled per-face material/texture colors replace expensive runtime textures. Hinge angles and the division of lens surfaces into lighting functions are illustrative, not measured CAD geometry. Actual front/rear lens meshes receive emissive unlit shading. With no openings, camera focus follows active indicators/brakes and briefly retains the view across off phases; only incoming bits control emitted light.

Rendering is bounded to one frame per50ms at most, skips missed frames, and stops redraw when the scene settles until another input changes. This20Hz cap is a target, not measured hardware FPS. No catch-up queue. Service/OTA pauses3D; the immutable recovery protocol remains independent.

## Validation gates

Require actual-mesh host captures, incremental/full LVGL pixel comparison, latest-state throttle tests, maintenance/navigation interruption and unknown-state tests. On hardware verify PSRAM discovery/memtest, boot self-test, render timing, internal heap and UI stack low-water marks under BLE/LED activity. Qualify Wi-Fi discovery separately; host timing is not physical display latency. Refer to the dated validation report for results, not this design target.
