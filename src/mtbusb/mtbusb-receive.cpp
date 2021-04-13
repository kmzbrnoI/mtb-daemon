#include "mtbusb.h"

namespace Mtb {

void MtbUsb::spHandleReadyRead() {
	// check timeout
	if (m_receiveTimeout < QDateTime::currentDateTime() && m_readData.size() > 0) {
		// clear input buffer when data not received for a long time
		m_readData.clear();
	}

	m_readData.append(m_serialPort.readAll());
	m_receiveTimeout = QDateTime::currentDateTime().addMSecs(_BUF_IN_TIMEOUT);

	// remove message till 0x2A 0x42
	int pos = m_readData.indexOf(QByteArray("\x2A\x42"));
	if (pos == -1)
		pos = m_readData.size();
	m_readData.remove(0, pos);

	while (m_readData.size() > 2 &&
	       m_readData.size() >= (m_readData[2])+3) {
		unsigned int length = m_readData[2];

		log("GET: " + dataToStr<QByteArray, uint8_t>(m_readData, length+3), LogLevel::RawData);

		auto begin = m_readData.begin();
		++begin;
		++begin;
		++begin;
		++begin;

		std::vector<uint8_t> data(begin, begin+length-1);
		parseMtbUsbMessage(m_readData[3], data); // without 0x2A 0x42 length; just command code & data
		m_readData.remove(0, static_cast<int>(length+3));
	}
}

void MtbUsb::parseMtbUsbMessage(uint8_t command_code, const std::vector<uint8_t> &data) {
	switch (static_cast<MtbUsbRecvCommand>(command_code)) {
	case MtbUsbRecvCommand::Error:
		if (data.size() >= 3)
			handleMtbUsbError(data[0], data[1], data[2]);
		return; // error is fully processed only here

	case MtbUsbRecvCommand::MtbBusForward:
		if (data.size() >= 2) {
			std::vector<uint8_t> mtbBusData(data.begin()+3, data.end());
			parseMtbBusMessage(data[1], data[2], mtbBusData);
		}
		return; // fully processes in parseMtbBusMessage

	case MtbUsbRecvCommand::MtbUsbInfo:
		if (data.size() >= 6) {
			MtbUsbInfo info;
			info.type = data[0];
			info.speed = static_cast<MtbBusSpeed>(data[1] & 0x03);
			info.fw_major = data[2];
			info.fw_minor = data[3];
			info.proto_major = data[4];
			info.proto_minor = data[5];
			m_mtbUsbInfo = info;
		}
		break;
	}

	// Find appropriate history item & call it's ok callback
	auto it = m_hist.begin();
	for (size_t i = 0; i < m_hist.size(); i++, ++it) {
		if (m_hist[i].cmd->processUsbResponse(static_cast<MtbUsbRecvCommand>(command_code), data)) {
			m_hist.erase(it);
			return;
		}
	}

	log("GET: unknown MTB-USB command "+QString(command_code), LogLevel::Warning);
}

void MtbUsb::parseMtbBusMessage(uint8_t module, uint8_t command_code, const std::vector<uint8_t> &data) {

	// Find appropriate history item & call it's ok callback
	auto it = m_hist.begin();
	for (size_t i = 0; i < m_hist.size(); i++, ++it) {
		if (is<CmdMtbUsbForward>(*m_hist[i].cmd)) {
			const CmdMtbUsbForward& forward = dynamic_cast<const CmdMtbUsbForward&>(*m_hist[i].cmd);
			if (forward.processBusResponse(static_cast<MtbBusRecvCommand>(command_code), data)) {
				m_hist.erase(it);
				return;
			}
		}
	}

	log("GET: unknown MTBbus command "+QString(command_code), LogLevel::Warning);
}

void MtbUsb::handleMtbUsbError(uint8_t code, uint8_t out_command_code, uint8_t addr) {
	MtbUsbRecvError error = static_cast<MtbUsbRecvError>(code);
	if (error == MtbUsbRecvError::NoResponse) {
		for (size_t i = 0; i < m_hist.size(); i++) {
			if (is<CmdMtbUsbForward>(*m_hist[i].cmd)) {
				const CmdMtbUsbForward& forward = dynamic_cast<const CmdMtbUsbForward&>(*m_hist[i].cmd);
				if ((out_command_code == forward.busCommandCode) && (addr == forward.module)) {
					log("GET: error: no response from module "+QString(addr)+" to command "+forward.msg(),
					    LogLevel::Warning);
					histTimeoutError(i);
					return;
				}
			}
		}

		log("GET: error not paired with outgoing command (code "+QString(code)+", out command code: "+
		    QString(out_command_code)+", addr "+QString(addr)+")", LogLevel::Warning);

	} else if (error == MtbUsbRecvError::NoResponse) {
		log("GET: error: full buffer (code "+QString(code)+", out command code: "+
		    QString(out_command_code)+", addr "+QString(addr)+")", LogLevel::Warning);
		// TODO: resend? report as error?
		// currently: error event will be called on timeout

	} else {
		log("GET: unknown error (code "+QString(code)+", out command code: "+
		    QString(out_command_code)+", addr "+QString(addr)+")", LogLevel::Warning);
	}
}

void MtbUsb::histTimeoutError(size_t i) {
	auto it = m_hist.begin();
	for (size_t j = 0; j < i; j++) {
		++it;
		if (it == m_hist.end())
			return;
	}

	assert(m_hist[i].cmd != nullptr);
	m_hist[i].cmd->callError();
	m_hist.erase(it);

	if (!m_out.empty())
		this->sendNextOut();
}

}; // namespace Mtb
