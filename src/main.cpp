#include <QSerialPort>
#include <QSerialPortInfo>
#include <QJsonArray>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <iostream>
#include "main.h"
#include "mtbusb/mtbusb-common.h"
#include "modules/uni.h"
#include "errors.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Mtb::MtbUsb mtbusb;
DaemonServer server;
std::array<std::unique_ptr<MtbModule>, Mtb::_MAX_MODULES> modules;
std::array<std::map<QTcpSocket*, bool>, Mtb::_MAX_MODULES> subscribes;
Mtb::LogLevel DaemonCoreApplication::loglevel = Mtb::LogLevel::Info;

#ifdef Q_OS_WIN
static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType);
#endif

int main(int argc, char *argv[]) {
	DaemonCoreApplication a(argc, argv);
#ifdef Q_OS_WIN
	SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif
	return a.exec();
}

DaemonCoreApplication::DaemonCoreApplication(int &argc, char **argv)
     : QCoreApplication(argc, argv) {
	QObject::connect(&server, SIGNAL(jsonReceived(QTcpSocket*, const QJsonObject&)),
	                 this, SLOT(serverReceived(QTcpSocket*, const QJsonObject&)));
	QObject::connect(&server, SIGNAL(clientDisconnected(QTcpSocket*)),
	                 this, SLOT(serverClientDisconnected(QTcpSocket*)));

	QObject::connect(&t_reconnect, SIGNAL(timeout()), this, SLOT(tReconnectTick()));
	QObject::connect(&t_reactivate, SIGNAL(timeout()), this, SLOT(tReactivateTick()));

	QObject::connect(&mtbusb, SIGNAL(onLog(QString, Mtb::LogLevel)),
	                 this, SLOT(mtbUsbOnLog(QString, Mtb::LogLevel)));
	QObject::connect(&mtbusb, SIGNAL(onConnect()), this, SLOT(mtbUsbOnConnect()));
	QObject::connect(&mtbusb, SIGNAL(onDisconnect()), this, SLOT(mtbUsbOnDisconnect()));
	QObject::connect(&mtbusb, SIGNAL(onNewModule(uint8_t)), this, SLOT(mtbUsbOnNewModule(uint8_t)));
	QObject::connect(&mtbusb, SIGNAL(onModuleFail(uint8_t)), this, SLOT(mtbUsbOnModuleFail(uint8_t)));
	QObject::connect(&mtbusb, SIGNAL(onModuleInputsChange(uint8_t, const std::vector<uint8_t>&)),
	                 this, SLOT(mtbUsbOnInputsChange(uint8_t, const std::vector<uint8_t>&)));

	{ // Load config file
		this->configFileName = (argc > 1) ? argv[1] : DEFAULT_CONFIG_FILENAME;
		bool configLoaded = this->loadConfig(this->configFileName);
		if (!configLoaded) {
			log("Unable to load config file "+configFileName+", resetting config, writing new config file...",
				Mtb::LogLevel::Info);
			this->config = QJsonObject{
				{"loglevel", static_cast<int>(Mtb::LogLevel::Info)},
				{"server", QJsonObject{
					{"host", "127.0.0.1"},
					{"port", static_cast<int>(SERVER_DEFAULT_PORT)},
					{"keepAlive", true},
				}},
				{"mtb-usb", QJsonObject{
					{"port", "auto"},
					{"keepAlive", true},
				}},
			};
			this->saveConfig(configFileName);
		} else {
			log("Config file "+configFileName+" successfully loaded.", Mtb::LogLevel::Info);
		}
	}

	Mtb::LogLevel loglevel = static_cast<Mtb::LogLevel>(this->config["loglevel"].toInt());
	mtbusb.loglevel = loglevel;
	mtbusb.ping = this->config["mtb-usb"].toObject()["keepAlive"].toBool(true);
	DaemonCoreApplication::loglevel = loglevel;

	{ // Start server
		const QJsonObject serverConfig = this->config["server"].toObject();
		size_t port = serverConfig["port"].toInt();
		bool keepAlive = serverConfig["keepAlive"].toBool(true);
		QHostAddress host(serverConfig["host"].toString());
		log("Starting server: "+host.toString()+":"+QString::number(port)+"...", Mtb::LogLevel::Info);
		server.listen(host, port, keepAlive);
	}

	this->mtbUsbConnect();
	if (!mtbusb.connected())
		this->t_reconnect.start(T_RECONNECT_PERIOD);
	this->t_reactivate.start(T_REACTIVATE_PERIOD);
}

/* MTB-USB handling ----------------------------------------------------------*/

