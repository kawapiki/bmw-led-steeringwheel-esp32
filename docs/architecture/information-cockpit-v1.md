# Information cockpit extension v1

Additive application telemetry for speed, RPM, transmission selector/gear, coolant and oil temperature, and individually valid closures. User requested a full information surface and automatic location-specific open-door indication. The UI has dual-arc Drive and angular Sport variants, plus Gateway and Service. This replaces the RPM-only visual scope; earlier one-primary/two-secondary role guidance does not limit this explicit user requirement.

## Compatibility

Existing application service remains unchanged. Existing characteristic selector4 continues the exact16-byte demo RPM v1 record for old wheels. New optional characteristic selector5 carries22 bytes, fitting one ATT Read Response at the default23-byte MTU (one-byte response opcode). New wheels prefer it only after discovery; old gateways fall back to RPM-only with every other field unavailable. Recovery UUIDs, framing, credential sharing, signed manifests and OTA behavior do not change.

## Byte layout (little endian)

| Offset | Bytes | Meaning |
|---|---:|---|
|0|1|Extension version1|
|1|1|Source1 = gateway demo; other sources rejected in this increment|
|2|2|Validity bitmap, bits0–10 below; remaining bits zero|
|4|4|Monotonic sequence, wrapping uint32|
|8|4|Gateway uptime milliseconds, wrapping uint32|
|12|2|RPM, valid range0–10000|
|14|2|Speed in0.1km/h, valid range0–3500|
|16|1|Selector high nibble; actual forward gear low nibble|
|17|1|Coolant Celsius +40, valid decoded range-40–150|
|18|1|Oil Celsius +40, valid decoded range-40–180|
|19|1|Open mask: front-left, front-right, rear-left, rear-right, trunk, hood; upper bits zero|
|20|2|Reserved zero|

Validity bits:0RPM,1speed,2gear,3coolant,4oil,5front-left,6front-right,7rear-left,8rear-right,9trunk,10hood. Values without their validity bit are unavailable, never implicitly zero or closed. Selector0unknown,1P,2R,3N,4D,5S,6M; low nibble0means forward gear unavailable,1–8 actual forward gear. P/R/N require low nibble0. Display D5 when both known, D when only selector known.

Authenticated reception, advancing sequence/uptime, bounded500ms read latency, one in-flight read,2500ms receiver freshness and recovery priority remain. This demo packet does not establish the age of future individual CAN samples. A real vehicle producer must clear each field's validity when its underlying input expires; do not reuse this demo as a validated BMW decoder.

## Deterministic bench source

The gateway supplies all synthetic fields with Demo provenance. A60-second scenario exercises six closures while speed is0 and selectorP, then closed-body driving data. It must never enable CAN transmission. Legacy RPM and extended RPM come from the same producer snapshot.

## Automatic closure presentation

On Drive/Sport, a fresh known opening automatically shows an original top-view car graphic and identifies all open closures. Speed and selector remain available. The underlying page is retained. K1/K2 can acknowledge the overlay; this consumes that press and leaves a persistent closure indication. A newly opened closure can rearm; closing-only changes do not repeatedly interrupt. Fresh known closure of all six resets the alert. Missing/stale inputs are shown as unknown or last-known, not silently closed. Maintenance, QR, update confirmation and Service are never replaced by an automatic overlay; it is reevaluated when returning to an instrument page.

## Ownership and verification

Root owns this contract, pure cockpit.h constants, demo shared state, font configuration and app composition. BLE role owns codec, discovery and transport tests. UI role owns pure per-field/alert model, LVGL views and render tests. Designer owns docs/design/information-cockpit-v2. No agent has device-write permission; root alone holds the previously authorized bench lease.

Keep the existing partial DMA buffers and one core1 LVGL owner. Built-in fonts14/20/28/40; no new framebuffer or PSRAM assumption. Required verification: mixed-version fallback,22-byte boundary, malformed/unknown fields, each closure/multiple/unknown/ack/rearm, OTA confirmation preservation, actual LVGL captures, both target builds, physical telemetry and memory measurements.
