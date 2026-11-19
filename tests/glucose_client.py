"""Reading validator and a small reference BLE client for the fake sensor."""
import asyncio
import logging
import struct
import time
from collections import namedtuple

from bleak import BleakClient, BleakScanner

log = logging.getLogger("glucose")

SERVICE_UUID = "e69cd7ab-9264-4243-9d7d-4df96f280b41"
READING_UUID = "eba44a3d-2184-47d0-a02b-c887544d4747"
FAULT_UUID = "9a1b2c3d-4e5f-4061-8273-8495a6b7c8d9"

# Fault commands (docs/FAULT_MATRIX.md): cmd, id, trigger, n(2), flags, param(4)
CLEAR = bytes(10)
DROP = bytes([1, 1, 0, 0, 0, 0, 0, 0, 0, 0])
DROP_NO_READV = bytes([1, 1, 0, 0, 0, 2, 0, 0, 0, 0])

# Timeouts in seconds
SCAN_S = 20
RECONNECT_S = 30
READING_WAIT_INTERVALS = 3  # wait this many intervals for the next reading

Reading = namedtuple("Reading", "seq timestamp glucose flags")


def validate(prev, data):
    """Decode one packet and check it against the previous reading."""
    if len(data) != 9:
        raise ValueError(f"packet is {len(data)} bytes, expected 9")
    r = Reading(*struct.unpack("<HIHB", data))
    if r.flags != 0:
        raise ValueError(f"flags {r.flags}, expected 0")
    if not 30 <= r.glucose <= 150:
        raise ValueError(f"glucose {r.glucose} outside 30..150")
    if prev is not None:
        if r.seq != (prev.seq + 1) & 0xFFFF:
            raise ValueError(f"seq gap: {prev.seq} -> {r.seq}")
        if r.timestamp < prev.timestamp:
            raise ValueError(f"timestamp went back: {prev.timestamp} -> {r.timestamp}")
        if abs(r.glucose - prev.glucose) > 3:
            raise ValueError(f"glucose step {prev.glucose} -> {r.glucose}")
    return r


def validate_stream(packets):
    """Validate packets in order; the first one only gets the format checks."""
    prev, out = None, []
    for p in packets:
        prev = validate(prev, p)
        out.append(prev)
    return out


class DeviceNotFound(Exception):
    pass


class Sensor:
    """Connects, subscribes and keeps every raw packet, across reconnects."""

    def __init__(self, interval_s):
        self.interval_s = interval_s
        self.packets = []
        self.client = None
        self.disconnected = asyncio.Event()

    async def connect(self):
        # shortcut: takes the first matching device, does not detect two sensors
        log.info("scan started")
        dev = await BleakScanner.find_device_by_filter(
            lambda d, adv: SERVICE_UUID in adv.service_uuids, timeout=SCAN_S
        )
        if dev is None:
            log.info("scan ended, device not found")
            raise DeviceNotFound(f"no device advertising {SERVICE_UUID} in {SCAN_S}s")
        log.info("device found %s", dev.address)
        self.disconnected.clear()
        self.client = BleakClient(dev, disconnected_callback=self._on_disconnect)
        await self.client.connect()
        log.info("connected %s", dev.address)
        await self.client.start_notify(READING_UUID, self._on_data)

    async def recover(self):
        """Reconnect and resubscribe within RECONNECT_S."""
        await asyncio.wait_for(self.connect(), RECONNECT_S)

    async def write(self, cmd):
        log.info("fault write %s", cmd.hex())
        await self.client.write_gatt_char(FAULT_UUID, cmd, response=True)

    async def wait_for(self, n):
        """Wait until n packets are held; fail if readings stop arriving."""
        limit = READING_WAIT_INTERVALS * self.interval_s
        last_change, seen = time.monotonic(), len(self.packets)
        while len(self.packets) < n:
            if len(self.packets) != seen:
                last_change, seen = time.monotonic(), len(self.packets)
            if time.monotonic() - last_change > limit:
                raise TimeoutError(f"no reading for {limit}s ({seen}/{n})")
            await asyncio.sleep(0.05)

    async def wait_disconnect(self):
        await asyncio.wait_for(
            self.disconnected.wait(), READING_WAIT_INTERVALS * self.interval_s
        )

    async def close(self):
        if self.client is not None and self.client.is_connected:
            await self.write(CLEAR)
            await self.client.disconnect()

    def _on_data(self, _, data):
        log.info("reading %s", bytes(data).hex())
        self.packets.append(bytes(data))

    def _on_disconnect(self, _):
        log.info("disconnected")
        self.disconnected.set()
