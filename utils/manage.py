#!/usr/bin/env python3

"""
MTB Daemon Command Line Utility

This utility connects to MTB Daemon server and allows user to interact with
the server, e.g. configure modules, read inputs, set outputs, upgrade firmware
etc. It provides nice high-level command-line interface to interact with
MTB Daemon.

Usage:
  manage.py [options] save_config
  manage.py [options] load_config
  manage.py [options] mtbusb
  manage.py [options] mtbusb speed <speed>
  manage.py [options] version
  manage.py [options] module <module_addr> [--diag]
  manage.py [options] delete <module_addr>
  manage.py [options] inputs <module_addr>
  manage.py [options] outputs <module_addr>
  manage.py [options] reboot <module_addr>
  manage.py [options] monitor <module_addr>
  manage.py [options] beacon <module_addr> <state>
  manage.py [options] fw_upgrade <module_addr> <hexfilename>
  manage.py [options] ir <module_addr> (yes|no|auto)
  manage.py [options] set_output <module_addr> <port> <type> <value>
  manage.py [options] config <module_addr> ports
  manage.py [options] config <module_addr> ports <ports_range> (plaini|plaino|s-com|ir) [<delay>]
  manage.py [options] config <module_addr> name [<module_name>]
  manage.py [options] set_addr <new_address>
  manage.py [options] change_addr <module_addr> <new_address>
  manage.py [options] dvnum <module_addr> <dvnum>
  manage.py [options] dvstr <module_addr> <dvstr>
  manage.py [options] specific <module_addr> <command>
  manage.py [options] specific <command>
  manage.py --help

Options:
  -s <servername>    Specify MTB Daemon server address [default: localhost]
  -p <port>          Specify MTB Daemon port [default: 3841]
  -v                 Verbose
  -h --help          Show this screen.
"""

import socket
import sys
import json
from docopt import docopt
from typing import Dict, Any, Optional, List
import datetime


class EDaemonResponse(Exception):
    pass


def request_response(socket: socket.socket, verbose: bool,
                     request: Dict[str, Any]) -> Dict[str, Any]:
    to_send = request
    to_send['type'] = 'request'
    if verbose:
        print(to_send)
    socket.send((json.dumps(to_send)+'\n').encode('utf-8'))

    while True:
        data = socket.recv(0xFFFF).decode('utf-8').strip()
        messages = [json.loads(msg) for msg in data.split('\n')]
        for message in messages:
            assert isinstance(message, dict)
            if verbose:
                print(message)
            if message.get('command', '') == request['command']:
                if message.get('status', '') != 'ok':
                    raise EDaemonResponse(
                        message.get('error', {}).get('message', 'Uknown error!')
                    )
                return message


def fw_upgrade(socket: socket.socket, module_addr: int, hexfilename: str,
               verbose: bool) -> None:
    firmware = {}
    offset = 0
    with open(hexfilename, 'r') as file:
        for line in file:
            line = line.strip()
            assert line.startswith(':')

            type_ = int(line[7:9], 16)
            addr = offset+int(line[3:7], 16)

            if type_ == 2:
                offset = int(line[9:13], base=16)*16

            if type_ == 0:
                firmware[addr] = line[9:-2]

    request_response(socket, verbose, {
        'command': 'module_upgrade_fw',
        'address': module_addr,
        'firmware': firmware,
    })


def mtbusb(sock: socket.socket, verbose: bool) -> None:
    response = request_response(sock, verbose, {'command': 'mtbusb'})
    for key, val in response['mtbusb'].items():
        print(key, ':', val)


def mtbusb_speed(sock: socket.socket, verbose: bool, speed: int) -> None:
    response = request_response(sock, verbose, {
        'command': 'mtbusb',
        'mtbusb': {'speed': speed},
    })
    for key, val in response['mtbusb'].items():
        print(key, ':', val)


def version(sock: socket.socket, verbose: bool) -> None:
    response = request_response(sock, verbose, {'command': 'version'})
    print(response['version'])


