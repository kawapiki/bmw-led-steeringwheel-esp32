# Gateway demo firmware

Target: ESP32-D0WDQ6-V3,4MiB flash, ESP-IDF6.1. Builds separately with tools/build-gateway.ps1. It exposes authenticated BLE application demo telemetry and independent recovery/control GATT, receives Wi-Fi credentials only from the approved bonded wheel, and independently verifies/installs its signed-manifest target through A/B OTA.

CAN/TWAI is not initialized. No CAN transmission, seat control or vehicle-state claim is present. The published RPM is synthetic.

Same private pairing NVS must be provisioned to both devices during the approved initial USB installation. Initial pairing window120seconds; normal reconnect uses the preserved bond/identity. Stock firmware was a Wi-Fi extender without OTA slots, so initial partition migration is required. Read docs/validation/initial-custom-flash-plan.md.

Two0x1e0000-byte OTA slots fit4MiB. App self-test checks initialized recovery host/journal, internal heap and exact installed image digest for pending updates. It never requires internet or another device for boot validity. Runtime pairing, coexistence and fault-injection tests remain unperformed.
