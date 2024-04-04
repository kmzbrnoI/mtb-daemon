#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <deque>
#include <fstream>
#include <memory>
#include "mtbusb.h"

void log(const QString&, Mtb::LogLevel);

struct LogRecord {
	QDateTime time;
	QString message;
	Mtb::LogLevel loglevel;

	LogRecord(const QString& message, Mtb::LogLevel loglevel)
    : time(QDateTime::currentDateTime()), message(message), loglevel(loglevel) {}
};

std::ofstream& operator<<(std::ofstream&, const LogRecord&);

class Logger {
public:
	void loadConfig(const QJsonObject& config);
	void log(const QString&, Mtb::LogLevel);

private:
	struct Prod {
		bool enabled = false;
		Mtb::LogLevel loglevel;
		size_t history;
		size_t future;
		QString directory;
		Mtb::LogLevel detectLevel;

		size_t remaining = 0;
		std::unique_ptr<std::ofstream> file;
		bool active() const { return this->remaining > 0; }
	};

	Mtb::LogLevel loglevel = Mtb::LogLevel::Info;
	Prod prod;
	std::deque<LogRecord> logHistory;

	void prodLog(const QString&, Mtb::LogLevel);
	void prodInit();
	void termLog(const QString&, Mtb::LogLevel);
};

extern Logger logger;

#endif