void DaemonCoreApplication::mtbUsbConnect() {
	const QJsonObject mtbUsbConfig = this->config["mtb-usb"].toObject();
	QString port = mtbUsbConfig["port"].toString();

	if (port == "auto") {
		const std::vector<QSerialPortInfo> &mtbUsbPorts = Mtb::MtbUsb::ports();
		log("Automatic MTB-USB port detected", Mtb::LogLevel::Info);

#ifdef Q_OS_WIN
		log("Automatic MTB-USB port detection on Windows probably won't work, you need to specify COM port manually",
			Mtb::LogLevel::Warning);
#endif

		if (mtbUsbPorts.size() == 1) {
			log("Found single port "+mtbUsbPorts[0].portName(), Mtb::LogLevel::Info);
			port = mtbUsbPorts[0].portName();
		} else {
			log("Found "+QString::number(mtbUsbPorts.size())+" MTB-USB modules. Not connecting to any.",
			    Mtb::LogLevel::Warning);
			return;
		}
	}

	try {
		mtbusb.connect(port, 115200, QSerialPort::FlowControl::NoFlowControl);
	} catch (const Mtb::EOpenError&) {}
}

void log(const QString &message, Mtb::LogLevel loglevel) {
	DaemonCoreApplication::log(message, loglevel);
}

void DaemonCoreApplication::log(const QString &message, Mtb::LogLevel loglevel) {
	if (loglevel > DaemonCoreApplication::loglevel)
		return;

	std::cout << "[" << QTime::currentTime().toString("hh:mm:ss,zzz").toStdString() << "] ";
	switch (loglevel) {
		case Mtb::LogLevel::Error: std::cout << "[ERROR] "; break;
		case Mtb::LogLevel::Warning: std::cout << "[warning] "; break;
		case Mtb::LogLevel::Info: std::cout << "[info] "; break;
		case Mtb::LogLevel::Commands: std::cout << "[command] "; break;
		case Mtb::LogLevel::RawData: std::cout << "[raw-data] "; break;
		case Mtb::LogLevel::Debug: std::cout << "[debug] "; break;
		default: break;
	}
	std::cout << message.toStdString() << std::endl;
}

void DaemonCoreApplication::mtbUsbOnLog(QString message, Mtb::LogLevel loglevel) {
	log(message, loglevel);
}

void DaemonCoreApplication::mtbUsbOnConnect() {
	mtbusb.send(
		Mtb::CmdMtbUsbInfoRequest(
			{[this](void*) { this->mtbUsbGotInfo(); }},
			{[this](Mtb::CmdError cmdError, void*) { this->mtbUsbDidNotGetInfo(cmdError); }}
		)
	);
}

void DaemonCoreApplication::mtbUsbGotInfo() {
	mtbusb.send(
		Mtb::CmdMtbUsbActiveModulesRequest(
			{[this](void*) { this->mtbUsbGotModules(); }},
			{[this](Mtb::CmdError cmdError, void*) { this->mtbUsbDidNotGetModules(cmdError); }}
		)
	);
}

void DaemonCoreApplication::mtbUsbDidNotGetInfo(Mtb::CmdError) {
	log("Did not get info from MTB-USB, disconnecting...", Mtb::LogLevel::Error);
	mtbusb.disconnect();
}

void DaemonCoreApplication::mtbUsbGotModules() {
	server.broadcast({
		{"command", "mtbusb"},
		{"type", "event"},
		{"mtbusb", this->mtbUsbJson()},
	});

	const auto activeModules = mtbusb.activeModules().value();

	{ // Logging
		size_t count = 0;
		for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
			if (activeModules[i])
				count++;
		QString message = "Got "+QString::number(count)+" active modules";
		if (count > 0)
			message += ", activating...";
		log(message, Mtb::LogLevel::Info);
	}

	for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
		if (activeModules[i])
			this->activateModule(i);
}

void DaemonCoreApplication::activateModule(uint8_t addr, size_t attemptsRemaining) {
	log("New module "+QString::number(addr)+" discovered, activating...", Mtb::LogLevel::Info);
	mtbusb.send(
		Mtb::CmdMtbModuleInfoRequest(
			addr,
			{[this](uint8_t addr, Mtb::ModuleInfo info, void*) { this->moduleGotInfo(addr, info); }},
			{[this, addr, attemptsRemaining](Mtb::CmdError, void*) {
				log("Did not get info from newly discovered module, module keeps disabled.",
				    Mtb::LogLevel::Error);
				if (attemptsRemaining > 0) {
					QTimer::singleShot(500, [this, addr, attemptsRemaining]() {
						if ((modules[addr] == nullptr) || (!modules[addr]->isActive() && !modules[addr]->isActivating()))
							this->activateModule(addr, attemptsRemaining-1);
					});
				}
			}}
		)
	);
}

