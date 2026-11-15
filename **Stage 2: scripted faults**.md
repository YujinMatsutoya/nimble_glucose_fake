**Stage 2: scripted faults**

**Goal:** make the fake sensor fail on demand, deterministically, so the Python layer and the iOS app tests have known bad behaviour to catch.

**Faults**
1. **Drop the connection:** the peripheral disconnects, with a configurable reason. Optionally it doesn't resume advertising.
2. **Skip a sequence number:** jump forward by k to create a gap.
3. **Duplicate a sequence number:** resend the same one (optionally with a different value).
4. **Stale timestamp:** send one in the past, not advancing, or going backwards.
5. **Stop sending data:** keep the link up but go silent for N seconds or indefinitely.
6. **ATT error:** a characteristic that returns a chosen error, using NimBLE's `BLE_ATT_ERR_*` constants. Some errors need a protected characteristic, so decide how much pairing is in scope.

**Fault control**
- A writable fault control characteristic, so a central can arm faults over BLE, plus a serial interface for manual debugging.
- Each fault has an ID, parameters, a trigger (now, after N readings, or at sequence N), one-shot or repeating, a clear/reset command, and a serial log line when it fires.

**Done when**
- Every fault can be armed, fires as specified and can be cleared, from both BLE and serial.
- Each one is checked by hand in nRF Connect.
- `docs/FAULT_MATRIX.md` lists each fault, its parameters and the behaviour a correct central should show.
- With no fault armed, behaviour is unchanged.