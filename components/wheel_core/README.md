# Portable initial control logic

Built unchanged into ESP-IDF and the native host test DLL. No allocation, logging,
GPIO, flash access or radio callbacks.

Recovery envelope candidate v1 (not deployed):
- bytes 0..1: E9 90
- byte 2: recovery major, 1
- byte 3: opcode 1 HELLO, 2 STATUS, 3 WIFI_CONFIG, 4 PREPARE_UPDATE,
  5 START_UPDATE, 6 CANCEL_BEFORE_COMMIT, 7 RESULT
- bytes 4..5: payload byte count, LE, maximum 128
- bytes 6..7: reserved zero
- bytes 8..11: session ID, LE
- bytes 12..15: request ID, LE
- bytes 16..23: transaction ID, LE
- remaining bytes: payload

WIFI_CONFIG payload: SSID length (1 byte), passphrase length (1 byte),
SSID bytes (1..32), printable ASCII WPA2 passphrase (8..63).
Never log the payload. Buffers containing it must be cleared by their owning transport.
Return codes: 0 envelope valid, -1 size/null, -2 unsupported format, -3 encrypted/bonded/trusted-partner precondition,
-4 malformed credentials. Other opcode payload semantics are deliberately NOT validated
here yet; no handlers exist. Passing this validator alone never authorizes an update.

No application-protocol version enters the validator. Future recovery service negotiation
must remain separate from telemetry Ready. UUIDs, fragmentation and operation schemas still
need endpoint interoperability tests before ABI publication/freezing.

The update gate checks an authenticated fresh journal snapshot, matching nonzero transaction, release and nonzero 32-byte digest. The future transport/journal must derive those fields from verified responses, not caller-supplied packet assertions. A standalone pass means wheel-only permission, never paired success.
