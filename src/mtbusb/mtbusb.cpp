#include <QSerialPortInfo>
#include "mtbusb.h"
#include "mtbusb-win-com-discover.h"

namespace Mtb {

MtbUsb::MtbUsb(QObject *parent) : QObject(parent) {
	m_serialPort.setReadBufferSize(128);

	QObject::connect(&m_serialPort, SIGNAL(readyRead()), this, SLOT(spHandleReadyRead()));
	QObject::connect(&m_serialPort, SIGNAL(errorOccurred(QSerialPort::SerialPortError)), this,
	                 SLOT(spHandleError(QSerialPort::SerialPortError)));
	QObject::connect(&m_serialPort, SIGNAL(aboutToClose()), this, SLOT(spAboutToClose()));

	QObject::connect(&m_pendingTimer, SIGNAL(timeout()), this, SLOT(pendingTimerTick()));
	QObject::connect(&m_pingTimer, SIGNAL(timeout()), this, SLOT(pingTimerTick()));

	m_pingTimer.setInterval(_PING_SEND_PERIOD_MS);
}

void MtbUsb::log(const QString &message, const LogLevel loglevel) {
	if (loglevel <= this->loglevel)
		emit onLog(message, loglevel);
}

void MtbUsb::spAboutToClose() {
	m_pendingTimer.stop();
	m_pingTimer.stop();
	while (!m_pending.empty()) {
		m_pending.front().cmd->callError(CmdError::SerialPortClosed);
		m_pending.pop_front();
	}
	while (!m_out.empty()) {
		m_out.front()->callError(CmdError::SerialPortClosed);
		m_out.pop_front();
	}
	m_mtbUsbInfo.reset();
	m_activeModules.reset();

	log("Disconnected", LogLevel::Info);
}

void MtbUsb::spHandleError(QSerialPort::SerialPortError serialPortError) {
	if (serialPortError != QSerialPort::NoError) {
		// Serial port error is considered as fatal → close device immediately
		if (this->connected())
			this->disconnect();
		log("Serial port error: " + m_serialPort.errorString(), LogLevel::Error);
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

void MtbUsb::pingTimerTick() {
	if (this->connected() && this->ping) {
		this->send(
			Mtb::CmdMtbUsbPing(
				{[](void*) {}},
				{[this](Mtb::CmdError, void*) { this->disconnect(); }}
			)
		);
	}
}

/* Public functions API ------------------------------------------------------*/

void MtbUsb::connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc) {
	log("Connecting to " + portname + ", br=" + QString::number(br) +
	    ", fc=" + flowControlToStr(fc) + "...", LogLevel::Info);

	m_serialPort.setBaudRate(br);
	m_serialPort.setFlowControl(fc);
	m_serialPort.setPortName(portname);

	if (!m_serialPort.open(QIODevice::ReadWrite))
		throw EOpenError(m_serialPort.errorString());

	m_serialPort.setDataTerminalReady(true);

	m_pendingTimer.start(_PENDING_CHECK_INTERVAL);
	m_pingTimer.start();
	log("Connected", LogLevel::Info);
	emit onConnect();
}

void MtbUsb::disconnect() {
	if (!this->connected())
		return;

	log("Disconnecting...", LogLevel::Info);
	m_serialPort.close();
	emit onDisconnect();
}

bool MtbUsb::connected() const { return m_serialPort.isOpen(); }

std::vector<QSerialPortInfo> MtbUsb::ports() {
#ifdef Q_OS_WIN
	return winMtbUsbPorts();
#else
	std::vector<QSerialPortInfo> result;
	QList<QSerialPortInfo> ports(QSerialPortInfo::availablePorts());
	for (const QSerialPortInfo &info : ports)
		if (info.description() == "MTB-USB v4")
			result.push_back(info);
	return result;
#endif
}

void MtbUsb::changeSpeed(MtbBusSpeed newSpeed, std::function<void()> onOk, std::function<void(Mtb::CmdError)> onError) {
	if ((!this->connected()) || (!this->m_mtbUsbInfo.has_value()))
		return;

	// Send 3× broadcast to change module speed
	this->send(Mtb::CmdMtbModuleChangeSpeed(newSpeed));
	this->send(Mtb::CmdMtbModuleChangeSpeed(newSpeed));
	this->send(
		Mtb::CmdMtbModuleChangeSpeed(
			newSpeed,
			{[this, onOk, onError, newSpeed](void*) {
				// Send MTB-USB speed change request
				this->send(
					Mtb::CmdMtbUsbChangeSpeed(
						newSpeed,
						{[this, newSpeed, onOk](void*) {
							this->m_mtbUsbInfo.value().speed = newSpeed;
							onOk();
						}},
						{[onError](Mtb::CmdError cmdError, void*) { onError(cmdError); }}
					)
				);
			}},
			{[onError](Mtb::CmdError cmdError, void*) { onError(cmdError); }}
		)
	);
}

} // namespace Mtb
