"""
Test general module endpoints of MTB Daemon TCP server using PyTest.
"""

from typing import Dict, Any
import time

import common
from mtbdaemonif import mtb_daemon, MtbDaemonIFace


def test_endpoints_present() -> None:
    mtb_daemon.request_response({'command': 'modules'})
    mtb_daemon.request_response({'command': 'module', 'address': common.TEST_MODULE_ADDR})
    response = mtb_daemon.request_response({'command': 'module'}, timeout=1, ok=False)
    common.check_error(response, common.MtbDaemonError.MODULE_INVALID_ADDR)


def validate_module(server: Dict[str, Any], jsonAddr: int,
                    json: Dict[str, Any], state: str) -> None:
    # Validaes general section of the module only
    # E.g. NOT 'MTB-UNI v4'
    assert 'address' in server
    assert server['address'] == jsonAddr

    assert 'name' in server
    assert server['name'] == json['name']

    assert 'type_code' in server
    assert server['type_code'] == json['type']

    assert 'type' in server
    assert isinstance(server['type'], str)

    assert 'state' in server
    assert server['state'] == state

    for key in ['firmware_version', 'protocol_version', 'bootloader_version']:
        if state == 'active':
            assert key in server
            common.check_version_format(server[key])
        else:
            assert key not in server

    for key in ['error', 'warning', 'beacon', 'fw_deprecated']:
        if state == 'active':
            assert key in server
            assert isinstance(server[key], bool)
            assert not server[key]
        else:
            assert key not in server


def validate_module_common(server: Dict[str, Any], jsonAddr: int, state: str) -> None:
    validate_module(server, jsonAddr, common.MODULES_JSON[jsonAddr], state)


def test_active_module() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.TEST_MODULE_ADDR}
    )
    assert 'module' in response
    module = response['module']
    assert isinstance(module, dict)
    validate_module_common(module, common.TEST_MODULE_ADDR, 'active')


def test_inactive_module() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.INACTIVE_MODULE_ADDR}
    )
    assert 'module' in response
    module = response['module']
    assert isinstance(module, dict)
    validate_module_common(module, common.INACTIVE_MODULE_ADDR, 'inactive')


def test_module_invalid_addr() -> None:
    common.check_invalid_addresses({'command': 'module'}, 'address')


def test_unknown_module() -> None:
    response = mtb_daemon.request_response({'command': 'module', 'address': 42},
                                           timeout=1, ok=False)
    common.check_error(response, common.MtbDaemonError.MODULE_INVALID_ADDR)


def test_modules() -> None:
    response = mtb_daemon.request_response({'command': 'modules'})
    assert 'modules' in response
    modules = response['modules']
    assert isinstance(modules, dict)
    assert list(modules.keys()) == [str(common.TEST_MODULE_ADDR), str(common.INACTIVE_MODULE_ADDR)]
    validate_module_common(modules[str(common.TEST_MODULE_ADDR)], common.TEST_MODULE_ADDR,
                           'active')
    validate_module_common(modules[str(common.INACTIVE_MODULE_ADDR)],
                           common.INACTIVE_MODULE_ADDR, 'inactive')


def test_unable_to_delete_active_module() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module_delete', 'address': common.TEST_MODULE_ADDR},
        ok=False
    )
    common.check_error(response, common.MtbDaemonError.MODULE_ACTIVE)


###############################################################################
# Delete

def test_delete() -> None:
    """
    Warning: this test changes state: it deletes INACTIVE_MODULE_ADDR module.
    If previous tests in this file would run after this test, they would fail.
    To restore state, run `test_create` or restart mtb-daemon.
    """
    response = mtb_daemon.request_response({'command': 'modules'})
    assert list(response['modules'].keys()) == \
        [str(common.TEST_MODULE_ADDR), str(common.INACTIVE_MODULE_ADDR)]

    mtb_daemon.request_response(
        {'command': 'module_delete', 'address': common.INACTIVE_MODULE_ADDR}
    )

    response = mtb_daemon.request_response({'command': 'modules'})
    assert list(response['modules'].keys()) == [str(common.TEST_MODULE_ADDR)]

    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.INACTIVE_MODULE_ADDR},
        timeout=1,
        ok=False
    )
    common.check_error(response, common.MtbDaemonError.MODULE_INVALID_ADDR)


