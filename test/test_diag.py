"""
Test diagnostics of MTB Daemon TCP server using PyTest.
"""

from typing import Dict, Any

import common
from mtbdaemonif import mtb_daemon


def check_dv_1(response: Dict[str, Any], address: int) -> None:
    assert 'address' in response
    assert response['address'] == address

    assert 'DVkey' in response
    assert response['DVkey'] == 'state'

    assert 'DVvalue' in response
    assert response['DVvalue'] == {'errors': False, 'warnings': False}

    assert 'DVvalueRaw' in response
    assert response['DVvalueRaw'] == [0]


def test_dvnum_1() -> None:
    response = mtb_daemon.request_response({
        'command': 'module_diag',
        'address': common.TEST_MODULE_ADDR,
        'DVnum': 1,
    })
    check_dv_1(response, common.TEST_MODULE_ADDR)


def test_dvnum_missing() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module_diag', 'address': common.TEST_MODULE_ADDR},
        ok=False
    )
    common.check_error(response, common.MtbDaemonError.INVALID_DV)


def test_dvnum_invalid_addrs() -> None:
    common.check_invalid_addresses({'command': 'module_diag', 'DVnum': 1}, 'address')


def test_no_dv_1_inactive_module() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module_diag', 'address': common.INACTIVE_MODULE_ADDR, 'DVnum': 1},
        ok=False
    )
    common.check_error(response, common.MtbDaemonError.BUS_NO_RESPONSE)


def test_dvkey_state() -> None:
    response = mtb_daemon.request_response({
        'command': 'module_diag',
        'address': common.TEST_MODULE_ADDR,
        'DVkey': 'state',
    })
    check_dv_1(response, common.TEST_MODULE_ADDR)


def test_unknown_dvkey() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module_diag', 'address': common.TEST_MODULE_ADDR, 'DVkey': 'blah'},
        ok=False
    )
    common.check_error(response, common.MtbDaemonError.INVALID_DV)


def test_dvkey_from_other_module() -> None:
    response = mtb_daemon.request_response(
        {
            'command': 'module_diag',
            'address': common.TEST_MODULE_ADDR,
            'DVkey': 'cutouts_started',
        },
        ok=False
    )
    common.check_error(response, common.MtbDaemonError.INVALID_DV)
