# Real E90 closure view and boot scene

The supplied source is now imported and rendered using Blender5.2.1 LTS. Original model/texture and CC BY4.0 attribution are in [assets/ui/e90](../../../assets/ui/e90/ATTRIBUTION.md). Original ZIP SHA256: fa702396d7fa54054831ff67823b010e90521b221e2625b4222888b36edd6eb9.

## Implemented asset preparation

The model has separate mirrored door/window meshes. The generator applies modifiers, splits left/right panels, sets front hinges, and groups the hood/trunk with their badges. The in-scene author text is removed from derived renders; attribution is retained in the repository. Opaque dark glass and an interior tub hide empty areas, and materials/light are adjusted for the small display. The original source remains unchanged.

Closure views select left/right front-quarter for one-sided openings, rear-quarter for trunk-only, and front overview for mixed sides. This supersedes the earlier top-down proposal after the owner requested hiding the empty cabin. The scene includes all six open-panel combinations (64masks). Open angles are illustrative, not measured door angles. Unknown input must retain explicit last-known/unknown text, not assert that unseen panels are closed.

Boot sequence: real model moves into view with wheel rotation, camera makes a360-degree orbit, and the front lamps flash at the end. It is a one-time, interruptible presentation. Real fresh closure alerts, user input and maintenance/OTA take priority. Synthetic demo alerts are deferred until the short intro ends so it can be observed on the bench.

## Runtime and asset limits

The firmware displays real-model pre-rendered images, not a runtime3D engine. 28boot frames and64closure frames, each192x104RGB565, occupy exactly3,674,112bytes. No decompression framebuffer or per-frame asset allocation is needed. Source frames are flash-backed; existing partial LCD DMA buffers remain. Nominal boot frame interval50ms (20fps target) is not a measured panel-FPS guarantee. Closure states currently change to their selected camera view directly; smooth live camera interpolation between arbitrary masks is not implemented in this increment.

Build tools: tools/assets/render_e90.py (Blender) and tools/assets/pack_e90.py (Python/Pillow). Exact asset checksum is in asset-manifest.json. Source model and textured license provenance are separate from generated firmware data.

## Vehicle lights

Angel-eye/high-beam geometry is present for the boot demonstration. Live headlights, angel eyes, left/right indicators and brake lights are NOT yet available in the gateway telemetry. Their active state must remain unknown until validated decode, timestamps and per-field validity are supplied. Do not infer brakes from RPM/speed or animate fictitious turn signals as live vehicle data. A future additive lighting characteristic/asset-overlay contract can preserve the frozen recovery protocol and older peers. Actual light-state behavior is pending; the boot flash does not establish vehicle control or CAN reception.

## Verification

Runtime policy tests, real LVGL image rendering, flash fit, heap behavior and physical boot/door observation are required. [Runtime integration notes](runtime-plan.md). Measured bench results are recorded separately; desktop renders alone do not prove performance.

## Runtime 3D direction

The owner subsequently asked whether a real3D renderer would be preferable. It is the recommended direction for continuous camera motion and independently articulated lights/panels; this frame atlas is a tested fallback/prototype, not a claim that runtime3D has been implemented. The modified Blender model can be simplified and exported to a bounded mesh. Evaluate TGX with a small RGB565 viewport and depth buffer, verify the board's PSRAM configuration, and measure input/BLE/LED behavior before replacing the renderer. Do not infer FPS from desktop tests or allocate a full scene buffer in the remaining TLS heap.

The operational wheel versus gateway-simulator boundary is renderer-independent and is fixed by docs/architecture/operational-wheel-lighting-v1.md. Vehicle lighting received over BLE must drive the wheel presentation directly; decorative boot lamps must never be confused with actual received lamp state.

## Runtime3D implementation

The wheel now links TGX1.1.4 and the2498-triangle `e90_mesh.h` exported by `tools/assets/export_e90_mesh.py`; the frame atlas is not linked. See [runtime3D architecture](../../architecture/runtime3d-v1.md). Source/model attribution remains unchanged. Dynamic camera, panel transforms, wheel rotation and normalized lamp states replace frame selection.

### Hood and trunk roundels

The runtime exporter replaces the two texture-averaged badge meshes with blue/white quartered geometric roundels, black rings and chrome outlines. Each uses 80 triangles, explicitly reserved within the unchanged 2,498-triangle total. Groups 6 (hood) and 5 (trunk) preserve the existing panel transforms. Native-scale captures are in `runtime-renders/badge-*.png`; lettering is not legible at this resolution. The original Blender source remains unchanged.
