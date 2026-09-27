# Recovery v1 wire candidate (not deployed)

This implementation develops the v1 ABI; it has not been exercised across actual endpoints. UUIDs and vectors below are candidates until hardware interoperability and mixed-version tests pass. The permanent backward-compatibility rule in ota-recovery-v1.md remains binding.

All integers are little-endian. Recovery service UUID is `90000001-0000-1000-8000-0001e9900919`; read/write characteristic is `90000002-0000-1000-8000-0001e9900919`. Independent application service/characteristic use first group `90000003` / `90000004`; app payload is four uint32 values: major1, sequence, simulated RPM, gateway uptime-ms. Unknown app major only invalidates telemetry, never recovery.

Recovery logical frame: magic E9 90; version1; opcode; payload uint16 length; uint16 reserved-zero; uint32 session; uint32 request; uint64 transaction. Maximum payload128, frame152. ATT writes use offset-byte, total-byte and1–18 payload bytes, so default MTU23 works. Assembly timeout3000ms; out-of-order, oversized or malformed fragments clear assembly. Authentication precedes assembly.

Opcodes:
- 1 HELLO, 2 STATUS, 7 RESULT: zero payload; publish current recovery status and last accepted request.
- 3 WIFI_CONFIG: SSID length-byte, password length-byte,1–32 SSID bytes (NUL forbidden),8–63 printable-ASCII WPA2 password bytes. Never log or include in status/journal.
- 4 PREPARE_UPDATE: uint32 release and32-byte gateway file SHA256.
- 5 START_UPDATE: zero payload, must match prepared transaction. Installation independently authenticates manifest and image.
- 6 CANCEL_BEFORE_COMMIT: zero payload. Cancels prepared state or a download at the next chunk boundary; cannot undo boot commit.

GATT status64 bytes: BST1 magic; uint32 phase; uint32 release; uint64 transaction;32-byte file digest; uint32 gateway boot session; uint32 last accepted request; uint32 recovery major1. Phases0 idle,1 prepared,2 downloading,3 boot pending,4 verified running image,5 failed/cancelled. A read starts a coherent snapshot preserved across ATT Read Blob chunks. Status contains no credentials.

HELLO golden logical bytes:
`e99001010000000001000000020000000300000000000000`.
Golden MTU23 fragments:
`0018e99001010000000001000000020000000300`
`1218000000000000`.

Only actual encrypted, authenticated, bonded, persisted approved identity is accepted. First pairing uses a private per-pair passkey provisioned into NVS and a120-second initial window. Existing identity is never silently replaced. Generic OTA binaries have no pair-secret. Firmware contains only public release-verification material.

Release manifest480 bytes: signed payload96 then384-byte RSA3072 PKCS1v1.5/SHA256 signature. Payload: BMW1; uint32 release; uint32 wheel-size; uint32 gateway-size;32-byte wheel SHA256;32-byte gateway SHA256; four uint32 schema values(partition1/config1/recovery1/application-major positive). The application-major is metadata and never gates recovery or update eligibility. This compact authenticated envelope replaces the earlier proposed JSON-plus-separately-signed-image transport. Human-readable manifest.json accompanies it; it is not trusted by devices. OTA image authenticity comes from the verified envelope binding the exact streamed file digest. Hardware Secure Boot/eFuses remain off.

Public GitHub tags are system-rN; assets manifest.bin, wheel.bin, gateway.bin. The wheel examines at most3 pages of5 releases with24KiB response bound; drafts are ignored, published prereleases allowed. Both targets independently validate target, size, monotonic release, signature, SHA256 and ESP image integrity. Public HTTPS only; IDF6.1 rejects downgrade redirects; max5 requests. TLS requires time initialization.

The wheel persists intended transaction before sending mutations. Resume requires fresh physical confirmation; it queries current authenticated status before continuing. Gateway successful boot requires self-test and running-partition SHA256 before phase4. The wheel uses the shared native-tested transaction/digest gate before updating itself. This is sequential recovery, not atomic two-device commit.

Remaining hardware qualification: MTU23 pairing/reconnect, incompatible-app versions, radio coexistence, dropped frames and power loss at every journal/commit boundary.