def module(sock: socket.socket, verbose: bool, module: int) -> None:
    response = request_response(sock, verbose, {
        'command': 'module',
        'address': module,
    })
    type_ = response['module']['type']
    for key, val in response['module'].items():
        if (key == type_ and 'config' in val and type_.startswith('MTB-UNI ')):
            print('Config:')
            uni_print_config(val['config'])
            val.pop('config')
        if (key == type_ and 'config' in val and type_.startswith('MTB-UNIS ')):
            print('Config:')
            unis_print_config(val['config'])
            val.pop('config')
        print(key, ':', val)


def delete(sock: socket.socket, verbose: bool, module: int) -> None:
    response = request_response(sock, verbose, {
        'command': 'module_delete',
        'address': module,
    })


def module_diag(sock: socket.socket, verbose: bool, module: int) -> None:
    DVS = ['warnings', 'errors', 'uptime', 'mcu_voltage', 'mcu_temperature']

    for dvkey in DVS:
        response = request_response(sock, verbose, {
            'command': 'module_diag',
            'address': module,
            'DVkey': dvkey,
        })
        for key, val in response.get('DVvalue', {}).items():
            print(dv_str(key, val))


def uni_print_config(config: Dict[str, Any]) -> None:
    inputs = ['Inputs:']
    for i, delay in enumerate(config['inputsDelay']):
        ir = config['irs'][i] if 'irs' in config else False
        irstr = 'IR' if ir else ''
        inputs.append(f'{i}: {delay} {irstr}')

    outputs = ['Outputs:']
    for i, d in enumerate(config['outputsSafe']):
        outputs.append(f'{i}: {d["type"]} {d["value"]}')

    for inp, out in zip(inputs, outputs):
        print(inp.ljust(20), out)


def unis_print_config(config: Dict[str, Any]) -> None:
    inputs = ['Inputs:']
    for i, delay in enumerate(config['inputsDelay']):
        inputs.append(f'{i}: {delay}')

    outputs = ['Outputs:']
    for i, d in enumerate(config['outputsSafe']):
        outputs.append(f'{i}: {d["type"]} {d["value"]}')

    for inp, out in zip(inputs, outputs):
        print(inp.ljust(20), out)

def unis_inputs_str(inputs: Dict[str, Any]) -> str:
    return ((''.join(str(int(val)) for val in inputs['full'][:8])) + ' ' +
            (''.join(str(int(val)) for val in inputs['full'][8:16])) + ' ' +
	    (''.join(str(int(val)) for val in inputs['full'][16:18])) + ' ' +
	    (''.join(str(int(val)) for val in inputs['full'][18:20])) + ' ' +
	    (''.join(str(int(val)) for val in inputs['full'][20:22])) + ' ' +
	    (''.join(str(int(val)) for val in inputs['full'][22:24])) + ' ' +
	    (''.join(str(int(val)) for val in inputs['full'][24:26])) + ' ' +
	    (''.join(str(int(val)) for val in inputs['full'][26:28])))


def uni_inputs_str(inputs: Dict[str, Any]) -> str:
    return ((''.join(str(int(val)) for val in inputs['full'][:8])) + ' ' +
            (''.join(str(int(val)) for val in inputs['full'][8:])))


def rc_inputs_str(inputs: Dict[str, Any]) -> str:
    return ', '.join(f'{i}: {addrs}' for i, addrs in enumerate(inputs['ports']))


def inputs_str(module_type: str, inputs: Dict[str, Any]) -> str:
    if module_type.startswith('MTB-UNIS'):
        return unis_inputs_str(inputs)
    elif module_type == 'MTB-RC':
        return uni_inputs_str(inputs)
    elif module_type == 'MTB-RC':
        return rc_inputs_str(inputs)
    else:
        return 'Unknown module type: {module_["type"]}'


def uni_outputs_str(outputs: Dict[str, Any]) -> str:
    result = ''
    sorted_ = sorted(outputs.items(), key=lambda kv: int(kv[0]))
    for i, (port, value) in enumerate(sorted_):
        if value['type'] == 'plain' or value['value'] == 0:
            result += str(value['value'])
        else:
            result += value['type'][0]
        if i == 7:
            result += ' '
    return result


