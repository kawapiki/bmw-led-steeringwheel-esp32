# Initial custom installation and restoration — review only

No command in this plan has been executed. Ordinary build authorization does not authorize this hardware action. Both devices' firmware/partition layouts change. Keep the car CAN wiring disconnected; these images do not initialize CAN.

## Artifacts and private provisioning

Run tools/provision-development.py with the ESP-IDF Python environment once. Preserve .private/release-signing.pem securely for all future releases. Never publish .private, initial-nvs.csv/bin, pair_config.h or back/local. Generic OTA images contain public verification key only; the per-pair passkey is in the initial NVS image shared by these two devices. Do not recreate NVS on normal OTA.

Generate private initial NVS using IDF nvs_partition_gen.py generate .private/initial-nvs.csv .private/initial-nvs.bin 0x6000. The local private image has already been generated for review, not written to a device.

Before approval: run tools/check-system-build.py; verify exact device identity/port again (last observed wheel COM6 ESP32-S3, gateway COM7 ESP32 rev3.1). COM numbering is not identity. COM6 may require physical reconnect after reset. Confirm power, panel orientation/offset assumptions and preserve full backups.

## Initial write set

Use ESP-IDF6.1/esptool5 write-flash with DIO,40MHz and correct flash size. These are the **exact offset/file pairs** proposed; all listed regions are written and their sectors erased by write-flash. No whole-chip erase or eFuse command is proposed.

| Target | Flash size | Offset | File |
|---|---:|---:|---|
| Wheel |32MiB|0x0|firmware/wheel/build/bootloader/bootloader.bin|
| Wheel |32MiB|0x10000|firmware/wheel/build/partition_table/partition-table.bin|
| Wheel |32MiB|0x11000|.private/initial-nvs.bin|
| Wheel |32MiB|0x17000|firmware/wheel/build/ota_data_initial.bin|
| Wheel |32MiB|0x20000|firmware/wheel/build/wheel_demo.bin|
| Gateway |4MiB|0x1000|firmware/gateway/build/bootloader/bootloader.bin|
| Gateway |4MiB|0x10000|firmware/gateway/build/partition_table/partition-table.bin|
| Gateway |4MiB|0x11000|.private/initial-nvs.bin|
| Gateway |4MiB|0x17000|firmware/gateway/build/ota_data_initial.bin|
| Gateway |4MiB|0x20000|firmware/gateway/build/gateway_demo.bin|

Do not use unmodified idf.py flash for initial provisioning: its default write set omits our private NVS image. Do not preserve the stock otadata or NVS at their old addresses under the new partition scheme. Generated fresh ota_data_initial.bin selects the first custom slot. Wheel slots are6MiB; gateway slots0x1e0000 bytes. First custom installation has no previous custom image to roll back to; external full backup is the restoration path.

After approved writes: verify written regions, start gateway then wheel within the120-second first-pairing window, inspect boot logs and run component qualification before connecting anything to the car. Image self-test cannot prove screen orientation, GPIO polarity, usable frame rate or electrical safety.

## Full restoration

Disconnect peripherals if needed, identify chip again, enter ROM loader, then write the appropriate **whole image at offset0** using correct chip/flash size. Verify against that exact image afterwards.

- Wheel original immutable factory backup back/steering-wheel-original.bin:32MiB; SHA256 c8f796755d535f5eb5bdb820e3499595982c86fcfb986f1eb7ec6508f27d3e45.
- Wheel latest private original-state snapshot back/local/wheel-verified-20260927.bin:32MiB; SHA256 d6d548bb21751683ed925eb6de457b2ad048eb3894e80a3864fb546b797e4f43.
- Gateway private pre-custom snapshot back/local/lilygo-verified-20260927.bin:4MiB; SHA256 a15517044001ae4204357b0851d5c447eb5e6adc56678f8171bd91b8de4cbded.

Restoring full flash replaces custom firmware, partition table, NVS and pairing. Backups do not restore eFuses; this implementation does not change them. Keep private snapshots separate from GitHub and public release assets.
