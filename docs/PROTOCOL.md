# Glucose Fake Device: BLE protocol

A simulated glucose sensor. It is not a medical device and does not use the SIG Glucose or CGM services.

## Advertising
- Device name: `GLUCOSE_FAKE`
- The 128-bit service UUID is in the scan response (it does not fit beside the name in the 31-byte advertising data).
- No pairing or encryption.

## GATT
Besides the stack-provided Generic Access and Generic Attribute services:

| Item | UUID | Properties |
| --- | --- | --- |
| Glucose service (primary) | `e69cd7ab-9264-4243-9d7d-4df96f280b41` | |
| Glucose reading characteristic | `eba44a3d-2184-47d0-a02b-c887544d4747` | READ + INDICATE (default, `GLUCOSE_DELIVERY_INDICATE`), or READ + NOTIFY (`GLUCOSE_DELIVERY_NOTIFY`) |

The characteristic has a Client Characteristic Configuration descriptor (0x2902). Readings are sent only while the central has enabled the configured delivery method (notifications or indications).

## Packet (9 bytes, little-endian)

| Offset | Field | Type | Notes |
| --- | --- | --- | --- |
| 0 | sequence | uint16 | +1 per reading sent; wraps 65535 -> 0 |
| 2 | timestamp | uint32 | seconds since boot, monotonic |
| 6 | glucose | uint16 | mmol/L x 10 (56 = 5.6 mmol/L) |
| 8 | flags | uint8 | reserved, always 0 |

Example: `01 00 0a 00 00 00 38 00 00` = seq 1, t = 10 s, 5.6 mmol/L.

## Behaviour
- **Sequence:** advances only when a reading is sent, which is only while subscribed. It is not reset on disconnect or resubscribe, so a central sees continuous numbering (until the device reboots, which restarts at 0 and resets the timestamp). The first reading has seq 1.
- **READ:** returns the latest reading without advancing the sequence (seq 0 and 5.6 mmol/L before the first send).
- **Generator:** bounded random walk using xorshift32 seeded by `GLUCOSE_SEED` (default 1): start 5.6, step in [-0.3, +0.3] mmol/L, clamped to 3.0-15.0 mmol/L. The same seed gives the same sequence after every boot, provided the same number of readings is requested.
- **Interval:** `GLUCOSE_INTERVAL_MS`, default 3000 ms. Real CGMs send a reading every few minutes.
- **Differences from a real CGM:** no on-device buffering or backfill of readings missed while disconnected, no Record Access Control Point, no sensor status or trend fields.
