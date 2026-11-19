import asyncio

import pytest

from glucose_client import (DROP, DROP_NO_READV, SCAN_S, DeviceNotFound,
                            validate_stream)


async def test_drop_and_recover(sensor):
    await sensor.wait_for(3)
    await sensor.write(DROP)
    await sensor.wait_disconnect()
    held = len(sensor.packets)
    await sensor.recover()
    await sensor.wait_for(held + 3)  # only arrives if resubscribed
    validate_stream(sensor.packets)  # seq continues across the drop, no gap


async def test_no_readvertise(serial_clear, sensor):
    # serial_clear is listed first so the test skips before connecting
    await sensor.write(DROP_NO_READV)
    await sensor.wait_disconnect()
    with pytest.raises(DeviceNotFound):
        await asyncio.wait_for(sensor.connect(), SCAN_S + 5)
