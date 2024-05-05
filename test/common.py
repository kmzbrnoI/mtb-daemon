"""
Common test functions
"""

from typing import Dict, Any, Self
import json

from mtbdaemonif import mtb_daemon, MtbDaemonIFace

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
    MODULE_INVALID_PORT = 1101
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


def check_invalid_addresses(request: Dict[str, Any], addr_key: str) -> None:
    """
    Sends request `request` with several invalid addresses and checks that the
    server replies with proper error.
    """
    INVALID_ADDRS = [0, -1, 0x1FF, 49840938, 'hello', '0x24']
    for addr in INVALID_ADDRS:
        request[addr_key] = addr
        response = mtb_daemon.request_response(request, ok=False)
        check_error(response, MtbDaemonError.MODULE_INVALID_ADDR)


def set_single_output(addr: int, output: int, value: int) -> None:
    mtb_daemon.request_response({
        'command': 'module_set_outputs',
        'address': addr,
        'outputs': {str(output): {'type': 'plain', 'value': value}}
    })


def validate_oc_event(event: Dict[str, Any], addr: int, port: int, value: int) -> None:
    assert 'module_outputs_changed' in event
    moc_event = event['module_outputs_changed']
    assert 'address' in moc_event
    assert moc_event['address'] == addr
    assert 'outputs' in moc_event
    assert str(port) in moc_event['outputs']
    json_port = moc_event['outputs'][str(port)]
    assert json_port['type'] == 'plain'
    assert json_port['value'] == value


def validate_ic_event(event: Dict[str, Any], addr: int, port: int, value: bool) -> None:
    assert 'module_inputs_changed' in event
    moc_event = event['module_inputs_changed']
    assert 'address' in moc_event
    assert moc_event['address'] == addr
    assert 'inputs' in moc_event
    assert 'full' in moc_event['inputs']
    assert 'packed' in moc_event['inputs']
    assert moc_event['inputs']['full'][port] == value


class ModuleSubscription:
    def __init__(self, daemon: MtbDaemonIFace, addr: int):
        self.daemon: MtbDaemonIFace = daemon
        self.addr: int = addr

    def __enter__(self) -> Self:
        self.daemon.request_response({
            'command': 'module_subscribe',
            'addresses': [self.addr],
        })
        return self

    def __exit__(self, exception_type, exception_value, exception_traceback) -> None:
        self.daemon.request_response({
            'command': 'module_unsubscribe',
            'addresses': [self.addr],
        })
