"""
Test MTB-UNI module in MTB Daemon TCP server using PyTest.
"""

from typing import Dict, Any

import common
from mtbdaemonif import mtb_daemon
import time


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


def validate_uni_state(state: Dict[str, Any], outputs_state: int) -> None:
    assert 'outputs' in state
    assert isinstance(state['outputs'], dict)
    assert len(state['outputs']) == 16
    assert sorted([int(addr) for addr in state['outputs'].keys()]) == [i for i in range(16)]
    for outistr, output in state['outputs'].items():
        outi = int(outistr)
        active = bool((outputs_state >> outi) & 1)
        assert output == {'type': 'plain', 'value': (1 if active else 0)}, \
            f'Output {outistr} mismatch!'

    assert 'inputs' in state
    assert isinstance(state['inputs'], dict)
    assert set(state['inputs'].keys()) == set(['packed', 'full'])
    # Only outputs 0 is connected to input 0, rest unconnected
    assert state['inputs']['packed'] == (1 if (outputs_state & 1) > 0 else 0)
    assert state['inputs']['full'] == [(outputs_state & 1) > 0] + [False for _ in range(15)]


def check_uni_state(addr: int, outputs: int) -> None:
    response = mtb_daemon.request_response(
        {'command': 'module', 'address': addr, 'state': True}
    )
    validate_uni_state(response['module']['MTB-UNI v4']['state'], outputs)


def test_state_uni() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.TEST_MODULE_ADDR, 'state': True}
    )
    uni = response['module']['MTB-UNI v4']
    assert 'state' in uni
    assert isinstance(uni['state'], dict)
    validate_uni_state(uni['state'], 0)


def test_state_not_in_inactive_uni() -> None:
    response = mtb_daemon.request_response(
        {'command': 'module', 'address': common.INACTIVE_MODULE_ADDR, 'state': True}
    )
    uni = response['module']['MTB-UNI v4']
    assert 'state' not in uni


def set_uni_outputs_and_validate(addr: int, outputs: Dict[str, Any]) -> None:
    response = mtb_daemon.request_response({
        'command': 'module_set_outputs',
        'address': addr,
        'outputs': outputs,
    })

    assert 'outputs' in response
    for outputstri, output in outputs.items():
        assert outputstri in response['outputs']
        assert response['outputs'][outputstri] == output

    time.sleep(0.1)  # to propagate UNI's output to its input

    binoutputs: int = 0
    for strouti, output in outputs.items():
        assert output['type'] == 'plain'
        assert output['value'] in [0, 1]
        if output['value'] == 1:
            binoutputs |= (1 << int(strouti))

    check_uni_state(addr, binoutputs)


def reset_uni_outputs_and_validate(addr: int) -> None:
    set_uni_outputs_and_validate(
        addr,
        {str(i): {'type': 'plain', 'value': 0} for i in range(16)}
    )


def test_plain_set_output_and_feedback() -> None:
    """
    In case of test failure, the output is reset, because this test disconnects from MTB Dameon.
    """
    set_uni_outputs_and_validate(common.TEST_MODULE_ADDR, {'0': {'type': 'plain', 'value': 1}})
    reset_uni_outputs_and_validate(common.TEST_MODULE_ADDR)


def test_set_output_no_feedback() -> None:
    set_uni_outputs_and_validate(common.TEST_MODULE_ADDR, {'1': {'type': 'plain', 'value': 1}})
    set_uni_outputs_and_validate(
        common.TEST_MODULE_ADDR,
        {'1': {'type': 'plain', 'value': 1}, '15': {'type': 'plain', 'value': 1}}
    )
    set_uni_outputs_and_validate(
        common.TEST_MODULE_ADDR,
        {'1': {'type': 'plain', 'value': 0}, '15': {'type': 'plain', 'value': 1}}
    )
    reset_uni_outputs_and_validate(common.TEST_MODULE_ADDR)


def test_set_outputs_sequentially() -> None:
    for i in range(16):
        common.set_single_output(common.TEST_MODULE_ADDR, i, 1)

    time.sleep(0.1)

    check_uni_state(common.TEST_MODULE_ADDR, 0xFFFF)
    reset_uni_outputs_and_validate(common.TEST_MODULE_ADDR)


def test_set_outputs_sequentially_with_check() -> None:
    for i in range(16):
        set_uni_outputs_and_validate(
            common.TEST_MODULE_ADDR,
            {str(j): {'type': 'plain', 'value': 1} for j in range(i)}
        )
    reset_uni_outputs_and_validate(common.TEST_MODULE_ADDR)


def test_set_output_missing_address() -> None:
    response = mtb_daemon.request_response({'command': 'module_set_outputs'}, ok=False)
    common.check_error(response, common.MtbDaemonError.MODULE_INVALID_ADDR)


def test_set_output_invalid_port() -> None:
    for port in ['16', 'wtf', '-1', '0x02']:
        response = mtb_daemon.request_response(
            {
                'command': 'module_set_outputs',
                'address': common.TEST_MODULE_ADDR,
                'outputs': {port: {'type': 'plain', 'value': 1}},
            },
            ok=False
        )
        common.check_error(response, common.MtbDaemonError.MODULE_INVALID_PORT)


