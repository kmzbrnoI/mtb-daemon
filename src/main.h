#ifndef MAIN_H
#define MAIN_H

#include <QCoreApplication>
#include <QTcpSocket>
#include "mtbusb/mtbusb.h"
#include "server.h"
#include "modules/module.h"

extern Mtb::MtbUsb mtbusb;
extern DaemonServer server;
extern std::array<std::unique_ptr<MtbModule>, Mtb::_MAX_MODULES> modules;
extern std::array<std::map<QTcpSocket*, bool>, Mtb::_MAX_MODULES> subscribes;

class DaemonCoreApplication : public QCoreApplication {
	Q_OBJECT
public:
	DaemonCoreApplication(int &argc, char **argv);
	virtual ~DaemonCoreApplication() {};

private:
	void sendStatus(QTcpSocket&, std::optional<size_t> id);

private slots:
	void mtbUsbLog(QString message, Mtb::LogLevel loglevel);
	void serverReceived(QTcpSocket*, const QJsonObject&);
};

#endif
