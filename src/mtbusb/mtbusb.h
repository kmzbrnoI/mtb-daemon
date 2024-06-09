#ifndef _MTBUSB_H_
#define _MTBUSB_H_

/* Low-level access to MTB-USB module via CDC serial port. */

#include <QDateTime>
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <functional>
#include <memory>
#include <optional>
#include <queue>

#include "mtbusb-commands.h"

namespace Mtb {

constexpr size_t _MAX_MODULES = 256;
constexpr size_t _PENDING_CHECK_INTERVAL = 100; // ms
constexpr size_t _PENDING_TIMEOUT = 300; // ms
constexpr size_t _PENDING_RESEND_MAX = 3;
constexpr size_t _BUF_IN_TIMEOUT = 50; // ms
constexpr size_t _MAX_PENDING = 3; // maximum number of commands waiting for response
constexpr size_t _PING_SEND_PERIOD_MS = 5000;

struct EOpenError : public MtbUsbError {
	EOpenError(const std::string &str) : MtbUsbError(str) {}
	EOpenError(const QString &str) : MtbUsbError(str) {}
};

struct EWriteError : public MtbUsbError {
	EWriteError(const std::string &str) : MtbUsbError(str) {}
};

enum class LogLevel {
	None = 0,
	Error = 1,
	Warning = 2,
	Info = 3,
	Commands = 4,
	RawData = 5,
	Debug = 6,
};

QString flowControlToStr(QSerialPort::FlowControl);

template <typename DataT, typename ItemType>
QString dataToStr(DataT data, size_t len = 0) {
	QString out;
	size_t i = 0;
	for (auto d = data.begin(); (d != data.end() && (len == 0 || i < len)); d++, i++)
		out += "0x" +
		       QString("%1 ").arg(static_cast<ItemType>(*d), 2, 16, QLatin1Char('0')).toUpper();
	return out.trimmed();
}

// PendingCmd represents a command sent to the MTB-USB, for which the response
// has not arrived yet.
struct PendingCmd {
	PendingCmd(std::unique_ptr<const Cmd> &cmd, QDateTime timeout, size_t no_sent)
	    : cmd(std::move(cmd))
	    , timeout(timeout)
		, no_sent(no_sent) {}
	PendingCmd(PendingCmd &&pending) noexcept
	    : cmd(std::move(pending.cmd))
	    , timeout(pending.timeout)
		, no_sent(pending.no_sent) {}
	PendingCmd& operator=(PendingCmd &&pending) {
		cmd = std::move(pending.cmd);
		timeout = pending.timeout;
		no_sent = pending.no_sent;
		return *this;
	}

	std::unique_ptr<const Cmd> cmd;
	QDateTime timeout; // timeout for response
	size_t no_sent = 0; // how many times this command was resent (for calculating of giving-up)
};

struct MtbUsbInfo {
	uint8_t type;
	MtbBusSpeed speed;
	uint8_t fw_major;
	uint8_t fw_minor;
	uint8_t proto_major;
	uint8_t proto_minor;

	QString fw_version() const { return QString::number(fw_major) + "." + QString::number(fw_minor); }
	QString proto_version() const { return QString::number(proto_major) + "." + QString::number(proto_minor); }
	uint16_t fw_raw() const { return (fw_major << 8) | fw_minor; }
	bool fw_deprecated() const { return (fw_raw() < 0x0103); }
};


class MtbUsb : public QObject {
	Q_OBJECT

public:
	LogLevel loglevel = LogLevel::None;
	bool ping = true;

	MtbUsb(QObject *parent = nullptr);

	void connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc);
	void disconnect();
	bool connected() const;

	template <typename T>
	void send(const T &&cmd);

	std::optional<MtbUsbInfo> mtbUsbInfo() const { return m_mtbUsbInfo; }
	std::optional<std::array<bool, _MAX_MODULES>> activeModules() const { return m_activeModules; }

	void changeSpeed(MtbBusSpeed, std::function<void()> onOk, std::function<void(Mtb::CmdError)> onError);

	static std::vector<QSerialPortInfo> ports();

private slots:
	void spHandleReadyRead();
	void spHandleError(QSerialPort::SerialPortError);
	void spAboutToClose();
	void pendingTimerTick();
	void pingTimerTick();

signals:
	void onLog(QString message, Mtb::LogLevel loglevel);
	void onConnect();
	void onDisconnect();

	void onNewModule(uint8_t addr);
	void onModuleFail(uint8_t addr);
	void onModuleInputsChange(uint8_t addr, const std::vector<uint8_t> &data);
	void onModuleDiagStateChange(uint8_t addr, const std::vector<uint8_t> &data);

private:
	QSerialPort m_serialPort;
	QByteArray m_readData;
	QTimer m_pendingTimer;
	QTimer m_pingTimer;
	std::deque<PendingCmd> m_pending;
	std::deque<std::unique_ptr<const Cmd>> m_out;
	QDateTime m_receiveTimeout;
	std::optional<MtbUsbInfo> m_mtbUsbInfo;
	std::optional<std::array<bool, _MAX_MODULES>> m_activeModules;

	void log(const QString &message, LogLevel loglevel);

	void parseMtbUsbMessage(uint8_t command_code, const std::vector<uint8_t> &data);
	void parseMtbBusMessage(uint8_t module, uint8_t attempts, uint8_t command_code, const std::vector<uint8_t> &data);
	void send(std::vector<uint8_t>);
	void sendNextOut();

	void write(std::unique_ptr<const Cmd> cmd, size_t no_sent = 1);
	void send(std::unique_ptr<const Cmd> &cmd, bool bypass_m_out_emptiness = false);

	bool conflictWithPending(const Cmd &) const;
	bool conflictWithOut(const Cmd &) const;

	void handleMtbUsbError(uint8_t code, uint8_t out_command_code, uint8_t addr);
	void handleMtbBusError(uint8_t errorCode, uint8_t addr);
	void pendingTimeoutError(CmdError, size_t i = 0);
	void pendingResend();
};

// Templated functions must be in header file to compile

template <typename T>
void MtbUsb::send(const T &&cmd) {
	std::unique_ptr<const Cmd> cmd2(std::make_unique<const T>(cmd));
	send(cmd2);
}

} // namespace Mtb

#endif
