#!/usr/bin/env python3

import socket
from typing import Dict, Any
import json


class EDaemonResponse(Exception):
    pass


def request_response(socket: socket.socket, request: Dict[str, Any]) -> Dict[str, Any]:
    to_send = request
    to_send['type'] = 'request'
    socket.send((json.dumps(to_send)+'\n').encode('utf-8'))

    while True:
        data = socket.recv(0xFFFF).decode('utf-8').strip()
        messages = [json.loads(msg) for msg in data.split('\n')]
        for message in messages:
            assert isinstance(message, dict)
            if message.get('command', '') == request['command']:
                if message.get('status', '') != 'ok':
                    raise EDaemonResponse(
                        message.get('error', {}).get('message', 'Unknown error!')
                    )
                return message


def main() -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('127.0.0.1', 3841))

    request_response(sock, {
        'command': 'module_set_outputs',
        'address': 1,
        'outputs': {
            0: {'type': 'plain', 'value': 1},
        },
    })

    # Wait because disconnect causes output reset
    while True:
        sock.recv(0xFFFF).decode('utf-8').strip()


if __name__ == '__main__':
    main()
