Stage 1: Strip down to the essentials and make it a glucose reading

Goal: a minimal BLE peripheral that behaves like the example's heart rate service, but sends simulated glucose readings.

Scope

Start from NimBLE_GATT_Server. Remove what is not needed (the Automation IO / LED service, unused heart rate pieces). Keep the structure of the example's service table (gatt_svr_svcs), access callbacks (heart_rate_chr_access-style), subscription callback (gatt_svr_subscribe_cb) and sender task, because I want to learn that structure and later inject faults there.
One primary service with one data characteristic that supports subscription, plus the standard Generic Access/Generic Attribute services the stack provides.
Replace heart rate data with a glucose reading packet (below). Send periodically while a central is subscribed.
Serial logging of connect, disconnect, subscribe/unsubscribe, and each reading sent (including sequence number).

Default design (open to grilling)

Service/characteristic: custom 128-bit UUIDs, not Bluetooth SIG standard CGM/Glucose services. Reason: simpler to start with, easy to extend. Revisit whether using the SIG Glucose or CGM service would give more realistic practice (see Open questions).
Packet (little-endian), initial proposal:
Field	Type	Notes
sequence	uint16	increments by 1 per reading, wraps at 65535 (define wrap behaviour explicitly)
timestamp	uint32	seconds since device boot (default), monotonic; define semantics in PROTOCOL.md
glucose	uint16	mmol/L x 10 (e.g. 5.6 mmol/L = 56). UK units. Unit to confirm.
flags	uint8	reserved, 0 for now
Reading generator: deterministic and seedable (a simple pseudo-random walk or scripted profile within a plausible range such as 3.0 to 15.0 mmol/L), so tests are repeatable.
Interval: configurable, default a few seconds for fast tests. Note that real CGMs send readings every few minutes; document this difference.
Delivery: notifications by default. Make indications selectable by a build-time option so I can compare the two (the base example uses indications).
Security: none in Stage 1 (no pairing). Revisit in Stage 2 if the ATT error list needs a protected characteristic.

Acceptance criteria

Builds and flashes with a pinned ESP-IDF version (recorded in README).
nRF Connect shows the device name, the service and the characteristic with the expected properties.
Subscribing shows readings arriving at the configured interval with an incrementing sequence number; decoding by hand matches the serial log.
Unsubscribing stops sending; reconnecting and resubscribing works.
docs/PROTOCOL.md documents UUIDs, properties, packet layout, units and wrap behaviour.
Dead code from the original example is removed, and the diff against the example is easy to read.