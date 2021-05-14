#!/usr/bin/env python3

"""
MTB Daemon Command Line Utility

Usage:
  manage.py [options] fw_upgrade <module_addr> <hexfilename>

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
               verbose: bool) -> int:
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


if __name__ == '__main__':
    args = docopt(__doc__)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((args['-s'], int(args['-p'])))

    try:
        if args['fw_upgrade']:
            result = fw_upgrade(
                sock,
                int(args['<module_addr>']),
                args['<hexfilename>'],
                args['-v'],
            )
            sys.exit(result)
    except EDaemonResponse as e:
        sys.stderr.write(str(e)+'\n')
        sys.exit(1)
