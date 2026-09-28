# Operational wheel and gateway simulation boundary

The wheel is the operational presentation/controller firmware. Simulation runs exclusively on the gateway, substituting for the future CAN decoder and publishing the same normalized state. The normal wheel UI must not have a demo mode, invent changing vehicle data, or alter alert behavior based on simulated provenance. Keep origin in Gateway/Service diagnostics. This architectural intention is not vehicle qualification or a claim of validated CAN decoding.

Startup animation is decorative only. Any fresh opening, regardless of provenance, preempts it. The gateway bench scenario begins closed for6seconds so normal startup can be observed without a source-dependent wheel exception. Unknown/disconnected inputs remain unknown; valid zero is distinct. No CAN transmission is enabled.

## Additive lighting v1 characteristic

Use application UUID selector6, leaving selectors4/5 and recovery v1 byte-for-byte unchanged. Optional16-byte ATT-readable packet:

| Offset | Bytes | Meaning |
|---|---|---|
|0|1|Version1|
|1|1|Source1simulated gateway,2validated vehicle producer|
|2|1|Validity bits0low beam,1high beam,2angel eye,3left indicator,4right indicator,5brake|
|3|1|Current on bits in the same order; upper bits zero|
|4|4|LE sequence|
|8|4|LE producer uptime ms|
|12|4|Reserved zero|

Indicator bits describe the current lit phase. The wheel must not synthesize blinking from a local timer; a future CAN backend must normalize any command/phase difference before publishing. Each bit is independently valid; stale or unsupported is unknown, not off. Separate receipt time and sequence guard; authenticate, use current session, reject excessive read latency/replay. Missing optional characteristic leaves lights unknown without disabling legacy cockpit or OTA. No lights are inferred from RPM/speed.

Gateway owns the reproducible bench sequence. Steering UI consumes generic valid/on fields with no BMW IDs or simulator scheduling. Lighting is an additive app extension; existing peer compatibility and the frozen recovery protocol remain protected.
