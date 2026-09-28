# Information cockpit v2

2026-09-28. Replaces the rejected simple Ribbon proposal. This is a researched design proposal and original artwork, not an actual LVGL screen capture or measured firmware performance. The new user request for speed, RPM, transmission, coolant, oil and a door graphic overrides the earlier role guideline limiting the operational surface to three information groups.

## Reference research and analysis

- [Haltech uC-10 official overview](https://www.haltech.com/news-events/uc-10dash/): configurable vehicle channels on a1280×480 dashboard. The central primary-instrument / peripheral support-information hierarchy is relevant, but shrinking every available instrument to320×172 would sacrifice readability. Transfer the hierarchy, not the full density. Official image links returned a cache-fetch failure in the research tool; no official image was downloaded or copied into this repository.
- [Audi RS7 official control-system overview](https://www.audi.com/en/high-performance-at-its-most-beautiful-the-audi-rs-7-sportback-and-the-rs-7-sportback-performance-12082/control-system-12101): black background, reduced information hierarchy, explicit engine-oil information and contrasting shift indication. Our inference: graphical instrument structures should surround readable numbers, while warnings replace optional information temporarily. The12.3-inch1920×720 interface has far more space than this wheel; its full density is unsuitable here.
- [Requested BMW vehicle/door-photo reference](https://www.capitalone.com/cars/vehicle-details/2019/BMW/5%2BSeries/540i%2BxDrive/WBAJE7C5XKWW42177): redirected to general vehicle search on2026-09-28. Its specific door-warning photo could not be inspected. No claim is made that our silhouette matches that image or BMW's factory UI.

The top-view sedan below is original vector geometry created for this task. No logos, manufacturer screenshots, copyrighted graphics or font binaries are embedded.

## Two substantial graphic directions

**A Angular cluster (recommended)** joins an angled tachometer shoulder to a segmented top rail, with a central large speed readout framed by open cyan/magenta instrument brackets. Gear is isolated left; numerical RPM right. Coolant and oil form a bottom thermal row with distinct thermometer/drop outlines. This uses broad instrument geometry rather than the previous label-and-bar composition.

**B Dual arc** gives speed and RPM separate open circular scales, cyan and magenta, and places transmission between them. Temperatures occupy a bottom strip. It is familiar to conventional dashboards but has tighter central spacing and larger redraw areas for curved indicators.

`comparison.png` places all native previews at1:1. Each standalone PNG is320×172. `a-cockpit.png`, `b-cockpit.png`, their unavailable variants, and six door states are supplied. Numeric values are design fixtures only:108km/h,3420rpm,D,94°C coolant,102°C oil. They are not BMW observations and must remain visibly Demo when used in a synthetic firmware fixture.

## Data semantics

Each signal needs independent validity/freshness. `--` means unknown, not zero. Stale speed must never render0. Missing temperatures empty their indicators. Gear should preserve supplied transmission enum (P/R/N/D/S/M if supported); unknown is `--`, never infer from RPM/speed. Demo provenance persists across every graphic and automatic warning.

Door inputs distinguish valid-known closed/open for FL,FR,RL,RR. Coordinates are vehicle-relative: front points upward, vehicle left appears at image left. No steering-side assumption. An authenticated transport alone cannot make invalid door bits trustworthy. Multiple known-open doors are shown simultaneously.

Automatically show the door graphic when at least one current valid door reports open. Preserve current valid speed and transmission at left; hide supplementary engine/temperature readouts only while the automatic view is visible. Do not auto-switch over provisioning, an OTA confirmation or active update. Maintain the open warning until valid closed information arrives. If data becomes unavailable while showing open doors, show `Door data lost / State unknown`; do not synthesize a closing animation. The all-unknown preview illustrates this state. A known-open door plus an unknown door must still show the known-open geometry and explicitly mark the other unknown (e.g. `?`). Root owns the precise shared-state/priority contract; visual code must consume it, not invent CAN bits or freshness thresholds.

No permanent dismissal is proposed. Back/navigation may leave a manual door-details page, but must not silently clear the underlying open state. No new vehicle commands.

## Exact A geometry

All coordinates320×172. Screen#080D18, panel#111D2C, frame#2B4055, foreground#F1F5FF, secondary#9DAFC4, cyan#35E4FF, magenta#F35AC8, amber#FFC857. Warnings use text and door shape as well as color.

- Header E90 at(10,3), Demo at(258,3), font14.
- Angled tach shoulder polygon:(10,47),(36,26),(309,26),(309,44),(42,44),(22,59). Twenty-eight small visual segments beginx40 at9px steps,7px wide,y29..41. This visual segmentation need not equal the23 physical RPM LEDs.
- Optional scale0/2/4/6/8 atx40/104/167/230/292,y46. Use14px if12px unavailable; omit scale labels before reducing core values.
- Speed centeredx160,y60; implementation font40, mockup48; km/h atx160,y110,font14. Label box must fit up to3digits.
- Speed bracket left:(84,120),(84,84),(107,63),#264C63; right:(212,63),(236,84),(236,120),#513250;2px lines.
- Gearx18,y76,font28; labelx17,y107,font14. Left frame:(10,66),(61,66),(79,83),(79,121).
- RPM centeredx273,y76,font20; rpm labely103,font14. Right frame:(241,66),(310,66),(310,121),(260,121).
- Thermal row separator(10,130)..(310,130). Coolant iconx10,y139; labelx27,y137; valuex92,y134,font20. Oil iconx170;labelx187;valuex252. Optional small thermal linesy159, explicitly indicative scale only after domain range is agreed; unavailable must be empty. Avoid claiming a validated warning threshold from this artwork.

## Door geometry

Retain header. `Door open` at(10,26),font18/20. Speed(10,57),font40;unit(48,81),font14. Gear label(10,110),value(62,102),font28. For3digit moving-speed values use a wider left group/unit below, not overlaying the digits as in the illustrative stationary0 screenshot. Dividerx130,y28..144. Car centerx233,topy40,height109,width52. `Front` above the nose; explicitFL/FR/RL/RR labels outside door geometry. Footer describes selected doors and retains Demo.

Original body vertices relative to centerx/top: (-16,0),(16,0),(23,12),(26,80),(20,109),(-20,109),(-26,80),(-23,12). Windshield and rear-glass polygons, roof and tail lamps are in render.py. Front door hinges: side±25,y29. Rear: side±25,y59. An open door extends28px laterally with a contrasting amber polygon; front/rear must change independently. Root/UI may reuse these coordinates directly in bounded LVGL custom drawing.

## LVGL / memory handoff

Use existing fonts14/40 with integrator-approved20/28; desktop previews use locally installed Windows Bahnschrift without distributing the font. This is a typographic approximation. LVGL Montserrat assets remain subject to their pinned upstream license. Static geometry should be one bounded draw callback or a small number of persistent line/arc objects, not individual object-per-tick. Draw directly through LVGL; no screen-size canvas, raster car image, extra DMA framebuffer, shader, shadow or PSRAM dependency. PNG files are documentation only.

Objects remain UI-task owned. Reuse value labels and update only changed strings; invalidate tach/door bounds only when their state changes. One/two-step instantaneous door opening is sufficient; avoid an endless warning animation. No physical timing/heap claim comes from these previews. Existing OTA heap minimum9732bytes makes both additional font/cache cost and object allocations material; measure full HTTPS discovery/OTA with the chosen design before acceptance.

## Evidence and next step

Read repository roles and latest physical OTA/LED reports. Researched linked references above; limitations stated. Generated10native previews plus comparison with Pillow, then inspected the comparison visually. Corrected overlapping right-door labels and dimmed invalid temperature rails after critique. No firmware/config/protocol files changed; no build, device action, CAN traffic, commit or push performed. Design contractv2 only; recoveryv1 unchanged.

Root selects/communicates a direction; UI implementer integrates exact geometry against the shared normalized telemetry extension. Verification must cover each individual open door, multiple doors, mixed known/unknown, data loss while open, zero/current speed versus stale,3digit speed in door view, gear width and OTA priority. Render actual LVGL captures and inspect on the physical display before calling the design validated.

## Implemented variants and native evidence

The final implementation makes B/Dual arc the default Drive screen and A/Angular the alternate Sport screen, preserving the owner's multiple-instrument requirement. `implemented-*.png` are actual production LVGL9.4 view captures compiled on a desktop, not physical panel photos. They include all six closures (four doors, trunk, hood), multiple and unknown states, valid zero and a legacy RPM-only peer. Human-readable individual opening labels and degree-Celsius glyphs are rendered, not simulated with a different desktop font.

There are24 LVGL objects. In a32KiB allocator on the64-bit host, used memory was19,752bytes and peak21,712bytes; this is not an ESP32 heap figure. Three bounded custom-draw regions handle instruments and vehicle geometry; unchanged regions are not invalidated each UI loop. No screenshot asset is compiled into firmware, no framebuffer was added, and the existing partial DMA buffers remain.

Acknowledgement and maintenance behavior follow the authoritative [cockpit contract](../../architecture/information-cockpit-v1.md), including six independently valid closure bits. Physical results are recorded separately in the validation report.
