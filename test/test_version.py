"""
Test 'version' endpoint of MTB Daemon TCP server using PyTest.
"""

import common
import mtbdaemonif

mtb_daemon = mtbdaemonif.MtbDaemonIFace()


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
