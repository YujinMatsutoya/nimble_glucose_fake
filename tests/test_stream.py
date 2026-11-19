from glucose_client import validate_stream


async def test_baseline(sensor):
    await sensor.wait_for(10)
    validate_stream(sensor.packets)
