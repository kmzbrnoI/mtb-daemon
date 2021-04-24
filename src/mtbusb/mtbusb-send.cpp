#include "mtbusb.h"

namespace Mtb {

void MtbUsb::send(std::vector<uint8_t> data) {
	data.emplace(data.begin(), data.size());
	data.emplace(data.begin(), 0x42);
	data.emplace(data.begin(), 0x2A);

	log("PUT: " + dataToStr<std::vector<uint8_t>, uint8_t>(data), LogLevel::RawData);
	QByteArray qdata(reinterpret_cast<const char *>(data.data()), data.size());

	qint64 sent = m_serialPort.write(qdata);
	if (sent == -1 || sent != qdata.size())
		throw EWriteError("No data could we written!");
}

void MtbUsb::write(std::unique_ptr<const Cmd> cmd) {
	assert(nullptr != cmd);
	log("PUT: " + cmd->msg(), LogLevel::Commands);

	try {
		m_lastSent = QDateTime::currentDateTime();
		send(cmd->getBytes());
		m_hist.emplace_back(
			cmd,
			QDateTime::currentDateTime().addMSecs(_HIST_TIMEOUT)
		);
	} catch (std::exception &) {
		log("Fatal error when writing command: " + cmd->msg(), LogLevel::Error);
		cmd->callError(CmdError::SerialPortClosed);
	}
}

void MtbUsb::send(std::unique_ptr<const Cmd> &cmd, bool bypass_m_out_emptiness) {
	// Sends or queues
	if ((m_hist.size() >= _MAX_HIST_BUF_COUNT) || (!m_out.empty() && !bypass_m_out_emptiness) ||
	    conflictWithHistory(*cmd)) {
		// History full -> push & do not start timer (response from CS will send automatically)
		// We ensure history buffer never contains commands with conflict
		log("ENQUEUE: " + cmd->msg(), LogLevel::Debug);
		m_out.emplace_back(std::move(cmd));
	} else {
		if (m_lastSent.addMSecs(_OUT_TIMER_INTERVAL) > QDateTime::currentDateTime()) {
			// Last command sent too early, still space in hist buffer ->
			// queue & activate timer for next send
			log("ENQUEUE: " + cmd->msg(), LogLevel::Debug);
			m_out.emplace_back(std::move(cmd));
			if (!m_outTimer.isActive())
				m_outTimer.start();
		} else {
			write(std::move(cmd));
		}
	}
}

void MtbUsb::outTimerTick() {
	if (m_out.empty())
		m_outTimer.stop();
	else
		sendNextOut();
}

void MtbUsb::sendNextOut() {
	if (m_lastSent.addMSecs(_OUT_TIMER_INTERVAL) > QDateTime::currentDateTime()) {
		if (!m_outTimer.isActive())
			m_outTimer.start();
		return;
	}

	std::unique_ptr<const Cmd> out = std::move(m_out.front());
	log("DEQUEUE: " + out->msg(), LogLevel::Debug);
	m_out.pop_front();
	send(out, true);
}

} // namespace Mtb
