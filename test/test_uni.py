"""
Test MTB-UNI module in MTB Daemon TCP server using PyTest.
"""

from typing import Dict, Any

import common
import mtbdaemonif

mtb_daemon = mtbdaemonif.MtbDaemonIFace()


def validate_uni_data(server: Dict[str, Any], jsonAddr: int,
                      json: Dict[str, Any]) -> None:
    # Validaes 'MTB-UNI v4' section in 'module'
    assert not server.get('ir', False)  # 'ir' could be in the data, but must be 'False'

    assert 'config' in server
    assert 'config' in json

    assert 'outputsSafe' in server['config']
    assert server['config']['outputsSafe'] == json['config']['outputsSafe']
    assert server['config']['inputsDelay'] == json['config']['inputsDelay']
    assert 'irs' not in server['config']


def validate_uni_data_common(server: Dict[str, Any], jsonAddr: int) -> None:
    validate_uni_data(server, jsonAddr, common.MODULES_JSON[jsonAddr])


def test_data_uni() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.TEST_MODULE_ADDR}
    )
    assert 'module' in response
    assert 'MTB-UNI v4' in response['module']
    uni = response['module']['MTB-UNI v4']
    assert isinstance(uni, dict)
    validate_uni_data_common(uni, common.TEST_MODULE_ADDR)
    assert 'state' not in uni


def validate_uni_state(state: Dict[str, Any]) -> None:
    assert 'outputs' in state
    assert isinstance(state['outputs'], dict)
    assert len(state['outputs']) == 16
    assert sorted([int(addr) for addr in state['outputs'].keys()]) == [i for i in range(16)]
    for i, output in state['outputs'].items():
        assert output == {'type': 'plain', 'value': 0}

    assert 'inputs' in state
    assert isinstance(state['inputs'], dict)
    assert set(state['inputs'].keys()) == set(['packed', 'full'])
    assert state['inputs']['packed'] == 0
    assert state['inputs']['full'] == [False for _ in range(16)]


def test_state_uni() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.TEST_MODULE_ADDR, 'state': True}
    )
    uni = response['module']['MTB-UNI v4']
    assert 'state' in uni
    assert isinstance(uni['state'], dict)
    validate_uni_state(uni['state'])


def test_state_not_in_inactive_uni() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.INACTIVE_MODULE_ADDR, 'state': True}
    )
    uni = response['module']['MTB-UNI v4']
    assert 'state' not in uni