def test_delete_invalid_addr() -> None:
    common.check_invalid_addresses({'command': 'module_delete'}, 'address')


###############################################################################
# Create

def test_create() -> None:
    """Create deleted module by `test_create` (INACTIVE_MODULE_ADDR)."""
    response = mtb_daemon.request_response({'command': 'modules'})
    assert list(response['modules'].keys()) == [str(common.TEST_MODULE_ADDR)]

    module_json = common.MODULES_JSON[common.INACTIVE_MODULE_ADDR]
    mtb_daemon.request_response({
        'command': 'module_set_config',
        'address': common.INACTIVE_MODULE_ADDR,
        'type_code': module_json['type'],
        'name': module_json['name'],
        # omitting 'config' - default config used
    })

    response = mtb_daemon.request_response({'command': 'modules'})
    assert list(response['modules'].keys()) == \
        [str(common.TEST_MODULE_ADDR), str(common.INACTIVE_MODULE_ADDR)]


def test_create_invalid_addr() -> None:
    common.check_invalid_addresses({'command': 'module_set_config'}, 'address')


###############################################################################
# Reboot

def test_reboot() -> None:
    mtb_daemon.send_request({'command': 'module_reboot', 'address': common.TEST_MODULE_ADDR})
    response = mtb_daemon.request_response({
        'command': 'module', 'address': common.TEST_MODULE_ADDR
    })
    assert response['module']['state'] == 'rebooting'

    time.sleep(3)
    mtb_daemon.expect_response('module_reboot')

    response = mtb_daemon.request_response({
        'command': 'module', 'address': common.TEST_MODULE_ADDR
    })
    assert response['module']['state'] == 'active'


def test_reboot_invalid_addr() -> None:
    common.check_invalid_addresses({'command': 'module_reboot'}, 'address')


###############################################################################
# Beacon

def test_beacon() -> None:
    mtb_daemon.request_response({
        'command': 'module_beacon', 'address': common.TEST_MODULE_ADDR, 'beacon': True
    })

    response = mtb_daemon.request_response({
        'command': 'module', 'address': common.TEST_MODULE_ADDR
    })
    assert response['module']['beacon']

    mtb_daemon.request_response({
        'command': 'module_beacon', 'address': common.TEST_MODULE_ADDR, 'beacon': False
    })

    response = mtb_daemon.request_response({
        'command': 'module', 'address': common.TEST_MODULE_ADDR
    })
    assert not response['module']['beacon']


def test_beacon_invalid_addr() -> None:
    common.check_invalid_addresses({'command': 'module_beacon'}, 'address')


###############################################################################
# Module-specific command

def test_module_specific_broadcast() -> None:
    mtb_daemon.request_response({
        'command': 'module_specific_command',
        'data': [0xBE, 0xEF],
    })


def test_module_specific_command() -> None:
    # Just check for error response (no module-specific command implemneted yet)
    response = mtb_daemon.request_response(
        {
            'command': 'module_specific_command',
            'address': common.TEST_MODULE_ADDR,
            'data': [1, 2, 3, 4],
        },
        ok=False
    )

    common.check_error(response, common.MtbDaemonError.MODULE_UNKNOWN_COMMAND)


###############################################################################
# Set address

def test_module_set_address() -> None:
    response = mtb_daemon.request_response(
        {
            'command': 'module_set_address',
            'address': common.TEST_MODULE_ADDR,
            'new_address': 42,
        },
        ok=False
    )

    common.check_error(response, common.MtbDaemonError.MODULE_UNSUPPORTED_COMMAND)


def test_set_address() -> None:
    mtb_daemon.request_response(
        {
            'command': 'set_address',
            'new_address': 42,
        }
    )


###############################################################################
# Common

def test_disconnect_while_operation_pending() -> None:
    # Test that mtb-daemon does not crash when it wants to send response to the
    # non-existing socket.
    with MtbDaemonIFace() as second_daemon:
        second_daemon.send_request(
            {'command': 'module_reboot', 'address': common.TEST_MODULE_ADDR}
        )

    time.sleep(2.5)  # just to finish module rebooting

    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.TEST_MODULE_ADDR}
    )
    assert response['module']['state'] == 'active'
