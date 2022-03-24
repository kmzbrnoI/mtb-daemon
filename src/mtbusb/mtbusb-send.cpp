#include "mtbusb.h"

namespace Mtb {

void MtbUsb::send(std::vector<uint8_t> data) {
	data.emplace(data.begin(), data.size());
	data.emplace(data.begin(), 0x42);
	data.emplace(data.begin(), 0x2A);

	log("PUT: " + dataToStr<std::vector<uint8_t>, uint8_t>(data), LogLevel::RawData);
	QByteArray qdata(reinterpret_cast<const char *>(data.data()), data.size());

	if (!m_serialPort.isOpen())
		throw EWriteError("Serial port not open!");

	qint64 sent = m_serialPort.write(qdata);
	if (sent == -1 || sent != qdata.size())
		throw EWriteError("No data could we written!");
}

void MtbUsb::write(std::unique_ptr<const Cmd> cmd, size_t no_sent) {
	assert(nullptr != cmd);
	log("PUT: " + cmd->msg(), LogLevel::Commands);

	try {
		send(cmd->getBytes());
		m_hist.emplace_back(
			cmd,
			QDateTime::currentDateTime().addMSecs(_HIST_TIMEOUT),
			no_sent
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
		write(std::move(cmd));
	}
}

void MtbUsb::sendNextOut() {
	std::unique_ptr<const Cmd> out = std::move(m_out.front());
	log("DEQUEUE: " + out->msg(), LogLevel::Debug);
	m_out.pop_front();
	send(out, true);
}

} // namespace Mtb