def test_set_output_empty() -> None:
    # Missing 'outputs' -> should do nothing
    mtb_daemon.request_response(
        {'command': 'module_set_outputs', 'address': common.TEST_MODULE_ADDR},
    )
    check_uni_state(common.TEST_MODULE_ADDR, 0)


def test_set_output_of_inactive_module() -> None:
    response = mtb_daemon.request_response(
        {
            'command': 'module_set_outputs',
            'address': common.INACTIVE_MODULE_ADDR,
            'outputs': {'0': {'type': 'plain', 'value': 1}},
        },
        ok=False
    )
    common.check_error(response, common.MtbDaemonError.MODULE_FAILED)


###############################################################################


def test_reset_my_outputs() -> None:
    set_uni_outputs_and_validate(common.TEST_MODULE_ADDR, {'1': {'type': 'plain', 'value': 1}})
    mtb_daemon.request_response({'command': 'reset_my_outputs'})
    time.sleep(0.1)
    check_uni_state(common.TEST_MODULE_ADDR, 0)


def test_reset_outputs_on_disconnect() -> None:
    set_uni_outputs_and_validate(common.TEST_MODULE_ADDR, {'1': {'type': 'plain', 'value': 1}})
    mtb_daemon.disconnect()
    time.sleep(0.1)
    mtb_daemon.connect()
    check_uni_state(common.TEST_MODULE_ADDR, 0)


###############################################################################

def check_set_name(addr: int) -> None:
    TMPNAME = 'placeholder'

    response = mtb_daemon.request_response({
        'command': 'module_set_config',
        'address': addr,
        'name': TMPNAME,
    })

    assert 'address' in response
    assert response['address'] == addr

    response = mtb_daemon.request_response({'command': 'module', 'address': addr})
    assert response['module']['name'] == TMPNAME

    # Revert changes
    original = common.MODULES_JSON[addr]['name']
    response = mtb_daemon.request_response({
        'command': 'module_set_config',
        'address': addr,
        'name': original,
    })

    assert 'address' in response
    assert response['address'] == addr

    response = mtb_daemon.request_response({'command': 'module', 'address': addr})
    assert response['module']['name'] == original


def test_set_name_inactive() -> None:
    check_set_name(common.INACTIVE_MODULE_ADDR)


def test_set_name_active() -> None:
    check_set_name(common.TEST_MODULE_ADDR)


def set_and_check_input_delay(addr: int, delay: float) -> None:
    # First set config
    mtb_daemon.request_response({
        'command': 'module_set_config',
        'address': addr,
        'config': {'inputsDelay': [delay] + [0]*15},
    })

    # Activate output
    common.set_single_output(addr, 0, 1)

    # Wait for propagation to input
    time.sleep(0.1)

    # Deactivate output
    common.set_single_output(addr, 0, 0)

    # After delay/2, input should still be high
    time.sleep(delay/2)

    if delay > 0:
        response = mtb_daemon.request_response({
            'command': 'module',
            'address': addr,
            'state': True,
        })
        assert response['module']['MTB-UNI v4']['state']['inputs']['packed'] == 1

    # After delay/2 + some tolerance, input should fall
    time.sleep((delay/2) + 0.1)

    response = mtb_daemon.request_response({
        'command': 'module',
        'address': addr,
        'state': True,
    })
    assert response['module']['MTB-UNI v4']['state']['inputs']['packed'] == 0


def test_set_inputs_delay_active() -> None:
    set_and_check_input_delay(common.TEST_MODULE_ADDR, 1)
    set_and_check_input_delay(common.TEST_MODULE_ADDR, 0)  # Revert


def test_set_inputs_delay_inactive() -> None:
    delays = [1] + [0]*15
    mtb_daemon.request_response({
        'command': 'module_set_config',
        'address': common.INACTIVE_MODULE_ADDR,
        'config': {'inputsDelay': delays},
    })

    response = mtb_daemon.request_response({
        'command': 'module',
        'address': common.INACTIVE_MODULE_ADDR,
    })
    assert response['module']['MTB-UNI v4']['config']['inputsDelay'] == delays

    # Revert
    mtb_daemon.request_response({
        'command': 'module_set_config',
        'address': common.INACTIVE_MODULE_ADDR,
        'config': {'inputsDelay': [0]*16},
    })

    response = mtb_daemon.request_response({
        'command': 'module',
        'address': common.INACTIVE_MODULE_ADDR,
    })
    assert response['module']['MTB-UNI v4']['config']['inputsDelay'] == [0]*16


def set_output_0_safe_and_check(addr: int, value: int) -> None:
    mtb_daemon.request_response({
        'command': 'module_set_config',
        'address': addr,
        'config': {'outputsSafe': [{'type': 'plain', 'value': value}]},
    })

    time.sleep(0.5)  # TODO: this is required; is it an issue?
    mtb_daemon.request_response({'command': 'module_reboot', 'address': addr}, timeout=3)
    time.sleep(1)

    response = mtb_daemon.request_response({
        'command': 'module',
        'address': addr,
        'state': True,
    })
    assert response['module']['MTB-UNI v4']['state']['inputs']['packed'] == value


def test_set_outputs_safe() -> None:
    set_output_0_safe_and_check(common.TEST_MODULE_ADDR, 1)
    set_output_0_safe_and_check(common.TEST_MODULE_ADDR, 0)  # Revert