void DaemonCoreApplication::moduleGotInfo(uint8_t addr, Mtb::ModuleInfo info) {
	if ((info.type&0xF0) == 0x10) {
		if (modules[addr] == nullptr) {
			modules[addr] = std::make_unique<MtbUni>(addr);
		} else {
			if (static_cast<size_t>(modules[addr]->moduleType()) != info.type) {
				log("Detected module "+QString::number(addr)+" type & stored module type mismatch! Forgetting config...",
				    Mtb::LogLevel::Warning);
				modules[addr] = std::make_unique<MtbUni>(addr);
			}
		}
	} else {
		log("Unknown module type: "+QString::number(addr)+": 0x"+
			QString::number(info.type, 16)+"!", Mtb::LogLevel::Warning);
		modules[addr] = std::make_unique<MtbModule>(addr);
	}

	modules[addr]->mtbBusActivate(info);
}

void DaemonCoreApplication::mtbUsbDidNotGetModules(Mtb::CmdError) {
	log("Did not get active modules from MTB-USB, disconnecting...", Mtb::LogLevel::Info);
	mtbusb.disconnect();
}

void DaemonCoreApplication::mtbUsbOnDisconnect() {
	server.broadcast({
		{"command", "mtbusb"},
		{"type", "event"},
		{"mtbusb", this->mtbUsbJson()},
	});

	for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
		if (modules[i] != nullptr)
			modules[i]->mtbUsbDisconnected();

	this->t_reconnect.start(T_RECONNECT_PERIOD);
}

void DaemonCoreApplication::mtbUsbOnNewModule(uint8_t addr) {
	if ((modules[addr] == nullptr) || ((!modules[addr]->isActive()) && (!modules[addr]->isRebooting())))
		this->activateModule(addr);
}

void DaemonCoreApplication::mtbUsbOnModuleFail(uint8_t addr) {
	if (modules[addr] != nullptr)
		modules[addr]->mtbBusLost();
}

void DaemonCoreApplication::mtbUsbOnInputsChange(uint8_t addr, const std::vector<uint8_t> &data) {
	if (modules[addr] != nullptr)
		modules[addr]->mtbBusInputsChanged(data);
}

void DaemonCoreApplication::tReconnectTick() {
	if (mtbusb.connected())
		this->t_reconnect.stop();

	const QJsonObject mtbUsbConfig = this->config["mtb-usb"].toObject();
	QString port = mtbUsbConfig["port"].toString();
	const std::vector<QSerialPortInfo> &mtbUsbPorts = Mtb::MtbUsb::ports();

	if (port == "auto") {
		if (mtbUsbPorts.size() != 1)
			return;
	} else {
		bool found = false;
		for (const QSerialPortInfo &portInfo :  mtbUsbPorts)
			if (portInfo.portName() == port)
				found = true;
		if (!found)
			return;
	}

	log("MTB-USB discovered, trying to reconnect...", Mtb::LogLevel::Info);
	this->mtbUsbConnect();
	if (mtbusb.connected())
		this->t_reconnect.stop();
}

void DaemonCoreApplication::tReactivateTick() {
	for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
		if (modules[i] != nullptr)
			modules[i]->reactivateCheck();
}

/* JSON server handling ------------------------------------------------------*/

