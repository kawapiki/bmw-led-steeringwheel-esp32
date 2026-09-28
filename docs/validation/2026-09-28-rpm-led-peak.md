# RPM LED scale and falling peak — 2026-09-28

The 23 RPM pixels on each chain now represent0–6500RPM proportionally; physical pixel0remains independent button illumination. A fresh valid0RPM keeps one amber status pixel. Up to1200RPM, all filled pixels are amber. Above idle, the low-range segment stays amber, the middle segment becomes progressively brighter green, and the final four scale positions transition progressively to red. Existing6500RPM shift flash and6350RPM hysteresis remain. Existing brightness ceiling25/255 is retained.

On falling RPM the main bar updates immediately. One separate peak pixel returns with constant acceleration9000RPM/s², starting at zero velocity. A4500RPM drop is caught in approximately1second; the pixel follows its scale color. Rising RPM catches/reset the peak when it overtakes it. This is time-based50Hz rendering, with no heap allocations or extra task. An interruption of2seconds or more snaps to the current input. Disconnection, invalid/stale RPM, maintenance, OTA writing or a manual LED test clears peak state; unavailable data never becomes the stopped-engine amber indication.

Native checks passed: exhaustive0–10000RPM proportional fill, idle boundary, four red positions, shift flashing, accelerated fall, catch/reset, timer wrap, different render cadences and extreme input clamp. Existing production DMA/GRB transport harness passed, including pixel0reservation, independent chains and failure handling. These software checks do not prove physical color order or perceived animation smoothness.

Model/camera work from the preceding request remains separate and uncommitted while awaiting the actual E90 asset. This LED change does not modify BLE, CAN or permanent recovery protocol.

## Bench build

ESP-IDF6.1 build and both-image slot/security audit passed. Wheel image1,789,952bytes, SHA2560eef03ee8ef0fb4b7c3eca230fed6b6e3da370d1109fe4c920d7726b92f90faf. Esptool verified the application-only write at0x620000. Stock backup integrity is unchanged; gateway firmware, NVS, partition table, otadata and eFuses were not written.

The wheel passed its boot self-test, received authenticated full-validity Demo telemetry, and retained approximately80kB minimum free heap in the initial capture. Physical color/peak appearance requires the owner's observation; serial telemetry alone cannot verify it. No new signed OTA release or full OTA test is part of this increment.

The60-second capture completed without a logged LED TX failure or panic. Minimum free heap was80,212bytes; sampled moving-Drive mean loop periods remained around8–9.4ms. This does not measure optical LED timing or establish endurance.
