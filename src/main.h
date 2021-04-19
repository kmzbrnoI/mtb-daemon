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

void log(const QString&, Mtb::LogLevel);

class DaemonCoreApplication : public QCoreApplication {
	Q_OBJECT
public:
	DaemonCoreApplication(int &argc, char **argv);
	virtual ~DaemonCoreApplication() {};

private:
	void sendStatus(QTcpSocket&, std::optional<size_t> id);
	void mtbUsbGotInfo();
	void mtbUsbDidNotGetInfo(Mtb::CmdError);
	void mtbUsbGotModules();
	void mtbUsbDidNotGetModules(Mtb::CmdError);

	void activateModule(uint8_t addr);
	void moduleGotInfo(uint8_t addr, Mtb::ModuleInfo);
	void moduleDidNotGetInfo();

	void loadConfig(const QString& filename);
	void saveConfig(const QString& filename);

private slots:
	void mtbUsbLog(QString message, Mtb::LogLevel loglevel);
	void mtbUsbConnect();
	void mtbUsbDisconnect();
	void mtbUsbNewModule(uint8_t addr);
	void mtbUsbModuleFail(uint8_t addr);
	void mtbUsbInputsChange(uint8_t addr, const std::vector<uint8_t>& data);

	void serverReceived(QTcpSocket*, const QJsonObject&);
};

#endif
