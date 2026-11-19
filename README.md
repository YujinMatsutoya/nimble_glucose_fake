| Supported Targets | ESP32 |
| ----------------- | ----- |

# NimBLE Glucose Fake Device

## Overview

A minimal BLE peripheral that behaves like a glucose sensor, sending simulated readings. It is derived from the NimBLE GATT Server example and keeps that structure (GATT services table, characteristic access callback, subscribe callback, sender task).

- Advertises as `GLUCOSE_FAKE` with one custom primary service and one READ + INDICATE characteristic (READ + NOTIFY selectable in menuconfig under "Glucose Fake Device").
- While a central is subscribed, sends a 9-byte reading every `GLUCOSE_INTERVAL_MS` (default 3 s) from a seedable random walk (3.0-15.0 mmol/L).
- Logs connect, disconnect, subscribe/unsubscribe and every reading sent (with sequence number) on the serial console.

UUIDs, packet layout, units and behaviour are documented in [docs/PROTOCOL.md](docs/PROTOCOL.md).

Tested with ESP-IDF v6.1 on ESP32. To test, use *nRF Connect for Mobile*.

## Try It Yourself

### Set Target

Before project configuration and build, be sure to set the correct chip target using:

``` shell
idf.py set-target <chip_name>
```

For example, if you're using ESP32, then input

``` Shell
idf.py set-target esp32
```

### Build and Flash

Run the following command to build, flash and monitor the project.

``` Shell
idf.py -p <PORT> flash monitor
```

For example, if the corresponding serial port is `/dev/ttyACM0`, then it goes

``` Shell
idf.py -p /dev/ttyACM0 flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.

## Tests

Python tests for the data stream and the drop fault are in [tests/](tests/README.md). Fault control is documented in [docs/FAULT_MATRIX.md](docs/FAULT_MATRIX.md).
