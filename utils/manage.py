#!/usr/bin/env python3

"""
MTB Daemon Command Line Utility

Usage:
  manage.py [options] mtbusb
  manage.py [options] mtbusb speed <speed>
  manage.py [options] module <module_addr>
  manage.py [options] inputs <module_addr>
  manage.py [options] outputs <module_addr>
  manage.py [options] reboot <module_addr>
  manage.py [options] monitor <module_addr>
  manage.py [options] beacon <module_addr> <state>
  manage.py [options] fw_upgrade <module_addr> <hexfilename>
  manage.py [options] ir <module_addr> (yes|no|auto)
  manage.py [options] set_output <module_addr> <port> <value>
  manage.py [options] load_config
  manage.py [options] save_config
  manage.py [options] config <module_addr> ports <ports_range> (plaini|plaino|s-com|ir) [<delay>]
  manage.py [options] config <module_addr> name <module_name>
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
from docopt import docopt  # type: ignore
from typing import Dict, Any, Optional
import datetime


class EDaemonResponse(Exception):
    pass


def request_response(socket, verbose,
                     request: Dict[str, Any]) -> Dict[str, Any]:
    to_send = request
    to_send['type'] = 'request'
    socket.send((json.dumps(to_send)+'\n').encode('utf-8'))

    while True:
        data = socket.recv(0xFFFF).decode('utf-8').strip()
        messages = [json.loads(msg) for msg in data.split('\n')]
        for message in messages:
            if verbose:
                print(message)
            if message.get('command', '') == request['command']:
                if message.get('status', '') != 'ok':
                    raise EDaemonResponse(
                        message.get('error', {}).get('message', 'Uknown error!')
                    )
                return message


def fw_upgrade(socket, module_addr: int, hexfilename: str,
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


def mtbusb(socket, verbose: bool) -> None:
    response = request_response(socket, verbose, {'command': 'mtbusb'})
    for key, val in response['mtbusb'].items():
        print(key, ':', val)


def mtbusb_speed(socket, verbose: bool, speed: int) -> None:
    response = request_response(socket, verbose, {
        'command': 'mtbusb',
        'mtbusb': {'speed': speed},
    })
    for key, val in response['mtbusb'].items():
        print(key, ':', val)


def module(socket, verbose: bool, module: int) -> None:
    response = request_response(socket, verbose, {
        'command': 'module',
        'address': module,
    })
    for key, val in response['module'].items():
        print(key, ':', val)


def uni_print_config(config: Dict[str, Any]) -> None:
    print('Inputs:')
    for i, delay in enumerate(config['inputsDelay']):
        ir = config['irs'][i] if 'irs' in config else False
        irstr = 'IR' if ir else ''
        print(f'  {i}: {delay} {irstr}')

    print('Outputs:')
    for i, d in enumerate(config['outputsSafe']):
        print(f'  {i}: {d["type"]} {d["value"]}')


def uni_inputs_str(inputs: Dict[str, Any]) -> str:
    return ((''.join(str(int(val)) for val in inputs['full'][:8])) + ' ' +
            (''.join(str(int(val)) for val in inputs['full'][8:])))


def uni_outputs_str(outputs: Dict[str, Any]) -> str:
    result = ''
    for i, (port, value) in enumerate(outputs.items()):
        if value['type'] == 'plain' or value['value'] == 0:
            result += str(value['value'])
        else:
            result += value['type'][0]
        if i == 7:
            result += ' '
    return result


def get_inputs(socket, verbose: bool, module: int) -> None:
    response = request_response(socket, verbose, {
        'command': 'module',
        'address': module,
        'state': True,
    })
    module = response['module']
    module_spec = module[module['type']]
    if 'state' not in module_spec:
        raise EDaemonResponse(
            'No state received - is module active? Is it in bootloader?'
        )
    inputs = module_spec['state']['inputs']
    print(uni_inputs_str(inputs))


def get_outputs(socket, verbose: bool, module: int) -> None:
    response = request_response(socket, verbose, {
        'command': 'module',
        'address': module,
        'state': True,
    })
    module = response['module']
    module_spec = module[module['type']]
    if 'state' not in module_spec:
        raise EDaemonResponse(
            'No state received - is module active? Is it in bootloader?'
        )
    outputs = module_spec['state']['outputs']
    print(uni_outputs_str(outputs))


def reboot(socket, verbose: bool, module_: int) -> None:
    request_response(socket, verbose, {
        'command': 'module_reboot',
        'address': module_,
    })
    module(socket, verbose, module_)


def monitor(socket, verbose: bool, module: int) -> None:
    request_response(socket, verbose, {
        'command': 'module_subscribe',
        'addresses': [module],
    })
    print('['+str(datetime.datetime.now().time())+']', end=' ')
    get_inputs(socket, verbose, module)

    while True:
        data = socket.recv(0xFFFF).decode('utf-8').strip()
        messages = [json.loads(msg) for msg in data.split('\n')]
        for message in messages:
            if verbose:
                print('['+str(datetime.datetime.now().time())+']', end=' ')
                print(message)
            command = message.get('command', '')

            if command == 'module_inputs_changed':
                print('['+str(datetime.datetime.now().time())+']', end=' ')
                print(uni_inputs_str(message['module_inputs_changed']['inputs']))

            if command == 'module_outputs_changed':
                print('['+str(datetime.datetime.now().time())+'] Outputs changed')

            if command == 'module' and int(message['module']['address']) == module:
                print('['+str(datetime.datetime.now().time())+']', end=' ')
                print(f'state : {message["module"]["state"]}')
                type_ = message['module']['type']
                if 'state' in message['module'][type_]:
                    state = message['module'][type_]['state']
                    print('['+str(datetime.datetime.now().time())+']', end=' ')
                    print(uni_inputs_str(state['inputs']))

            if command == 'mtbusb':
                print('['+str(datetime.datetime.now().time())+']', end=' ')
                print(f'MTB-USB: {message["mtbusb"]}')


def beacon(socket, verbose: bool, module_: int, beacon: bool) -> None:
    request_response(socket, verbose, {
        'command': 'module_beacon',
        'address': module_,
        'beacon': beacon,
    })


def ir(socket, verbose: bool, module_: int, type_: str) -> None:
    data = []
    if type_ == 'auto':
        data = [0x01, 0xFF]
    elif type_ == 'yes':
        data = [0x01, 0x01]
    elif type_ == 'no':
        data = [0x01, 0x00]

    request_response(socket, verbose, {
        'command': 'module_specific_command',
        'address': module_,
        'data': data,
    })


def set_output(socket, verbose: bool, module: int, port: int, value: int) -> None:
    if value < 2:
        portdata = {'type': 'plain', 'value': value}
    else:
        portdata = {'type': 'flicker', 'value': value}

    request_response(socket, verbose, {
        'command': 'module_set_outputs',
        'address': module,
        'outputs': {
            port: portdata,
        },
    })

    # Wait because disconnect causes output reset
    while True:
        socket.recv(0xFFFF).decode('utf-8').strip()


def load_config(socket, verbose: bool) -> None:
    request_response(socket, verbose, {
        'command': 'load_config',
    })


def save_config(socket, verbose: bool) -> None:
    request_response(socket, verbose, {
        'command': 'save_config',
    })


def module_config_ports(socket, verbose: bool, module: int, rg, io_type: str,
                        delay: Optional[int]) -> None:
    response = request_response(socket, verbose, {
        'command': 'module',
        'address': module,
    })
    type_ = response['module']['type']
    config = response['module'][type_]['config']

    if io_type == 'ir' or io_type == 'plaini':
        for i in rg:
            config['irs'][i] = (io_type == 'ir')
        for i in rg:
            config['inputsDelay'][i] = delay

    if io_type == 's-com' or io_type == 'plaino':
        for i in rg:
            config['outputsSafe'][i]['type'] = io_type[:5]

    print(f'Module {module} – {response["module"]["name"]}:')
    uni_print_config(config)
    request_response(socket, verbose, {
        'command': 'module_set_config',
        'address': module,
        'type_code': response['module']['type_code'],
        'name': response['module']['name'],
        'config': config,
    })


def module_config_name(socket, verbose: bool, module: int, name: str) -> None:
    response = request_response(socket, verbose, {
        'command': 'module',
        'address': module,
    })
    type_ = response['module']['type']
    config = response['module'][type_]['config']

    request_response(socket, verbose, {
        'command': 'module_set_config',
        'address': module,
        'type_code': response['module']['type_code'],
        'name': name,
        'config': config,
    })


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

        elif args['module']:
            module(sock, args['-v'], int(args['<module_addr>']))

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
                       int(args['<port>']), int(args['<value>']))

        elif args['load_config']:
            load_config(sock, args['-v'])

        elif args['save_config']:
            save_config(sock, args['-v'])

        elif args['config'] and args['ports']:
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
                range(start, end+1), type_, args['<delay>']
            )

        elif args['config'] and args['name']:
            module_config_name(
                sock, args['-v'], int(args['<module_addr>']), args['<module_name>']
            )


    except EDaemonResponse as e:
        sys.stderr.write(str(e)+'\n')
        sys.exit(1)
