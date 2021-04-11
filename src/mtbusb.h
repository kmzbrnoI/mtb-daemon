#ifndef MTBUSB_H
#define MTBUSB_H

/* Low-level access to MTB-USB module via CDC serial port. */

#include <QObject>
#include <QSerialPort>
#include <QTimer>

namespace Mtb {

constexpr size_t _MAX_HISTORY_LEN = 32;
constexpr size_t _HIST_CHECK_INTERVAL = 100; // ms
constexpr size_t _HIST_TIMEOUT = 300; // ms
constexpr size_t _HIST_SEND_MAX = 3;
constexpr size_t _BUF_IN_TIMEOUT = 50; // ms
constexpr size_t _MAX_HIST_BUF_COUNT = 3;
constexpr size_t _OUT_TIMER_INTERVAL = 20; // 20 ms

struct EOpenError : public std::logic_error {
	EOpenError(const std::string& str) : logic_error(str) {}
	EOpenError(const QString& str) : logic_error(str.toStdString()) {}
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

class MtbUsb : public QObject {
	Q_OBJECT

public:
	LogLevel loglevel = LogLevel::None;

	MtbUsb(QObject *parent = nullptr);

	void connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc);
	void disconnect();
	bool connected() const;

private slots:
	void sp_handle_ready_read();
	void sp_handle_error(QSerialPort::SerialPortError);
	void sp_about_to_close();
	void m_hist_timer_tick();
	void m_out_timer_tick();

signals:
	void onLog(QString message, Mtb::LogLevel loglevel);
	void onConnect();
	void onDisconnect();

private:
	QSerialPort m_serialPort;
	QByteArray m_readData;
	QTimer m_hist_timer;
	QTimer m_out_timer;

	void log(const QString &message, LogLevel loglevel);

};

};

#endif
