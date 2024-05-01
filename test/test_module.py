"""
Test general module endpoints of MTB Daemon TCP server using PyTest.
"""

from typing import Dict, Any

import common
import mtbdaemonif

mtb_daemon = mtbdaemonif.MtbDaemonIFace()


def test_endpointis_present() -> None:
    mtb_daemon.request_response({'command': 'modules'})
    mtb_daemon.request_response({'command': 'module', 'address': common.TEST_MODULE_ADDR})
    response = mtb_daemon.request_response({'command': 'module'}, timeout=1, ok=False)
    common.check_error(response, common.MtbDaemonError.MODULE_INVALID_ADDR)


def validate_test_module(module: Dict[str, Any]) -> None:
    # Validaes general section of the module only
    # E.g. NOT 'MTB-UNI v4'
    assert 'address' in module
    assert module['address'] == common.TEST_MODULE_ADDR

    assert 'name' in module
    assert isinstance(module['name'], str)


def test_module() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.TEST_MODULE_ADDR}
    )
    assert 'module' in response
    module = response['module']
    assert isinstance(module, dict)
    validate_test_module(module)


def test_modules() -> None:
    response = mtb_daemon.request_response({'command': 'modules'})
    assert 'modules' in response
    modules = response['modules']
    assert isinstance(modules, dict)
    assert list(modules.keys()) == [str(common.TEST_MODULE_ADDR)]
    validate_test_module(modules[str(common.TEST_MODULE_ADDR)])
