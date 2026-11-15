# Fault matrix

Scripted faults make the fake sensor misbehave on demand. With no fault armed, behaviour is exactly as in `PROTOCOL.md`.

## Arming

One fault slot: arming replaces the armed fault. `clear` also ends a silence and resumes advertising after a no-re-advertise drop. Fault state survives a disconnect and is lost on reboot.

**BLE:** write (with response) 10 bytes, little-endian, to the fault characteristic.

| Item | UUID |
| --- | --- |
| Fault service | `5f8e3c1a-7b2d-4e90-a6c4-1d2f3b4a5c6e` |
| Fault control characteristic (WRITE) | `9a1b2c3d-4e5f-4061-8273-8495a6b7c8d9` |

| Offset | Field | Type | Notes |
| --- | --- | --- | --- |
| 0 | cmd | uint8 | 0 clear, 1 arm |
| 1 | fault id | uint8 | see table |
| 2 | trigger | uint8 | 0 now, 1 after N readings, 2 at seq N |
| 3 | trigger N | uint16 | |
| 5 | flags | uint8 | bit0 repeat, bit1 no-re-advertise (drop) |
| 6 | param | uint32 | per fault |

Example, skip 3 at seq 10: `01 02 02 0a 00 00 03 00 00 00`. Wrong length: `Invalid Attribute Value Length`. Invalid fault or parameters: `Value Not Allowed`.

**Serial** (console on the UART, prompt `fault>`):
```
fault arm <drop|skip|silence|1-3> [now|after=N|seq=N] [repeat] [param=V] [no-readv]
fault clear
fault status
```
Each firing logs `FAULT fired id=<n> name=<name> seq=<n> param=<v>`.

## Triggers

- **now:** the next reading.
- **after=N:** fires once N normal readings have gone by since arming, while subscribed. Repeating: N normal ones, then the fault, and so on.
- **seq=N:** fires on the reading that would carry seq N. Cannot be combined with repeat. Arming a seq that has already passed warns on serial and never fires.

## Faults

| ID | Name | param | Behaviour | A correct central should |
| --- | --- | --- | --- | --- |
| 1 | drop | HCI reason, 0 = 0x13 | Disconnects in place of a reading (seq not used). Advertising resumes unless no-re-advertise is set. | Report the disconnect, reconnect, resubscribe; numbering continues without a gap. With no-re-advertise, time out finding the device. |
| 2 | skip | k, 1..65535 | Seq jumps forward by k and stays advanced: k numbers are missing. | Detect a gap of k. |
| 3 | silence | seconds, 0 = until cleared | Stops sending; link and subscription stay up and seq does not advance. READ still works. | Notice the missing readings by timing and surface a data-stale state. |

Not covered: pairing, ATT errors, duplicate and stale-timestamp faults.