def unis_outputs_str(outputs: Dict[str, Any]) -> str:
    result = ''
    sorted_ = sorted(outputs.items(), key=lambda kv: int(kv[0]))
    for i, (port, value) in enumerate(sorted_):
        if value['type'] == 'plain' or value['value'] == 0:
            result += str(value['value'])
        else:
            result += value['type'][0]
        if i == 7:
            result += ' '
        if (i > 14) and (i & 1):
            result += ' '
    return result


def get_inputs(sock: socket.socket, verbose: bool, module: int) -> None:
    response = request_response(sock, verbose, {
        'command': 'module',
        'address': module,
        'state': True,
    })
    module_ = response['module']
    module_spec = module_[module_['type']]
    if 'state' not in module_spec:
        raise EDaemonResponse(
            'No state received - is module active? Is it in bootloader?'
        )
    inputs = module_spec['state']['inputs']
    print(inputs_str(module_['type'], inputs))


def get_outputs(sock: socket.socket, verbose: bool, module: int) -> None:
    response = request_response(sock, verbose, {
        'command': 'module',
        'address': module,
        'state': True,
    })
    module_ = response['module']
    module_spec = module_[module_['type']]
    if 'state' not in module_spec:
        raise EDaemonResponse(
            'No state received - is module active? Is it in bootloader?'
        )
    outputs = module_spec['state']['outputs']
    if module_['type'] == 'MTB-UNIS':
        print(unis_outputs_str(outputs))
    elif module_['type'].startswith('MTB-UNI'):
        print(uni_outputs_str(outputs))
    else:
        print(f'Unknown module type: {module_["type"]}')


def reboot(sock: socket.socket, verbose: bool, module_: int) -> None:
    request_response(sock, verbose, {
        'command': 'module_reboot',
        'address': module_,
    })
    module(sock, verbose, module_)


def monitor(sock: socket.socket, verbose: bool, module: int) -> None:
    request_response(sock, verbose, {
        'command': 'module_subscribe',
        'addresses': [module],
    })
    print('['+str(datetime.datetime.now().time())+']', end=' ')
    get_inputs(sock, verbose, module)

    while True:
        data = sock.recv(0xFFFF).decode('utf-8').strip()
        messages = [json.loads(msg) for msg in data.split('\n')]
        for message in messages:
            if verbose:
                print('['+str(datetime.datetime.now().time())+']', end=' ')
                print(message)
            command = message.get('command', '')

            if command == 'module_inputs_changed':
                mic = message['module_inputs_changed']
                print('['+str(datetime.datetime.now().time())+']', end=' ')
                print(inputs_str(mic['type'], mic['inputs']))

            if command == 'module_outputs_changed':
                print('['+str(datetime.datetime.now().time())+'] Outputs:', end=' ')
                print(uni_outputs_str(message['module_outputs_changed']['outputs']))

            if command == 'module' and int(message['module']['address']) == module:
                print('['+str(datetime.datetime.now().time())+']', end=' ')
                print(f'state : {message["module"]["state"]}')
                type_ = message['module']['type']
                if 'state' in message['module'][type_]:
                    state = message['module'][type_]['state']
                    print('['+str(datetime.datetime.now().time())+']', end=' ')
                    print(inputs_str(type_, state['inputs']))

            if command == 'mtbusb':
                print('['+str(datetime.datetime.now().time())+']', end=' ')
                print(f'MTB-USB: {message["mtbusb"]}')


def beacon(sock: socket.socket, verbose: bool, module_: int, beacon: bool) -> None:
    request_response(sock, verbose, {
        'command': 'module_beacon',
        'address': module_,
        'beacon': beacon,
    })


def ir(sock: socket.socket, verbose: bool, module_: int, type_: str) -> None:
    data = []
    if type_ == 'auto':
        data = [0x01, 0xFF]
    elif type_ == 'yes':
        data = [0x01, 0x01]
    elif type_ == 'no':
        data = [0x01, 0x00]

    request_response(sock, verbose, {
        'command': 'module_specific_command',
        'address': module_,
        'data': data,
    })


def set_output(sock: socket.socket, verbose: bool, module: int, port: int,
               type_: str, value: int) -> None:
    request_response(sock, verbose, {
        'command': 'module_set_outputs',
        'address': module,
        'outputs': {
            port: {'type': type_, 'value': value},
        },
    })

    # Wait because disconnect causes output reset
    while True:
        sock.recv(0xFFFF).decode('utf-8').strip()


