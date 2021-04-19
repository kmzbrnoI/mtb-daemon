#ifndef MTBUSB_H
#define MTBUSB_H

/* Low-level access to MTB-USB module via CDC serial port. */

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QDateTime>
#include <queue>
#include <functional>
#include <memory>
#include <optional>

#include "mtbusb-commands.h"

namespace Mtb {

constexpr size_t _MAX_MODULES = 256;
constexpr size_t _MAX_HISTORY_LEN = 32;
constexpr size_t _HIST_CHECK_INTERVAL = 100; // ms
constexpr size_t _HIST_TIMEOUT = 300; // ms
constexpr size_t _HIST_SEND_MAX = 3;
constexpr size_t _BUF_IN_TIMEOUT = 50; // ms
constexpr size_t _MAX_HIST_BUF_COUNT = 3;
constexpr size_t _OUT_TIMER_INTERVAL = 20; // 20 ms

struct EOpenError : public MtbUsbError {
	EOpenError(const std::string& str) : MtbUsbError(str) {}
	EOpenError(const QString& str) : MtbUsbError(str) {}
};

struct EWriteError : public MtbUsbError {
	EWriteError(const std::string& str) : MtbUsbError(str) {}
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

struct HistoryItem {
	HistoryItem(std::unique_ptr<const Cmd> &cmd, QDateTime timeout)
	    : cmd(std::move(cmd))
	    , timeout(timeout) {}
	HistoryItem(HistoryItem &&hist) noexcept
	    : cmd(std::move(hist.cmd))
	    , timeout(hist.timeout) {}
	HistoryItem& operator=(HistoryItem &&hist) {
		cmd = std::move(hist.cmd);
		timeout = hist.timeout;
		return *this;
	}

	std::unique_ptr<const Cmd> cmd;
	QDateTime timeout;
};

struct MtbUsbInfo {
	uint8_t type;
	MtbBusSpeed speed;
	uint8_t fw_major;
	uint8_t fw_minor;
	uint8_t proto_major;
	uint8_t proto_minor;

	QString fw_version() const { return QString::number(fw_major)+"."+QString::number(fw_minor); }
	QString proto_version() const { return QString::number(proto_major)+"."+QString::number(proto_minor); }
};


class MtbUsb : public QObject {
	Q_OBJECT

public:
	LogLevel loglevel = LogLevel::None;

	MtbUsb(QObject *parent = nullptr);

	void connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc);
	void disconnect();
	bool connected() const;

	template <typename T>
	void send(const T &&cmd);

	std::optional<MtbUsbInfo> mtbUsbInfo() const { return m_mtbUsbInfo; }
	std::optional<std::array<bool, _MAX_MODULES>> activeModules() const { return m_activeModules; }

	static std::vector<QSerialPortInfo> ports();

private slots:
	void spHandleReadyRead();
	void spHandleError(QSerialPort::SerialPortError);
	void spAboutToClose();
	void histTimerTick();
	void outTimerTick();

signals:
	void onLog(QString message, Mtb::LogLevel loglevel);
	void onConnect();
	void onDisconnect();

	void onNewModule(uint8_t addr);
	void onModuleFail(uint8_t addr);
	void onModuleInputsChange(uint8_t addr, const std::vector<uint8_t>& data);

private:
	QSerialPort m_serialPort;
	QByteArray m_readData;
	QTimer m_histTimer;
	QTimer m_outTimer;
	std::deque<HistoryItem> m_hist;
	std::deque<std::unique_ptr<const Cmd>> m_out;
	QDateTime m_lastSent;
	QDateTime m_receiveTimeout;
	std::optional<MtbUsbInfo> m_mtbUsbInfo;
	std::optional<std::array<bool, _MAX_MODULES>> m_activeModules;

	void log(const QString &message, LogLevel loglevel);

	void parseMtbUsbMessage(uint8_t command_code, const std::vector<uint8_t> &data);
	void parseMtbBusMessage(uint8_t module, uint8_t attempts, uint8_t command_code, const std::vector<uint8_t> &data);
	void send(std::vector<uint8_t>);
	void sendNextOut();

	void write(std::unique_ptr<const Cmd> cmd);
	void send(std::unique_ptr<const Cmd> &cmd, bool bypass_m_out_emptiness = false);

	bool conflictWithHistory(const Cmd &) const;
	bool conflictWithOut(const Cmd &) const;

	void handleMtbUsbError(uint8_t code, uint8_t out_command_code, uint8_t addr);
	void handleMtbBusError(uint8_t errorCode, uint8_t addr);
	void histTimeoutError(CmdError, size_t i = 0);
};

// Templated functions must be in header file to compile

template <typename T>
void MtbUsb::send(const T &&cmd) {
	std::unique_ptr<const Cmd> cmd2(std::make_unique<const T>(cmd));
	send(cmd2);
}

};

#endif
