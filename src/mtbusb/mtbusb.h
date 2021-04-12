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

#include "mtbusb-commands.h"

namespace Mtb {

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

	std::unique_ptr<const Cmd> cmd;
	QDateTime timeout;
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

private:
	QSerialPort m_serialPort;
	QByteArray m_readData;
	QTimer m_histTimer;
	QTimer m_outTimer;
	std::deque<HistoryItem> m_hist;
	std::deque<std::unique_ptr<const Cmd>> m_out;
	QDateTime m_lastSent;
	QDateTime m_receiveTimeout;

	void log(const QString &message, LogLevel loglevel);

	using MsgType = std::vector<uint8_t>;
	void parseMessage(MsgType &msg);
	void send(MsgType);
	void sendNextOut();

	void write(std::unique_ptr<const Cmd> cmd);
	void send(std::unique_ptr<const Cmd> &cmd, bool bypass_m_out_emptiness = false);

	bool conflictWithHistory(const Cmd &) const;
	bool conflictWithOut(const Cmd &) const;
};

// Templated functions must be in header file to compile

template <typename T>
void MtbUsb::send(const T &&cmd) {
	std::unique_ptr<const Cmd> cmd2(std::make_unique<const T>(cmd));
	send(cmd2);
}

};

#endif
