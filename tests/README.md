# Python test layer

Checks the fake sensor's data stream and its drop fault from a Python central (`bleak`), so it can be trusted before the app tests run against it.

## Run

Flash the board, then:

```
cd tests
uv run pytest                                   # all tests, writes report.xml (JUnit) and logs/last.log
uv run pytest --serial-port=/dev/cu.usbserial-XXXX   # also runs the no-re-advertise test
uv run pytest test_validator.py                 # no hardware needed
```

Options: `--interval-ms` (default 3000, must match `GLUCOSE_INTERVAL_MS` in the build) and `--serial-port`. Close `idf.py monitor` first, only one program can hold the port.

On macOS the terminal app needs Bluetooth permission (System Settings, Privacy & Security, Bluetooth). `bleak` identifies the device by UUID, not MAC address.

## Tests

| Test | What it proves |
| --- | --- |
| `test_validator.py` (5 cases) | The packet checks (length, ranges, seq gap, seq wrap) work, with no hardware. This covers gap detection without a skip fault. |
| `test_stream.py::test_baseline` | Connect, subscribe, 10 readings arrive with contiguous seq and valid values. |
| `test_drop.py::test_drop_and_recover` | After a drop fault the central sees the disconnect, reconnects within 30 s, resubscribes, and numbering continues with no gap. |
| `test_drop.py::test_no_readvertise` | After a drop with no re-advertising the central ends in "device not found" after a 20 s scan, not a hang. Skipped without `--serial-port`. |

Every test starts with the fault cleared (BLE write) and clears it again afterwards. After the no-re-advertise test the fixture sends `fault clear` over serial so the board advertises again. Every connect, disconnect and reading is logged with a timestamp and raw hex in `logs/last.log`.

## Known limits

- Readings are checked for plausibility from the first one received (seq contiguity, 3.0 to 15.0 mmol/L, step at most 0.3), not for exact values, because the generator only resets on reboot.
- BLE timing on macOS is the main source of flakiness. After a disconnect the board restarts advertising within about 50 ms, but CoreBluetooth can take 3 to 15 s to report it again. The timeouts (`SCAN_S` = 20 s, `RECONNECT_S` = 30 s in `glucose_client.py`) are set to absorb this, which is also why the no-re-advertise test takes 20 s.
- There are no retries. A failure is a failure, so a timeout failure is worth a rerun. Check `logs/last.log`: the gap between `scan started` and `device found` shows whether the scan was slow or the board never advertised (the board's serial log shows `advertising started!` after each disconnect).
- The client takes the first device advertising the service UUID, so two boards in range are not detected.
- Not covered: the reason code of a drop (not visible to the central on macOS), skip and silence faults, the iOS app.
