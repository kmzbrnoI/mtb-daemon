#include "mtbusb.h"

namespace Mtb {

void MtbUsb::histTimerTick() {
	if (!m_serialPort.isOpen()) {
		for (const auto &hist : m_hist)
			hist.cmd->callError(CmdError::SerialPortClosed);
		m_hist.clear();
	}

	if (m_hist.empty())
		return;

	if (m_hist.front().timeout < QDateTime::currentDateTime()) {
		if (m_hist.front().no_sent >= _HIST_SEND_MAX)
			histTimeoutError(CmdError::UsbNoResponse);
		else
			histResend();
	}
}

void MtbUsb::histResend() {
	HistoryItem hist = std::move(m_hist.front());
	m_hist.pop_front();

	// to_send guarantees us that conflict can never occur in hist buffer
	// we just check conflict in out buffer

	if (this->conflictWithOut(*(hist.cmd))) {
		log("Not sending again, conflict: " + hist.cmd->msg(), LogLevel::Warning);
		hist.cmd->callError(CmdError::HistoryConflict);
		if (!m_out.empty())
			this->sendNextOut();
		return;
	}

	log("Sending again: " + hist.cmd->msg(), LogLevel::Warning);

	try {
		this->write(std::move(hist.cmd), hist.no_sent+1);
	} catch (...) {}
}

bool MtbUsb::conflictWithHistory(const Cmd &cmd) const {
	for (const HistoryItem &hist : m_hist)
		if (hist.cmd->conflict(cmd) || cmd.conflict(*(hist.cmd)))
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
