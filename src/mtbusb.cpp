#include "mtbusb.h"

namespace Mtb {

MtbUsb::MtbUsb(QObject *parent) : QObject(parent) {
	m_serialPort.setReadBufferSize(128);

	QObject::connect(&m_serialPort, SIGNAL(readyRead()), this, SLOT(spHandleReadyRead()));
	QObject::connect(&m_serialPort, SIGNAL(errorOccurred(QSerialPort::SerialPortError)), this,
	                 SLOT(spHandleError(QSerialPort::SerialPortError)));
	QObject::connect(&m_serialPort, SIGNAL(aboutToClose()), this, SLOT(spAboutToClose()));
}

void MtbUsb::log(const QString &message, const LogLevel loglevel) {
	if (loglevel <= this->loglevel)
		onLog(message, loglevel);
}

void MtbUsb::spAboutToClose() {
	m_histTimer.stop();
	m_outTimer.stop();
	/*while (!m_hist.empty()) {
		if (nullptr != m_hist.front().callback_err)
			m_hist.front().callback_err->func(this, m_hist.front().callback_err->data);
		m_hist.pop_front();
	}
	while (!m_out.empty()) {
		if (nullptr != m_out.front().callback_err)
			m_out.front().callback_err->func(this, m_out.front().callback_err->data);
		m_out.pop_front();
	}*/

	log("Disconnected", LogLevel::Info);
}

void MtbUsb::spHandleError(QSerialPort::SerialPortError serialPortError) {
	if (serialPortError != QSerialPort::NoError) {
		// Serial port error is considered as fatal → close device immediately
		if (this->connected())
			this->disconnect();
		log("Serial port error: "+m_serialPort.errorString(), LogLevel::Error);
	}
}

QString flowControlToStr(QSerialPort::FlowControl fc) {
	if (fc == QSerialPort::FlowControl::HardwareControl)
		return "hardware";
	if (fc == QSerialPort::FlowControl::SoftwareControl)
		return "software";
	if (fc == QSerialPort::FlowControl::NoFlowControl)
		return "no";
	return "unknown";
}

/* Public functions API ------------------------------------------------------*/

void MtbUsb::connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc) {
	log("Connecting to " + portname + ", br=" + QString::number(br) +
	    ", fc=" + flowControlToStr(fc) + ") ...", LogLevel::Info);

	m_serialPort.setBaudRate(br);
	m_serialPort.setFlowControl(fc);
	m_serialPort.setPortName(portname);

	if (!m_serialPort.open(QIODevice::ReadWrite))
		throw EOpenError(m_serialPort.errorString());

	m_histTimer.start(_HIST_CHECK_INTERVAL);
	log("Connected", LogLevel::Info);
	onConnect();
}

void MtbUsb::disconnect() {
	log("Disconnecting...", LogLevel::Info);
	m_serialPort.close();
	onDisconnect();
}

bool MtbUsb::connected() const { return m_serialPort.isOpen(); }

}; // namespace Mtb
