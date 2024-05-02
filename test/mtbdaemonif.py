"""
Interface to MTB Daemon JSON TCP server
"""

from typing import Dict, Any
import socket
import logging
import json
import select
import time


HOST = '127.0.0.1'
PORT = 3841


class EMtbDaemon(Exception):
    pass


class EMtbDaemonTimeout(EMtbDaemon):
    pass


class MtbDaemonIFace:
    def __init__(self, host: str = HOST, port: int = PORT):
        self.host = host
        self.port = port
        self.buf_received = ''
        self.id: int = 0
        self.connect()

    def disconnect(self) -> None:
        logging.info('Disconnecting from mtb-daemon ...')
        self.sock.close()
        logging.info('Disconnected')

    def connect(self) -> None:
        logging.info(f'Connecting to {self.host}:{self.port} ...')
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        logging.info('Connected')

    def send_message(self, data: Dict[str, Any]) -> None:
        logging.debug(f'Send: {data}')
        self.sock.send((json.dumps(data)+'\n').encode('utf-8'))

    def send_request(self, data: Dict[str, Any]) -> None:
        data['type'] = 'request'
        self.send_message(data)

    def expect_message(self, command: str, timeout: float = 1) -> Dict[str, Any]:
        start = time.time()
        while True:
            readable, _, _ = select.select([self.sock], [], [], timeout)
            if self.sock in readable:
                self.buf_received += self.sock.recv(0xFFFF).decode('utf-8')
                logging.debug(f'{self.buf_received=}')
                while '\n' in self.buf_received:
                    offset = self.buf_received.find('\n')
                    message = json.loads(self.buf_received[:offset])
                    self.buf_received = self.buf_received[offset+1:]

                    logging.debug(f'Received: {message}')
                    assert isinstance(message, dict)

                    # Strictly forbid receiving of other commands and events,
                    # allow only empty message.
                    if message == {}:
                        continue

                    assert 'command' in message
                    if message['command'] == command:
                        return message

            if (time.time() - start) >= timeout:
                raise EMtbDaemonTimeout(
                    f'Timeout waiting for mtb-daemon"s response to {command} command!'
                )

    def expect_response(self, command: str, timeout: float = 1, ok: bool = True) -> Dict[str, Any]:
        response = self.expect_message(command, timeout)
        assert 'status' in response
        if ok:
            assert response['status'] == 'ok'
        assert 'type' in response
        assert response['type'] == 'response'
        return response

    def request_response(self, request: Dict[str, Any], timeout: float = 1,
                         ok: bool = True) -> Dict[str, Any]:
        assert 'command' in request
        if 'id' not in request:
            request['id'] = self.id
            self.id += 1
        self.send_request(request)
        response = self.expect_response(request['command'], timeout, ok)
        assert 'id' in response
        assert response['id'] == request['id']
        return response


mtb_daemon = MtbDaemonIFace()
