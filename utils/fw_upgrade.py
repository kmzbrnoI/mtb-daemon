#!/usr/bin/env python3

"""
MTB Daemon Firmware Upgrade Command Line Utility

Usage:
  fw_upgrade.py [options] <module_addr> <hexfilename>

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


def fw_upgrade(server: str, port: int, module_addr: int, hexfilename: str,
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

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((server, port))

    to_send = {
        'command': 'module_upgrade_fw',
        'address': module_addr,
        'firmware': firmware,
        'id': 12,
    }
    s.send((json.dumps(to_send)+'\n').encode('utf-8'))

    while True:
        data = json.loads(s.recv(0xFFFF).decode('utf-8').strip())
        if verbose:
            print(data)
        if data.get('command', '') == 'module_upgrade_fw':
            if data.get('status', '') != 'ok':
                sys.stderr.write(
                    data.get('error', {}).get('message', 'Uknown error!')+'\n'
                )
            return 0 if data.get('status', '') == 'ok' else 1


if __name__ == '__main__':
    args = docopt(__doc__)

    result = fw_upgrade(
        args['-s'],
        int(args['-p']),
        int(args['<module_addr>']),
        args['<hexfilename>'],
        args['-v'],
    )
    sys.exit(result)
