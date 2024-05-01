"""
Test 'mtbusb' endpoint of MTB Daemon TCP server using PyTest.
"""

import mtbdaemonif

mtb_daemon = mtbdaemonif.MtbDaemonIFace()


def test_endpoint_present() -> None:
    mtb_daemon.request_response({'command': 'mtbusb'})
    mtb_daemon.request_response({'command': 'mtbusb'})  # with different id


def check_version_format(version: str) -> None:
    versions = version.split('.')
    assert len(versions) == 2
    major, minor = versions
    assert major.isdigit()
    assert minor.isdigit()


def test_common_response() -> None:
    response = mtb_daemon.request_response({'command': 'mtbusb'})
    assert 'mtbusb' in response
    mtbusb = response['mtbusb']

    assert 'connected' in mtbusb
    assert isinstance(mtbusb['connected'], bool)
    assert mtbusb['connected'], 'Probably disconnected from MTB-USB'

    assert 'type' in mtbusb
    assert isinstance(mtbusb['type'], int)
    assert mtbusb['type'] == 1

    assert 'speed' in mtbusb
    assert isinstance(mtbusb['speed'], int)
    assert mtbusb['speed'] in [38400, 57600, 115200, 230400], 'Invalid MTBbus speed!'

    assert 'firmware_version' in mtbusb
    assert isinstance(mtbusb['firmware_version'], str)
    check_version_format(mtbusb['firmware_version'])

    assert 'protocol_version' in mtbusb
    assert isinstance(mtbusb['protocol_version'], str)
    check_version_format(mtbusb['protocol_version'])


def test_active_modules() -> None:
    response = mtb_daemon.request_response({'command': 'mtbusb'})
    mtbusb = response['mtbusb']

    assert 'active_modules' in mtbusb
    assert isinstance(mtbusb['active_modules'], list)
    assert mtbusb['active_modules'] == [1], 'Only module "1" should be active on the bus!'
