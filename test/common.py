"""
Common test functions
"""

from typing import Dict, Any
import json

TEST_MODULE_ADDR = 1
INACTIVE_MODULE_ADDR = 2
CONFIG_FN = 'mtb-daemon-test.json'

with open(CONFIG_FN, 'r') as file:
    CONFIG_JSON = json.loads(file.read())

MODULES_JSON = {int(addrstr): module for addrstr, module in CONFIG_JSON['modules'].items()}
assert TEST_MODULE_ADDR in MODULES_JSON.keys()
assert INACTIVE_MODULE_ADDR in MODULES_JSON.keys()


class MtbDaemonError:
    MODULE_INVALID_ADDR = 1100
    MODULE_FAILED = 1102
    INVALID_SPEED = 1105
    INVALID_DV = 1106
    MODULE_ACTIVE = 1107
    FILE_CANNOT_ACCESS = 1010
    MODULE_ALREADY_WRITING = 1110
    UNKNOWN_COMMAND = 1020

    DEVICE_DISCONNECTED = 2004
    ALREADY_STARTED = 2012

    MODULE_UPGRADING_FW = 3110
    MODULE_IN_BOOTLOADER = 3111
    MODULE_CONFIG_SETTING = 3112
    MODULE_REBOOTING = 3113
    MODULE_FWUPGD_ERROR = 3114

    MODULE_UNKNOWN_COMMAND = 0x1001
    MODULE_UNSUPPORTED_COMMAND = 0x1002
    MODULE_BAD_ADDRESS = 0x1003
    SERIAL_PORT_CLOSED = 0x1010
    USB_NO_RESPONSE = 0x1011
    BUS_NO_RESPONSE = 0x1012


def check_version_format(version: str) -> None:
    versions = version.split('.')
    assert len(versions) == 2
    major, minor = versions
    assert major.isdigit()
    assert minor.isdigit()


def check_error(response: Dict[str, Any], error_id: int) -> None:
    assert 'status' in response
    assert isinstance(response['status'], str)
    assert response['status'] == 'error'

    assert 'error' in response
    assert isinstance(response['error'], dict)
    error = response['error']

    assert 'code' in error
    assert isinstance(error['code'], int)
    assert response['error']['code'] == error_id

    assert 'message' in error
    assert isinstance(error['message'], str)
    assert error['message'] != ''
