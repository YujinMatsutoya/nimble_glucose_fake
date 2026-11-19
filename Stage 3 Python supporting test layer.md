Stage 3: Python supporting test layer (small, timeboxed)

Goal: fast, repeatable checks of the fake sensor’s drop behaviour and the data stream, so you can trust it before the app tests run against it. Timebox it to one or two days.

Stack: Python with bleak and pytest (plus pytest-asyncio), with pyserial if you need serial. On macOS, bleak identifies devices by UUID, not MAC address, and the terminal needs Bluetooth permission.

Design

A small reading-stream validator that checks sequence continuity and value format.
Tests start from a known state: fault cleared, seeded generator, device found by name or service UUID, explicit timeouts.
Log the raw bytes and a timestamp for every notification and connection event.

Tests

Baseline: connect, subscribe, collect N readings with a contiguous sequence and valid values.
Drop and recover:
Arm drop, and assert the disconnect is detected.
Assert the central reconnects within a time limit and resubscribes.
Assert readings resume and numbering continues with no gap.
No re-advertise: assert the central fails cleanly with a timeout when it can’t find the device, not a hang.
Validator unit test (no hardware): feed hand-made byte sequences with a gap, so the gap-detection logic is still covered without a skip fault.

Done when

One command runs the suite, gives pass/fail and a JUnit-style report, and leaves the peripheral in a known state.
Test 2 fails if the central doesn’t reconnect or resubscribe.
Flaky tests are handled openly (a retry policy or documented limits).
The README says how to run the tests and what each one proves.