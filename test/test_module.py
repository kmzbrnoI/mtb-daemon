"""
Test general module endpoints of MTB Daemon TCP server using PyTest.
"""

from typing import Dict, Any

import common
import mtbdaemonif

mtb_daemon = mtbdaemonif.MtbDaemonIFace()


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

    for key in ['error', 'warning']:
        if state == 'active':
            assert key in server
            assert isinstance(server[key], bool)
            assert not server[key]
        else:
            assert key not in server

    # 'beacon' present even in inactive module (TODO is it ok?)
    assert 'beacon' in server
    assert isinstance(server['beacon'], bool)
    assert not server['beacon']


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
