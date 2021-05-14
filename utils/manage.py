#!/usr/bin/env python3

"""
MTB Daemon Command Line Utility

Usage:
  manage.py [options] mtbusb
  manage.py [options] mtbusb speed <speed>
  manage.py [options] module <module_addr>
  manage.py [options] inputs <module_addr>
  manage.py [options] reboot <module_addr>
  manage.py [options] monitor <module_addr>
  manage.py [options] beacon <module_addr> <state>
  manage.py [options] fw_upgrade <module_addr> <hexfilename>
  manage.py [options] ir <module_addr> (yes|no|auto)
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
from typing import Dict, Any
import datetime


class EDaemonResponse(Exception):
    pass


def request_response(socket, verbose,
                     request: Dict[str, Any]) -> Dict[str, Any]:
    to_send = request
    to_send['type'] = 'request'
    socket.send((json.dumps(to_send)+'\n').encode('utf-8'))

    while True:
        data = json.loads(socket.recv(0xFFFF).decode('utf-8').strip())
        if verbose:
            print(data)
        if data.get('command', '') == request['command']:
            if data.get('status', '') != 'ok':
                raise EDaemonResponse(
                    data.get('error', {}).get('message', 'Uknown error!')
                )
            return data


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


def uni_inputs_str(inputs: Dict[str, Any]) -> str:
    return ((''.join(str(int(val)) for val in inputs['full'][:8])) + ' ' +
            (''.join(str(int(val)) for val in inputs['full'][8:])))


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


def reboot(socket, verbose: bool, module_: int) -> None:
    request_response(socket, verbose, {
        'command': 'module_reboot',
        'address': module_,
    })
    module(socket, verbose, module_)


def monitor(socket, verbose: bool, module: int) -> None:
    response = request_response(socket, verbose, {
        'command': 'module_subscribe',
        'addresses': [module],
    })
    print('['+str(datetime.datetime.now().time())+']', end=' ')
    get_inputs(socket, verbose, module)

    while True:
        data = json.loads(socket.recv(0xFFFF).decode('utf-8').strip())
        if verbose:
            print('['+str(datetime.datetime.now().time())+']', end=' ')
            print(data)

        if data.get('command', '') == 'module_inputs_changed':
            print('['+str(datetime.datetime.now().time())+']', end=' ')
            print(uni_inputs_str(data['module_inputs_changed']['inputs']))

        if data.get('command', '') == 'module_outputs_changed':
            print('['+str(datetime.datetime.now().time())+']', end=' ')
            print('Outputs changed')


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

        elif args['reboot']:
            reboot(sock, args['-v'], int(args['<module_addr>']))

        elif args['monitor']:
            monitor(sock, args['-v'], int(args['<module_addr>']))

        elif args['beacon']:
            beacon(sock, args['-v'], int(args['<module_addr>']),
                   bool(int(args['<state>'])))

        elif args['ir']:
            type_ = ''
            if args['auto']:
                type_ = 'auto'
            elif args['yes']:
                type_ = 'yes'
            elif args['no']:
                type_ = 'no'
            ir(sock, args['-v'], int(args['<module_addr>']), type_)

    except EDaemonResponse as e:
        sys.stderr.write(str(e)+'\n')
        sys.exit(1)
