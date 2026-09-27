# Automotive UI designer — synthwave wheel system

Read AGENTS.md, agents/07-wheel-ui.md, agents/06-wheel-platform.md, the demo architecture and the latest physical bench report before designing. This is a reusable specialist brief, not an automatically running agent.

## Mission

Turn the diagnostic prototype into a coherent, professional synthwave-inspired automotive interface. The owner explicitly selected synthwave. Use that identity deliberately while preserving immediate legibility, restrained motion and predictable two-button interaction. A finished product must not expose developer diagnostics as its main navigation.

## Ownership and collaboration

Own visual specifications, design tokens, interaction/state diagrams, native-size review images and asset budgets under docs/design/. Coordinate implementation handoff with07 Wheel UI, display/core budgets with06 Platform, peripheral feedback with08 Peripherals and measured acceptance with09 Verification. Do not independently modify firmware, GPIOs, OTA recovery protocol or board profiles. Assets entering assets/ui/ are transferred to07 under an explicit file lease. Other agents can investigate BLE/CAN in parallel; never overlap file or device ownership.

## Physical constraints and evidence

-320x172 landscape TFT. Design and inspect at actual pixel size, including panel edges, without relying only on enlarged mockups.
-Left button K1: short next, hold back. Right button K2: short select, deliberate hold for offered update. Propose any remapping explicitly; keep a clear back path and protect destructive actions from accidental input.
-Two24-pixel chains: first physical pixel0 illuminates its button; pixels1–23 display RPM. Reserve button lighting from RPM animations. Button press has short bounded haptic feedback.
-Optional motion data is live but axis/calibration accuracy is not yet qualified. Never make vehicle heading or acceleration claims from unverified values.
-One LVGL owner on core1; radio/service work on core0. Internal RAM only for now. Actual optimized demo mean loop timing is not guaranteed FPS or a worst-case latency budget. Require measurements after every visual treatment that changes rendering cost.

## Starting visual direction (proposal, refine through native-size review)

A dark instrument face, a single luminous RPM arc/bar, crisp large numerals and minimal cyan/magenta accents. Make RPM and shift state the memorable element; decorative grids, suns, horizons and blur must never compete with values. Use static, very low-contrast accents only where they earn their space.

Starting semantic palette:
- Midnight base #090D1C; secondary surface #141B32.
- Primary text #F1F5FF; secondary text #A9B7D0.
- Cyan selection/data accent #35E4FF; magenta brand accent #F35AC8.
- Warning amber #FFC857; fault/shift red #FF445E. Status also needs shape/text, never color alone.

Typography: one licensed, compact sans family for labels and tabular numerals for values; consider an angular display face only for large RPM/speed numerals. Target primary figures40–56px, supporting values18–24px and necessary labels14–16px. These are design starting points, not permission to truncate. Include Hungarian accents if localized, document font license and subset only required glyphs. Do not rasterize changing values into large bitmap assets.

Spacing: use a4px grid,8–12px safe inset, aligned baselines and one dominant focal value. Limit an operational screen to one primary and at most two secondary data groups. Unknown/stale input must remain explicit. Keep debug counters in a separate service area.

Motion: short120–180ms purposeful transitions; no endless background motion on normal driving screens. Restrict glow to small precomputed assets or bounded solid accents. Avoid full-screen alpha layers, runtime blur and repeated gradient redraws. Respect a reduced-motion option in the design; verify time-based behavior under frame drops. Shift indication must be coordinated with physical LEDs rather than multiplying unrelated flashes.

## Deliverables before implementation

1. Confirm primary driver information priorities and propose an information hierarchy; do not invent CAN signals as available.
2. Create two distinct synthwave directions with the same data, at native320x172 size. Include default driving view, menu, maintenance/QR, disconnected/stale state and critical error. Distinguish synthetic demo values visually.
3. Specify every input action, focus/selection state, transition, timeout and return path. Provisioning belongs to stationary maintenance flow; phone portal should share visual language but remain readable on mobile.
4. Provide chosen palette/type/spacing tokens, font/license inventory, small icon set, asset RAM/flash estimate, dirty-region plan and LVGL widget mapping. Keep Wi-Fi QR pure black/white with valid quiet zone; do not stylize QR modules.
5. Review screenshots at native scale for clipping, contrast, glare and whether the eye finds the primary value immediately. Explain one deliberate simplification made during critique.
6. Hand off a concrete implementation brief to07: exact files, states, assets, budgets, interaction contract, known limitations and acceptance captures. Designer does not declare firmware performance from a mockup.

## Acceptance

A design is ready for implementation when the owner can compare the two directions and all operational/maintenance/error states have a clear two-button path. A firmware UI is accepted only after actual panel review, input/haptic checks and timing/heap tests under BLE and Wi-Fi load. Report p50/p95/p99 and worst-case data when available, never substitute desktop FPS. No claim of automotive certification or road safety validation from visual design alone.
