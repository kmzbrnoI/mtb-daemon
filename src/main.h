#ifndef MAIN_H
#define MAIN_H

#include <QCoreApplication>
#include <QTcpSocket>
#include <map>
#include <array>
#include "mtbusb.h"
#include "server.h"
#include "module.h"

extern Mtb::MtbUsb mtbusb;
extern DaemonServer server;
extern std::array<std::unique_ptr<MtbModule>, Mtb::_MAX_MODULES> modules;
extern std::array<std::map<QTcpSocket*, bool>, Mtb::_MAX_MODULES> subscribes;

constexpr size_t T_RECONNECT_PERIOD = 1000; // 1 s
constexpr size_t T_REACTIVATE_PERIOD = 500; // 500 ms

const QString DEFAULT_CONFIG_FILENAME = "mtb-daemon.json";

std::vector<QTcpSocket*> outputSetters();

struct ConfigNotFound : public std::logic_error {
	ConfigNotFound(const std::string &str) : std::logic_error(str) {}
	ConfigNotFound(const QString &str) : logic_error(str.toStdString()) {}
};

struct FileWriteError : public std::logic_error {
	FileWriteError(const std::string &str) : std::logic_error(str) {}
	FileWriteError(const QString &str) : logic_error(str.toStdString()) {}
};

struct JsonParseError : public std::logic_error {
	JsonParseError(const std::string &str) : std::logic_error(str) {}
	JsonParseError(const QString &str) : logic_error(str.toStdString()) {}
};


enum class StartupError {
	Ok = 0,
	ConfigLoad = 1,
	ServerStart = 2,
};

class DaemonCoreApplication : public QCoreApplication {
	Q_OBJECT
public:
	DaemonCoreApplication(int &argc, char **argv);
	~DaemonCoreApplication() override = default;

	bool hasWriteAccess(const QTcpSocket*);
	StartupError startupError() const { return startError; }

private:
	QJsonObject config;
	QString configFileName;
	QTimer t_reconnect;
	QTimer t_reactivate;
	std::vector<QHostAddress> writeAccess;
	StartupError startError = StartupError::Ok;

	QJsonObject mtbUsbJson() const;
	void mtbUsbProperSpeedSet();
	void mtbUsbGotInfo();
	void mtbUsbGotModules();

	void activateModule(uint8_t addr, size_t attemptsRemaining = 5);
	void moduleGotInfo(uint8_t addr, Mtb::ModuleInfo);
	void moduleDidNotGetInfo();
	static std::unique_ptr<MtbModule> newModule(size_t type, uint8_t addr);

	void loadConfig(const QString &filename);
	void saveConfig(const QString &filename);

	void mtbUsbConnect();

	void clientResetOutputs(QTcpSocket*, std::function<void()> onOk,
	                        std::function<void()> onError);

	void serverCmdMtbusb(QTcpSocket*, const QJsonObject&);
	void serverCmdVersion(QTcpSocket*, const QJsonObject&);
	void serverCmdSaveConfig(QTcpSocket*, const QJsonObject&);
	void serverCmdLoadConfig(QTcpSocket*, const QJsonObject&);
	void serverCmdModule(QTcpSocket*, const QJsonObject&);
	void serverCmdModuleDelete(QTcpSocket*, const QJsonObject&);
	void serverCmdModules(QTcpSocket*, const QJsonObject&);
	void serverCmdModuleSubscribe(QTcpSocket*, const QJsonObject&);
	void serverCmdModuleUnsubscribe(QTcpSocket*, const QJsonObject&);
	void serverCmdModuleSetConfig(QTcpSocket*, const QJsonObject&);
	void serverCmdModuleSpecificCommand(QTcpSocket*, const QJsonObject&);
	void serverCmdSetAddress(QTcpSocket*, const QJsonObject&);
	void serverCmdResetMyOutputs(QTcpSocket*, const QJsonObject&);

private slots:
	void mtbUsbOnLog(QString message, Mtb::LogLevel loglevel);
	void mtbUsbOnConnect();
	void mtbUsbOnDisconnect();
	void mtbUsbOnNewModule(uint8_t addr);
	void mtbUsbOnModuleFail(uint8_t addr);
	void mtbUsbOnInputsChange(uint8_t addr, const std::vector<uint8_t> &data);
	void mtbUsbOnDiagStateChange(uint8_t addr, const std::vector<uint8_t> &data);

	void serverReceived(QTcpSocket*, const QJsonObject&);
	void serverClientDisconnected(QTcpSocket*);

	void tReconnectTick();
	void tReactivateTick();
};

#endif
