"""
Test events of MTB Daemon TCP server using PyTest.

moc = module_output_changed
mic = module_input_changed
"""

import common
from mtbdaemonif import mtb_daemon, MtbDaemonIFace


def test_mic_event() -> None:
    with common.ModuleSubscription(mtb_daemon, common.TEST_MODULE_ADDR):
        common.set_single_output(common.TEST_MODULE_ADDR, 0, 1)
        ic_event = mtb_daemon.expect_event('module_inputs_changed')
        common.validate_ic_event(ic_event, common.TEST_MODULE_ADDR, 0, True)

        common.set_single_output(common.TEST_MODULE_ADDR, 0, 0)
        ic_event = mtb_daemon.expect_event('module_inputs_changed')
        common.validate_ic_event(ic_event, common.TEST_MODULE_ADDR, 0, False)


def test_moc_event() -> None:
    with MtbDaemonIFace() as second_daemon, \
            common.ModuleSubscription(second_daemon, common.TEST_MODULE_ADDR):
        common.set_single_output(common.TEST_MODULE_ADDR, 1, 1)
        ic_event = second_daemon.expect_event('module_outputs_changed')
        common.validate_oc_event(ic_event, common.TEST_MODULE_ADDR, 1, 1)


def test_module_subscribe_inactive() -> None:
    with common.ModuleSubscription(mtb_daemon, common.INACTIVE_MODULE_ADDR):
        pass


def test_no_event_other_module() -> None:
    with MtbDaemonIFace() as second_daemon, \
            common.ModuleSubscription(second_daemon, common.INACTIVE_MODULE_ADDR):
        common.set_single_output(common.TEST_MODULE_ADDR, 0, 1)
        common.set_single_output(common.TEST_MODULE_ADDR, 0, 0)
        second_daemon.expect_no_message(timeout=0.5)


# TODO: module_subscribe
# TODO: module_unsubscribe
# TODO: my_module_subscribes
# TODO: topology_subscribe
# TODO: topology_unsubscribe

# TODO: module_inputs_changed
# TODO: module_outputs_changed
# TODO: mtbusb changed ???
# TODO: module changed
# TODO: module_deleted
