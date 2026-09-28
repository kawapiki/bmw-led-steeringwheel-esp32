# Runtime E90 renderer v1

The operational wheel renders actual E90 geometry with TGX1.1.4 (commit d43f88906ab87f877ea249af2232896b097f15a7). LVGL still owns menus, instruments, captions and textual availability states; separate lamp icons are intentionally absent. No vehicle signals originate on the wheel; the gateway simulator or future validated CAN decoder provides the same normalized state. Application/recovery BLE contracts are unchanged.

## Ownership and memory

The existing UI task on core1 is the sole owner of both TGX rendering and LVGL. Network/core0 callbacks only publish state. The renderer contains no network, flash write or per-frame heap allocation. A 320x172 RGB565 image and uint16 depth buffer consume 220,160 bytes in explicitly requested PSRAM. LVGL copies image pixels through the existing internal DMA draw buffers; those buffers are never repurposed as render storage. Static descriptor/pixel lifetime lasts until shutdown. The image is invalidated only when the renderer generation changes.

Board evidence identifies8MB embedded octal AP_3v3 RAM. Configure auto-detection,40MHz, boot memory test and CAPS-only external allocation; keep ordinary malloc/stacks/DMA, XIP/rodata and Wi-Fi allocation policy unchanged. Missing PSRAM results in a checked unavailable/vector path. The boot health gate explicitly checks internal8bit memory so external RAM cannot hide its20KB minimum.

## Scene behavior

The 5-second boot scene starts black and frontal, fades in only the angel eyes, reveals the body while zooming closer, then yaws and slides the car off-screen with wheel rotation. It is decorative and immediately interruptible by input, maintenance or a fresh opening. Runtime door/panel transforms use retained normalized closure state; unknown is never forced closed. Camera yaw/elevation follows one-sided openings or a higher viewpoint for both sides. Closed-state lamps can be inspected on the Vehicle page. Successful 3D scenes occupy the full display without text, icons or gauges. Incomplete validity dims the car; unknown signals never produce illuminated mesh parts. Text remains available on diagnostic, service and unavailable/paused fallback pages.

Actual E90 mesh:3100triangles,124,000bytes plus48bytes wheel pivots, generated from the attributed prepared Blender model. Separate groups cover body, four doors, trunk, hood, angel eyes, high beam and four wheels. Sampled per-face material/texture colors replace expensive runtime textures. Hinge angles and the division of lens surfaces into lighting functions are illustrative, not measured CAD geometry. Actual front/rear lens meshes receive emissive unlit shading. With no openings, camera focus follows active indicators/brakes and briefly retains the view across off phases; only incoming bits control emitted light.

Rendering is bounded to one frame per50ms at most, skips missed frames, and stops redraw when the scene settles until another input changes. This20Hz cap is a target, not measured hardware FPS. No catch-up queue. Service/OTA pauses3D; the immutable recovery protocol remains independent.

## Validation gates

Require actual-mesh host captures, incremental/full LVGL pixel comparison, latest-state throttle tests, maintenance/navigation interruption and unknown-state tests. On hardware verify PSRAM discovery/memtest, boot self-test, render timing, internal heap and UI stack low-water marks under BLE/LED activity. Qualify Wi-Fi discovery separately; host timing is not physical display latency. Refer to the dated validation report for results, not this design target.

## Surface lighting

Light pools are bounded, layered world-space polygons on a virtual ground plane. Only emissive lamp faces receive a dim mirrored reflection, avoiding a second full-car render. These are stylized reflection cues, not ray-traced illumination. Both live gateway lighting and the decorative boot angel eyes use these effects. They use the same depth-tested renderer and do not allocate per-frame buffers.

## Black metallic paint

Per-face material IDs distinguish black body paint, dark neutral glass and original trim/lamp/badge materials. The ID occupies existing triangle-struct padding (40 bytes per triangle); geometry now contains 3100 triangles after the wheel-detail refinement. Paint uses a bounded camera-relative studio highlight computed from transformed face normals, with an additive neutral clearcoat cue independent of the black base. This avoids TGX's multiplicative material color suppressing highlights on black paint. Glass retains subdued diffuse lighting; chrome and roundels retain their colors. Boot reveal and incomplete-state dimming also apply to paint. The low-poly mesh remains visibly faceted; this is stylized metallic lighting, not physically based rendering. No new textures, buffers or frame allocations are introduced.
