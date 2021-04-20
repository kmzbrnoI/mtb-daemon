#!/usr/bin/env python3

import socket
import sys
import json


def main() -> None:
    if len(sys.argv) < 5:
        sys.stderr.write('Usage: fw_upgrade.py server port module_addr hexfile\n')
        sys.exit(1)
    server, port, addr, hexfilename = sys.argv[1:]
    port = int(port)
    addr = int(addr)

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
        'command':'module_upgrade_fw',
        'address': addr,
        'firmware': firmware,
    }
    s.send((json.dumps(to_send)+'\n').encode('utf-8'))

    message = s.recv(0xFFFF).decode('utf-8').strip()
    print(message)

if __name__ == '__main__':
    main()

