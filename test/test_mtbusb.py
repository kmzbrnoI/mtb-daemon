"""
Test 'mtbusb' endpoint of MTB Daemon TCP server using PyTest.
"""

from typing import Dict, Any
import time

import common
import mtbdaemonif

MTBBUS_SPEEDS = [38400, 57600, 115200, 230400]

mtb_daemon = mtbdaemonif.MtbDaemonIFace()


def test_endpoint_present() -> None:
    mtb_daemon.request_response({'command': 'mtbusb'})
    mtb_daemon.request_response({'command': 'mtbusb'})  # with different id


def validate_mtbusb_response(mtbusb: Dict[str, Any]) -> None:
    assert 'connected' in mtbusb
    assert isinstance(mtbusb['connected'], bool)
    assert mtbusb['connected'], 'Probably disconnected from MTB-USB'

    assert 'type' in mtbusb
    assert isinstance(mtbusb['type'], int)
    assert mtbusb['type'] == 1

    assert 'speed' in mtbusb
    assert isinstance(mtbusb['speed'], int)
    assert mtbusb['speed'] in MTBBUS_SPEEDS, 'Invalid MTBbus speed!'

    assert 'firmware_version' in mtbusb
    assert isinstance(mtbusb['firmware_version'], str)
    common.check_version_format(mtbusb['firmware_version'])

    assert 'protocol_version' in mtbusb
    assert isinstance(mtbusb['protocol_version'], str)
    common.check_version_format(mtbusb['protocol_version'])


def test_common_response() -> None:
    response = mtb_daemon.request_response({'command': 'mtbusb'})
    assert 'mtbusb' in response
    validate_mtbusb_response(response['mtbusb'])


def test_active_modules() -> None:
    response = mtb_daemon.request_response({'command': 'mtbusb'})
    mtbusb = response['mtbusb']

    assert 'active_modules' in mtbusb
    assert isinstance(mtbusb['active_modules'], list)
    assert mtbusb['active_modules'] == [1], 'Only module "1" should be active on the bus!'


def test_change_speed() -> None:
    for speed in MTBBUS_SPEEDS:
        response = mtb_daemon.request_response({'command': 'mtbusb', 'mtbusb': {'speed': speed}})
        assert 'mtbusb' in response
        validate_mtbusb_response(response['mtbusb'])
        assert response['mtbusb']['speed'] == speed
        time.sleep(0.5)
    # End with the highest speed