void DaemonCoreApplication::serverReceived(QTcpSocket *socket, const QJsonObject &request) {
	QString command = request["command"].toString();
	std::optional<size_t> id;
	if (request.contains("id"))
		id = request["id"].toInt();

	if (command == "mtbusb") {
		if (request.contains("mtbusb")) { // Changing MTB-USB
			QJsonObject jsonMtbUsb = request["mtbusb"].toObject();
			if (jsonMtbUsb.contains("speed")) { // Change MTBbus speed
				if (!mtbusb.connected() || !mtbusb.mtbUsbInfo().has_value()) {
					sendError(socket, request, MTB_DEVICE_DISCONNECTED, "Disconnected from MTB-USB!");
					return;
				}
				size_t speed = jsonMtbUsb["speed"].toInt();
				if (!Mtb::mtbBusSpeedValid(speed)) {
					sendError(socket, request, MTB_INVALID_SPEED, "Invalid MTBbus speed!");
					return;
				}
				Mtb::MtbBusSpeed mtbUsbSpeed = mtbusb.mtbUsbInfo().value().speed;
				Mtb::MtbBusSpeed newSpeed = Mtb::intToMtbBusSpeed(speed);
				if (mtbUsbSpeed != newSpeed) {
					mtbusb.changeSpeed(
						newSpeed,
						{[this, socket, request]() {
							QJsonObject response = jsonOkResponse(request);
							response["mtbusb"] = this->mtbUsbJson();
							server.send(socket, response);
						}},
						{[socket, request](Mtb::CmdError error) { sendError(socket, request, error); }}
					);
					return;
				}
			}
		}

		QJsonObject response = jsonOkResponse(request);
		response["mtbusb"] = this->mtbUsbJson();
		server.send(socket, response);

	} else if (command == "save_config") {
		QString filename = this->configFileName;
		if (request.contains("filename"))
			filename = request["filename"].toString();
		bool ok = this->saveConfig(filename);
		QJsonObject response {
			{"command", "save_config"},
			{"type", "response"},
			{"status", ok ? "ok" : "error"},
		};
		if (!ok)
			response["error"] = jsonError(MTB_FILE_CANNOT_ACCESS, "Cannot access file "+filename);
		if (id)
			response["id"] = static_cast<int>(id.value());

		server.send(socket, response);

	} else if (command == "module") {
		QJsonObject response = jsonOkResponse(request);

		size_t addr = request["address"].toInt();
		if ((Mtb::isValidModuleAddress(addr)) && (modules[addr] != nullptr)) {
			response["module"] = modules[addr]->moduleInfo(request["state"].toBool(), true);
			response["status"] = "ok";
		} else {
			response["status"] = "error";
			response["error"] = DaemonServer::error(MTB_MODULE_INVALID_ADDR, "Invalid module address");
		}

		server.send(socket, response);

	} else if (command == "modules") {
		QJsonObject response = jsonOkResponse(request);
		QJsonObject jsonModules;

		for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
			if (modules[i] != nullptr)
				jsonModules[QString::number(i)] = modules[i]->moduleInfo(request["state"].toBool(), true);
		}
		response["modules"] = jsonModules;

		server.send(socket, response);

	} else if (command == "module_subscribe") {
		QJsonObject response = jsonOkResponse(request);

		QJsonArray addresses;
		for (const auto &value : request["addresses"].toArray()) {
			size_t addr = value.toInt();
			if (Mtb::isValidModuleAddress(addr)) {
				subscribes[addr].insert_or_assign(socket, true);
				addresses.push_back(static_cast<int>(addr));
			} else {
				response["status"] = "error";
				response["error"] = DaemonServer::error(MTB_MODULE_INVALID_ADDR, "Invalid module address");
				break;
			}
		}

		response["addresses"] = addresses;
		server.send(socket, response);

	} else if (command == "module_unsubscribe") {
		QJsonObject response = jsonOkResponse(request);

		for (const auto &value : response["addresses"].toArray()) {
			size_t addr = value.toInt();
			if (Mtb::isValidModuleAddress(addr)) {
				if (subscribes[addr].find(socket) != subscribes[addr].end())
					subscribes[addr].erase(socket);
			} else {
				response["status"] = "error";
				response["error"] = DaemonServer::error(MTB_MODULE_INVALID_ADDR, "Invalid module address");
				break;
			}
		}

		server.send(socket, response);

	} else if (command == "module_set_config") {
		// Set config can change module type

		size_t addr = request["address"].toInt();
		uint8_t type = request["type"].toInt();
		if (!Mtb::isValidModuleAddress(addr)) {
			std::cout << "here" << std::endl;
			sendError(socket, request, MTB_MODULE_INVALID_ADDR, "Invalid module address");
			return;
		}

		if (modules[addr] == nullptr) {
			if ((type&0xF0) == 0x10)
				modules[addr] = std::make_unique<MtbUni>(addr);
			else
				modules[addr] = std::make_unique<MtbModule>(addr);
			modules[addr]->jsonSetConfig(socket, request);
			return;
		}

		if ((modules[addr]->isActive()) && (type != static_cast<size_t>(modules[addr]->moduleType()))) {
			sendError(socket, request, MTB_ALREADY_STARTED, "Cannot change type of active module!");
			return;
		}

		// Change config of active module
		modules[addr]->jsonSetConfig(socket, request);

	} else if (command.startsWith("module_")) {
		size_t addr = request["address"].toInt();
		if ((Mtb::isValidModuleAddress(addr)) && (modules[addr] != nullptr)) {
			modules[addr]->jsonCommand(socket, request);
		} else {
			sendError(socket, request, MTB_MODULE_INVALID_ADDR, "Invalid module address");
		}

	} else if (command == "reset_my_outputs") {
		this->clientResetOutputs(
			socket,
			[socket, request]() { server.send(socket, jsonOkResponse(request)); },
			[socket, request]() { sendError(socket, request, Mtb::CmdError::BusNoResponse); }
		);

	}
}

