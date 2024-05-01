#!/usr/bin/env python3

import logging
import socket

import common


HOST = '127.0.0.1'
PORT = 3841

mtb_daemon = common.MtbDaemonIFace(HOST, PORT)


def test_mtbusb() -> None:
    mtbusb = mtb_daemon.request_response({'command': 'mtbusb'})
