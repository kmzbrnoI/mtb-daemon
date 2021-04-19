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

const QString DEFAULT_CONFIG_FILENAME = "mtb-daemon.json";

void log(const QString&, Mtb::LogLevel);

class DaemonCoreApplication : public QCoreApplication {
	Q_OBJECT
public:
	static Mtb::LogLevel loglevel;

	DaemonCoreApplication(int &argc, char **argv);
	virtual ~DaemonCoreApplication() {};

	static void log(const QString&, Mtb::LogLevel);

private:
	QJsonObject config;
	QString configFileName;

	void sendStatus(QTcpSocket&, std::optional<size_t> id);
	void mtbUsbGotInfo();
	void mtbUsbDidNotGetInfo(Mtb::CmdError);
	void mtbUsbGotModules();
	void mtbUsbDidNotGetModules(Mtb::CmdError);

	void activateModule(uint8_t addr);
	void moduleGotInfo(uint8_t addr, Mtb::ModuleInfo);
	void moduleDidNotGetInfo();

	bool loadConfig(const QString& filename);
	bool saveConfig(const QString& filename);

	void mtbUsbConnect();

private slots:
	void mtbUsbOnLog(QString message, Mtb::LogLevel loglevel);
	void mtbUsbOnConnect();
	void mtbUsbOnDisconnect();
	void mtbUsbOnNewModule(uint8_t addr);
	void mtbUsbOnModuleFail(uint8_t addr);
	void mtbUsbOnInputsChange(uint8_t addr, const std::vector<uint8_t>& data);

	void serverReceived(QTcpSocket*, const QJsonObject&);
};

#endif