QJsonObject DaemonCoreApplication::mtbUsbJson() const {
	QJsonObject status;
	bool connected = (mtbusb.connected() && mtbusb.mtbUsbInfo().has_value() && mtbusb.activeModules().has_value());
	status["connected"] = connected;
	if (connected) {
		const Mtb::MtbUsbInfo mtbusbinfo = mtbusb.mtbUsbInfo().value();
		const std::array<bool, Mtb::_MAX_MODULES> activeModules = mtbusb.activeModules().value();
		status["type"] = mtbusbinfo.type;
		try {
			status["speed"] = Mtb::mtbBusSpeedToInt(mtbusbinfo.speed);
		} catch (...) {}
		status["firmware_version"] = mtbusbinfo.fw_version();
		status["protocol_version"] = mtbusbinfo.proto_version();

		QJsonArray jsonActiveModules;
		for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
			if (activeModules[i])
				jsonActiveModules.push_back(static_cast<int>(i));

		status["active_modules"] = jsonActiveModules;
	}
	return status;
}

/* Configuration ------------------------------------------------------------ */

bool DaemonCoreApplication::loadConfig(const QString& filename) {
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;
	QString content = file.readAll();
	file.close();
	this->config = QJsonDocument::fromJson(content.toUtf8()).object();

	{
		// Load modules
		QJsonObject _modules = this->config["modules"].toObject();
		for (const QString &_addr : _modules.keys()) {
			size_t addr = _addr.toInt();
			QJsonObject module = _modules[_addr].toObject();
			size_t type = module["type"].toInt();

			if ((type&0xF0) == 0x10)
				modules[addr] = std::make_unique<MtbUni>(addr);
			else
				modules[addr] = std::make_unique<MtbModule>(addr);

			modules[addr]->loadConfig(module);
		}
	}

	this->config.remove("modules");
	return true;
}

bool DaemonCoreApplication::saveConfig(const QString &filename) {
	log("Saving config to "+filename+"...", Mtb::LogLevel::Info);

	QJsonObject root = this->config;
	QJsonObject jsonModules;
	for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
		if (modules[i] != nullptr) {
			QJsonObject module;
			modules[i]->saveConfig(module);
			jsonModules[QString::number(i)] = module;
		}
	}
	root["modules"] = jsonModules;

	QJsonDocument doc(root);

	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return false;
	file.write(doc.toJson(QJsonDocument::JsonFormat::Indented));
	file.close();
	return true;
}

std::vector<QTcpSocket*> outputSetters() {
	std::vector<QTcpSocket*> result;
	for (const auto& modulePtr : modules) {
		if (modulePtr != nullptr) {
			for (QTcpSocket* socket : modulePtr->outputSetters())
				if (std::find(result.begin(), result.end(), socket) == result.end())
					result.push_back(socket);
		}
	}
	return result;
}

void DaemonCoreApplication::serverClientDisconnected(QTcpSocket* socket) {
	for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
		if (subscribes[i].find(socket) != subscribes[i].end())
			subscribes[i].erase(socket);
		if (modules[i] != nullptr)
			modules[i]->clientDisconnected(socket);
	}

	this->clientResetOutputs(socket, [](){}, [](){});
}

void DaemonCoreApplication::clientResetOutputs(
		QTcpSocket* socket,
		std::function<void()> onOk,
		std::function<void()> onError) {
	const std::vector<QTcpSocket*>& setters = outputSetters();

	if (setters.size() >= 2) {
		for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
			if (modules[i] != nullptr)
				modules[i]->resetOutputsOfClient(socket);
		onOk();
	} else if (setters.size() == 1) {
		// Reset outputs of all modules with broadcast
		for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
			if (modules[i] != nullptr)
				modules[i]->allOutputsReset();

		mtbusb.send(
			Mtb::CmdMtbModuleResetOutputs(
				{[onOk](void*) { onOk(); }},
				{[onError](Mtb::CmdError, void*) {
					log("Unable to reset MTB modules outputs!", Mtb::LogLevel::Error);
					onError();
				}}
			)
		);
	} else {
		onOk();
	}
}

#ifdef Q_OS_WIN
// Handler function will be called on separate thread!
static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT: // Ctrl+C
		QCoreApplication::quit();
		return TRUE;
	case CTRL_CLOSE_EVENT: // Closing the console window
		QCoreApplication::quit();
		return TRUE;
	}

	// Return TRUE if handled this message, further handler functions won't be called.
	// Return FALSE to pass this message to further handlers until default handler calls ExitProcess().
	return FALSE;
}
#endif