def save_config(sock: socket.socket, verbose: bool) -> None:
    request_response(sock, verbose, {
        'command': 'save_config',
    })


def load_config(sock: socket.socket, verbose: bool) -> None:
    request_response(sock, verbose, {
        'command': 'load_config',
    })


def set_address(sock: socket.socket, verbose: bool, newaddr: int) -> None:
    request_response(sock, verbose, {
        'command': 'set_address',
        'new_address': newaddr,
    })


def change_address(sock: socket.socket, verbose: bool, module: int, newaddr: int) -> None:
    request_response(sock, verbose, {
        'command': 'module_set_address',
        'new_address': newaddr,
        'address': module,
    })


def module_config_ports(sock: socket.socket, verbose: bool, module: int, rg: range, io_type: str,
                        delay: Optional[float]) -> None:
    response = request_response(sock, verbose, {
        'command': 'module',
        'address': module,
    })
    type_ = response['module']['type']
    assert type_.startswith('MTB-UNI'), f'Nepodporovaný typ modulu: {type_}!'
    config = response['module'][type_]['config']

    if io_type == 'ir' or io_type == 'plaini':
        for i in rg:
            if io_type == 'ir':
                assert 'irs' in config
                config['irs'][i] = True
            else:
                if 'irs' in config:
                    config['irs'][i] = False
        for i in rg:
            config['inputsDelay'][i] = delay

    if io_type == 's-com' or io_type == 'plaino':
        for i in rg:
            config['outputsSafe'][i]['type'] = io_type[:5]

    request_response(sock, verbose, {
        'command': 'module_set_config',
        'address': module,
        'type_code': response['module']['type_code'],
        'name': response['module']['name'],
        'config': config,
    })

    module_print_config(sock, verbose, module)


def module_print_config(sock: socket.socket, verbose: bool, module: int) -> None:
    response = request_response(sock, verbose, {
        'command': 'module',
        'address': module,
    })
    type_ = response['module']['type']
    config = response['module'][type_]['config']
    print(f'Module {module} – {response["module"]["name"]}:')
    if type_ == 'MTB-UNIS':
        unis_print_config(config)
    elif type_.startswith('MTB-UNI'):
        uni_print_config(config)
    else:
        assert False, f'Nepodporovaný typ modulu: {type_}!'


def module_config_name(sock: socket.socket, verbose: bool, module: int,
                       name: Optional[str]) -> None:
    response = request_response(sock, verbose, {
        'command': 'module',
        'address': module,
    })
    if name is None:
        print(response['module']['name'])
        return
    type_ = response['module']['type']
    config = response['module'][type_]['config']

    request_response(sock, verbose, {
        'command': 'module_set_config',
        'address': module,
        'type_code': response['module']['type_code'],
        'name': name,
        'config': config,
    })


def dv_str(key: str, value: Any) -> str:
    if isinstance(value, float):
        value = round(value, 2)
    return f'{key}: {value}'


def dvnum(sock: socket.socket, verbose: bool, module: int, dvi: int) -> None:
    response = request_response(sock, verbose, {
        'command': 'module_diag',
        'address': module,
        'DVnum': dvi,
    })
    dvvalue = response.get('DVvalue', {})
    dvvalue_raw = response.get('DVvalueRaw', {})
    if not dvvalue:
        print(f'Empty DVvalue, DVvalueRaw={dvvalue_raw}!')
    for key, val in dvvalue.items():
        print(dv_str(key, val))
    if verbose:
        print(f'DVvalueRaw={dvvalue_raw}')


def dvstr(sock: socket.socket, verbose: bool, module: int, dvstr: str) -> None:
    response = request_response(sock, verbose, {
        'command': 'module_diag',
        'address': module,
        'DVkey': dvstr,
    })
    dvvalue = response.get('DVvalue', {})
    dvvalue_raw = response.get('DVvalueRaw', {})
    if not dvvalue:
        print(f'Empty DVvalue, DVvalueRaw={dvvalue_raw}!')
    for key, val in dvvalue.items():
        print(dv_str(key, val))
    if verbose:
        print(f'DVvalueRaw={dvvalue_raw}')


