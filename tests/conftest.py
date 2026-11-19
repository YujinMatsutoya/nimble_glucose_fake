import pytest
import serial

from glucose_client import CLEAR, Sensor


def pytest_addoption(parser):
    parser.addoption("--interval-ms", type=int, default=3000,
                     help="GLUCOSE_INTERVAL_MS the board was built with")
    parser.addoption("--serial-port", default=None,
                     help="board console, needed to recover from no-re-advertise")


@pytest.fixture
async def sensor(request):
    """Connected, subscribed, fault cleared; cleared and disconnected after."""
    s = Sensor(request.config.getoption("--interval-ms") / 1000)
    await s.connect()
    await s.write(CLEAR)
    yield s
    await s.close()


@pytest.fixture
def serial_clear(request):
    """Skip without a port; after the test, 'fault clear' restarts advertising."""
    port = request.config.getoption("--serial-port")
    if not port:
        pytest.skip("needs --serial-port")
    yield
    # DTR/RTS low so opening the port does not reset the board
    s = serial.Serial()
    s.port, s.baudrate, s.dtr, s.rts = port, 115200, False, False
    s.open()
    s.write(b"fault clear\n")
    s.close()
