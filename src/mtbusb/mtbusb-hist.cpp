#include "mtbusb.h"

namespace Mtb {

void MtbUsb::pendingTimerTick() {
	if (!m_serialPort.isOpen()) {
		for (const auto &pending : m_pending)
			pending.cmd->callError(CmdError::SerialPortClosed);
		m_pending.clear();
	}

	if (m_pending.empty())
		return;

	if (m_pending.front().timeout < QDateTime::currentDateTime()) {
		if (m_pending.front().no_sent >= _PENDING_RESEND_MAX)
			pendingTimeoutError(CmdError::UsbNoResponse);
		else
			pendingResend();
	}
}

void MtbUsb::pendingResend() {
	PendingCmd pending = std::move(m_pending.front());
	m_pending.pop_front();

	// to_send guarantees us that conflict can never occur in pending buffer
	// we just check conflict in out buffer

	if (this->conflictWithOut(*(pending.cmd))) {
		log("Not sending again, conflict: " + pending.cmd->msg(), LogLevel::Warning);
		pending.cmd->callError(CmdError::PendingConflict);
		if (!m_out.empty())
			this->sendNextOut();
		return;
	}

	log("Sending again: " + pending.cmd->msg(), LogLevel::Warning);

	try {
		this->write(std::move(pending.cmd), pending.no_sent+1);
	} catch (...) {}
}

bool MtbUsb::conflictWithPending(const Cmd &cmd) const {
	for (const PendingCmd &pending : m_pending)
		if (pending.cmd->conflict(cmd) || cmd.conflict(*(pending.cmd)))
			return true;
	return false;
}

bool MtbUsb::conflictWithOut(const Cmd &cmd) const {
	for (const auto &out : m_out)
		if (out->conflict(cmd) || cmd.conflict(*out))
			return true;
	return false;
}

} // namespace Mtb