def specific_broadcast(sock: socket.socket, verbose: bool, data: List[int]) -> None:
    response = request_response(sock, verbose, {
        'command': 'module_specific_command',
        'data': data,
    })
    print(response['status'])


def specific_module(sock: socket.socket, verbose: bool, module: int, data: List[int]) -> None:
    response = request_response(sock, verbose, {
        'command': 'module_specific_command',
        'address': module,
        'data': data,
    })
    print(response)


if __name__ == '__main__':
    args = docopt(__doc__)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((args['-s'], int(args['-p'])))

    try:
        if args['fw_upgrade']:
            fw_upgrade(
                sock,
                int(args['<module_addr>']),
                args['<hexfilename>'],
                args['-v'],
            )

        elif args['mtbusb'] and not args['speed']:
            mtbusb(sock, args['-v'])

        elif args['mtbusb'] and args['speed']:
            mtbusb_speed(sock, args['-v'], int(args['<speed>']))

        elif args['version']:
            version(sock, args['-v'])

        elif args['module']:
            module(sock, args['-v'], int(args['<module_addr>']))
            if bool(args['--diag']):
                module_diag(sock, args['-v'], int(args['<module_addr>']))

        elif args['delete']:
            delete(sock, args['-v'], int(args['<module_addr>']))

        elif args['inputs']:
            get_inputs(sock, args['-v'], int(args['<module_addr>']))

        elif args['outputs']:
            get_outputs(sock, args['-v'], int(args['<module_addr>']))

        elif args['reboot']:
            reboot(sock, args['-v'], int(args['<module_addr>']))

        elif args['monitor']:
            monitor(sock, args['-v'], int(args['<module_addr>']))

        elif args['beacon']:
            beacon(sock, args['-v'], int(args['<module_addr>']),
                   bool(int(args['<state>'])))

        elif args['ir'] and not args['config']:
            type_ = ''
            if args['auto']:
                type_ = 'auto'
            elif args['yes']:
                type_ = 'yes'
            elif args['no']:
                type_ = 'no'
            ir(sock, args['-v'], int(args['<module_addr>']), type_)

        elif args['set_output']:
            set_output(sock, args['-v'], int(args['<module_addr>']),
                       int(args['<port>']), args['<type>'], int(args['<value>']))

        elif args['save_config']:
            save_config(sock, args['-v'])

        elif args['load_config']:
            load_config(sock, args['-v'])

        elif args['config'] and args['ports'] and not args['<ports_range>']:
            module_print_config(sock, args['-v'], int(args['<module_addr>']))

        elif args['config'] and args['ports'] and args['<ports_range>']:
            start, end = map(int, args['<ports_range>'].split(':'))
            if args['s-com']:
                type_ = 's-com'
            elif args['ir']:
                type_ = 'ir'
            elif args['plaini']:
                type_ = 'plaini'
            elif args['plaino']:
                type_ = 'plaino'
            else:
                raise Exception('Provide IO type!')
            module_config_ports(
                sock, args['-v'], int(args['<module_addr>']),
                range(start, end+1), type_,
                float(args['<delay>']) if args['<delay>'] else None
            )

        elif args['config'] and args['name']:
            module_config_name(
                sock, args['-v'], int(args['<module_addr>']), args['<module_name>']
            )
        elif args['set_addr']:
            set_address(sock, args['-v'], int(args['<new_address>']))
        elif args['change_addr']:
            change_address(sock, args['-v'], int(args['<module_addr>']),
                           int(args['<new_address>']))
        elif args['dvnum']:
            dvnum(sock, args['-v'], int(args['<module_addr>']), int(args['<dvnum>']))
        elif args['dvstr']:
            dvstr(sock, args['-v'], int(args['<module_addr>']), args['<dvstr>'])
        elif args['specific'] and args['<module_addr>']:
            specific_module(sock, args['-v'], int(args['<module_addr>']), list(args['<command>']))
        elif args['specific']:
            specific_broadcast(sock, args['-v'], list(args['<command>']))

    except EDaemonResponse as e:
        sys.stderr.write(str(e)+'\n')
        sys.exit(1)
