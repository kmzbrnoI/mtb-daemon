#ifndef MAIN_H
#define MAIN_H

#include <QCoreApplication>
#include <QTcpSocket>
#include "mtbusb/mtbusb.h"

extern Mtb::MtbUsb mtbusb;

class DaemonCoreApplication : public QCoreApplication {
	Q_OBJECT
public:
	DaemonCoreApplication(int &argc, char **argv);
	virtual ~DaemonCoreApplication() {};

private:
	void sendStatus(QTcpSocket&, std::optional<size_t> id);

private slots:
	void mtbUsbLog(QString message, Mtb::LogLevel loglevel);
	void serverReceived(QTcpSocket&, const QJsonObject&);
};

#endif
