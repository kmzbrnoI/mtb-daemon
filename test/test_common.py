"""
Test common behavior of MTB Daemon TCP server using PyTest.
"""

import common
from mtbdaemonif import mtb_daemon


def test_endpoint_present() -> None:
    mtb_daemon.request_response({'command': 'version'})
    mtb_daemon.request_response({'command': 'version'})  # with different id


def test_version_response() -> None:
    response = mtb_daemon.request_response({'command': 'version'})
    assert 'version' in response
    assert isinstance(response['version'], dict)
    version = response['version']

    assert 'sw_version' in version
    common.check_version_format(version['sw_version'])

    assert 'sw_version_major' in version
    assert isinstance(version['sw_version_major'], int)

    assert 'sw_version_minor' in version
    assert isinstance(version['sw_version_minor'], int)


def test_unknown_command() -> None:
    response = mtb_daemon.request_response(
        {'command': 'nonexisting_command'},
        ok=False
    )
    common.check_error(response, common.MtbDaemonError.UNKNOWN_COMMAND)


def test_unknown_command_module_prefix() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module_nonexisting_command', 'address': common.TEST_MODULE_ADDR},
        ok=False
    )
    common.check_error(response, common.MtbDaemonError.UNKNOWN_COMMAND)
