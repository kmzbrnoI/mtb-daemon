"""
Testing of MTB Daemon behavior requiring manual interventions
(e.g. disconnecting power from MTB module etc.).
"""

import logging

import common
from mtbdaemonif import mtb_daemon


def test_mtbusb_module_lost() -> None:
    response = mtb_daemon.request_response({'command': 'mtbusb'})
    assert response['mtbusb']['active_modules'] == [common.TEST_MODULE_ADDR]

    with common.TopoSubscription(mtb_daemon):
        logging.info(f'Now turn the power of module {common.TEST_MODULE_ADDR} off!')
        event = mtb_daemon.expect_event('mtbusb', timeout=10)
        assert event['mtbusb']['active_modules'] == []


def test_mtbusb_module_found() -> None:
    response = mtb_daemon.request_response({'command': 'mtbusb'})
    assert response['mtbusb']['active_modules'] == []

    with common.TopoSubscription(mtb_daemon):
        logging.info(f'Now turn the power of module {common.TEST_MODULE_ADDR} on!')
        event = mtb_daemon.expect_event('mtbusb', timeout=10)
        assert event['mtbusb']['active_modules'] == [common.TEST_MODULE_ADDR]


def test_mtbusb_disconnect() -> None:
    response = mtb_daemon.request_response({'command': 'mtbusb'})
    assert response['mtbusb']['connected']

    with common.TopoSubscription(mtb_daemon):
        logging.info('Now disconnect MTB-USB!')
        event = mtb_daemon.expect_event('mtbusb', timeout=10)
        assert not event['mtbusb']['connected']


def test_mtbusb_connect() -> None:
    response = mtb_daemon.request_response({'command': 'mtbusb'})
    assert not response['mtbusb']['connected']

    with common.TopoSubscription(mtb_daemon):
        logging.info('Now connect MTB-USB!')
        event = mtb_daemon.expect_event('mtbusb', timeout=10)
        assert event['mtbusb']['connected']
