"""Validator tests, no hardware needed."""
import struct

import pytest

from glucose_client import validate_stream


def pkt(seq, ts=0, glucose=56, flags=0):
    return struct.pack("<HIHB", seq, ts, glucose, flags)


def test_clean_run():
    assert len(validate_stream([pkt(1, 3), pkt(2, 6), pkt(3, 9, 58)])) == 3


def test_gap():
    with pytest.raises(ValueError, match="seq gap: 2 -> 5"):
        validate_stream([pkt(1), pkt(2), pkt(5)])


def test_seq_wrap():
    validate_stream([pkt(65535), pkt(0)])


def test_short_packet():
    with pytest.raises(ValueError, match="expected 9"):
        validate_stream([pkt(1)[:8]])


def test_glucose_out_of_range():
    with pytest.raises(ValueError, match="outside 30..150"):
        validate_stream([pkt(1, glucose=151)])
