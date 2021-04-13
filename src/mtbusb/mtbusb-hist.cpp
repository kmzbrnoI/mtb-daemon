#include "mtbusb.h"

namespace Mtb {

void MtbUsb::histTimerTick() {
	if (!m_serialPort.isOpen()) {
		for (const auto& hist : m_hist)
			hist.cmd->callError();
		m_hist.clear();
	}

	if (m_hist.empty())
		return;

	if (m_hist.front().timeout < QDateTime::currentDateTime())
		histTimeoutError();
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

}; // namespace Mtb
