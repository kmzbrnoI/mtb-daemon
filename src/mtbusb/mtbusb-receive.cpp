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

		std::vector<uint8_t> message(begin, begin + length);
		parseMessage(message); // without 0x2A 0x42 length; just command code & data
		m_readData.remove(0, static_cast<int>(length+3));
	}
}

void MtbUsb::parseMessage(std::vector<uint8_t> &msg) {
}

}; // namespace Mtb
