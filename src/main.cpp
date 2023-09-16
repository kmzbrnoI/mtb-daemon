#include <QSerialPort>
#include <QSerialPortInfo>
#include <QJsonArray>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include "main.h"
#include "mtbusb/mtbusb-common.h"
#include "modules/uni.h"
#include "modules/unis.h"
#include "errors.h"
#include "logging.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Mtb::MtbUsb mtbusb;
DaemonServer server;
std::array<std::unique_ptr<MtbModule>, Mtb::_MAX_MODULES> modules;
std::array<std::map<QTcpSocket*, bool>, Mtb::_MAX_MODULES> subscribes;

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

const QJsonObject DEFAULT_CONFIG = {
	{"loglevel", static_cast<int>(Mtb::LogLevel::Info)},
	{"server", QJsonObject{
		{"host", "127.0.0.1"},
		{"port", static_cast<int>(SERVER_DEFAULT_PORT)},
		{"keepAlive", true},
		{"allowedClients", QJsonArray{"127.0.0.1"}}
	}},
	{"mtb-usb", QJsonObject{
		{"port", "auto"},
		{"keepAlive", true},
		// In case {"speed", 115200} is present, speed is forced to MTB-USB
		// If not present, MTB-USB chooses speed based on its EEPROM-saved value
	}},
	{"production_logging", QJsonObject{
		{"enabled", false},
		{"loglevel", static_cast<int>(Mtb::LogLevel::RawData)},
		{"history", 100},
		{"future", 20},
		{"directory", "prodLog"},
		{"detectLevel", static_cast<int>(Mtb::LogLevel::Warning)},
	}},
};


DaemonCoreApplication::DaemonCoreApplication(int &argc, char **argv)
     : QCoreApplication(argc, argv) {
	QObject::connect(&server, SIGNAL(jsonReceived(QTcpSocket*, const QJsonObject&)),
	                 this, SLOT(serverReceived(QTcpSocket*, const QJsonObject&)), Qt::DirectConnection);
	QObject::connect(&server, SIGNAL(clientDisconnected(QTcpSocket*)),
	                 this, SLOT(serverClientDisconnected(QTcpSocket*)), Qt::DirectConnection);

	QObject::connect(&t_reconnect, SIGNAL(timeout()), this, SLOT(tReconnectTick()));
	QObject::connect(&t_reactivate, SIGNAL(timeout()), this, SLOT(tReactivateTick()));

	// Use Qt::DirectConnection in all mtbusb signals, because it is significantly faster.
	// ASSERT: singnal must be emitted in the same thread!
	QObject::connect(&mtbusb, SIGNAL(onLog(QString, Mtb::LogLevel)),
	                 this, SLOT(mtbUsbOnLog(QString, Mtb::LogLevel)), Qt::DirectConnection);
	QObject::connect(&mtbusb, SIGNAL(onConnect()), this, SLOT(mtbUsbOnConnect()), Qt::DirectConnection);
	QObject::connect(&mtbusb, SIGNAL(onDisconnect()), this, SLOT(mtbUsbOnDisconnect()), Qt::DirectConnection);
	QObject::connect(&mtbusb, SIGNAL(onNewModule(uint8_t)), this, SLOT(mtbUsbOnNewModule(uint8_t)), Qt::DirectConnection);
	QObject::connect(&mtbusb, SIGNAL(onModuleFail(uint8_t)), this, SLOT(mtbUsbOnModuleFail(uint8_t)), Qt::DirectConnection);
	QObject::connect(&mtbusb, SIGNAL(onModuleInputsChange(uint8_t, const std::vector<uint8_t>&)),
	                 this, SLOT(mtbUsbOnInputsChange(uint8_t, const std::vector<uint8_t>&)), Qt::DirectConnection);
	QObject::connect(&mtbusb, SIGNAL(onModuleDiagStateChange(uint8_t, const std::vector<uint8_t>&)),
	                 this, SLOT(mtbUsbOnDiagStateChange(uint8_t, const std::vector<uint8_t>&)), Qt::DirectConnection);

#ifdef Q_OS_WIN
	SetConsoleOutputCP(CP_UTF8);
#endif

	log("Starting MTB Daemon v"+QString(VERSION)+"...", Mtb::LogLevel::Info);

	{ // Load config file
		this->configFileName = (argc > 1) ? argv[1] : DEFAULT_CONFIG_FILENAME;
		try {
			this->loadConfig(this->configFileName);
			log("Config file "+configFileName+" successfully loaded.", Mtb::LogLevel::Info);
		} catch (const ConfigNotFound&) {
			log("Unable to load config file "+configFileName+
			    ", resetting config, writing default config file...",
			    Mtb::LogLevel::Info);
			this->config = DEFAULT_CONFIG;
			this->saveConfig(configFileName);
		}
	}

	logger.loadConfig(this->config);

	mtbusb.loglevel = Mtb::LogLevel::Debug; // get everything, filter ourself
	mtbusb.ping = this->config["mtb-usb"].toObject()["keepAlive"].toBool(true);

	{ // Start server
		const QJsonObject serverConfig = this->config["server"].toObject();
		size_t port = serverConfig["port"].toInt();
		bool keepAlive = serverConfig["keepAlive"].toBool(true);
		QHostAddress host(serverConfig["host"].toString());
		log("Starting server: "+host.toString()+":"+QString::number(port)+"...", Mtb::LogLevel::Info);
		server.listen(host, port, keepAlive);
	}

	this->mtbUsbConnect();
	if (!mtbusb.connected()) {
		this->t_reconnect.start(T_RECONNECT_PERIOD);
		log("Waiting for MTB-USB to appear...", Mtb::LogLevel::Info);
	}
	this->t_reactivate.start(T_REACTIVATE_PERIOD);
}

/* MTB-USB handling ----------------------------------------------------------*/

void DaemonCoreApplication::mtbUsbConnect() {
	const QJsonObject mtbUsbConfig = this->config["mtb-usb"].toObject();
	QString port = mtbUsbConfig["port"].toString();

	if (port == "auto") {
		const std::vector<QSerialPortInfo> &mtbUsbPorts = Mtb::MtbUsb::ports();
		log("Automatic MTB-USB port detected", Mtb::LogLevel::Info);

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

void DaemonCoreApplication::mtbUsbOnLog(QString message, Mtb::LogLevel loglevel) {
	log(message, loglevel);
}

void DaemonCoreApplication::mtbUsbOnConnect() {
	mtbusb.send(
		Mtb::CmdMtbUsbInfoRequest(
			{[this](void*) { this->mtbUsbGotInfo(); }},
			{[](Mtb::CmdError, void*) {
				log("Did not get info from MTB-USB, disconnecting...", Mtb::LogLevel::Error);
				mtbusb.disconnect();
			}}
		)
	);
}

void DaemonCoreApplication::mtbUsbGotInfo() {
	const QJsonObject& mtbusbObj = this->config["mtb-usb"].toObject();
	if (!mtbusbObj.contains("speed"))
		return this->mtbUsbProperSpeedSet();

	const int fileSpeed = mtbusbObj["speed"].toInt();
	if (!Mtb::mtbBusSpeedValid(fileSpeed)) {
		log("Invalid MTBbus speed in config file: "+QString::number(mtbusbObj["speed"].toInt()),
		    Mtb::LogLevel::Warning);
		return this->mtbUsbProperSpeedSet();
	}

	Mtb::MtbBusSpeed newSpeed = Mtb::intToMtbBusSpeed(fileSpeed);

	if (newSpeed == mtbusb.mtbUsbInfo().value().speed) {
		log("Saved MTBbus speed matches current MTB-USB speed, ok.", Mtb::LogLevel::Info);
		return this->mtbUsbProperSpeedSet();
	}

	log("Saved MTBbus speed does NOT match current MTB-USB speed, changing...", Mtb::LogLevel::Info);
	mtbusb.changeSpeed(
		newSpeed,
		{[this]() { this->mtbUsbProperSpeedSet(); }},
		{[](Mtb::CmdError) {
			log("Unable to set MTBbus speed, disconnecting...", Mtb::LogLevel::Error);
			mtbusb.disconnect();
		}}
	);
}

void DaemonCoreApplication::mtbUsbProperSpeedSet() {
	mtbusb.send(
		Mtb::CmdMtbUsbActiveModulesRequest(
			{[this](void*) { this->mtbUsbGotModules(); }},
			{[](Mtb::CmdError, void*) {
				log("Did not get active modules from MTB-USB, disconnecting...", Mtb::LogLevel::Error);
				mtbusb.disconnect();
			}}
		)
	);
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
				log("Did not get info from module "+QString::number(addr)+", trying again...",
				    Mtb::LogLevel::Error);
				if (attemptsRemaining > 0) {
					QTimer::singleShot(500, [this, addr, attemptsRemaining]() {
						if (!mtbusb.connected())
							return;
						if ((modules[addr] == nullptr) || (!modules[addr]->isActive() && !modules[addr]->isActivating()))
							this->activateModule(addr, attemptsRemaining-1);
					});
				}
			}}
		)
	);
}

void DaemonCoreApplication::moduleGotInfo(uint8_t addr, Mtb::ModuleInfo info) {
	if ((modules[addr] != nullptr) && (static_cast<size_t>(modules[addr]->moduleType()) != info.type))
		log("Detected module "+QString::number(addr)+" type & stored module type mismatch! Forgetting config...",
		    Mtb::LogLevel::Warning);

	if ((info.type&0xF0) == 0x10) {
		modules[addr] = std::make_unique<MtbUni>(addr);
	} else if (info.type == 0x50) {
		modules[addr] = std::make_unique<MtbUnis>(addr);
	} else {
		log("Unknown module type: "+QString::number(addr)+": 0x"+
		    QString::number(info.type, 16)+"!", Mtb::LogLevel::Warning);
		modules[addr] = std::make_unique<MtbModule>(addr);
	}

	modules[addr]->mtbBusActivate(info);
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
	log("Waiting for MTB-USB to appear...", Mtb::LogLevel::Info);
}

void DaemonCoreApplication::mtbUsbOnNewModule(uint8_t addr) {
	if ((modules[addr] == nullptr) || ((!modules[addr]->isActive()) && (!modules[addr]->isRebooting()) &&
	    (!modules[addr]->isFirmwareUpgrading())))
		this->activateModule(addr);
}

void DaemonCoreApplication::mtbUsbOnModuleFail(uint8_t addr) {
	// Warning: any operation could be pending on module
	// Beware module instance deletion!
	if ((modules[addr] != nullptr) && (!modules[addr]->isFirmwareUpgrading()) && (!modules[addr]->isRebooting()))
		modules[addr]->mtbBusLost();
}

void DaemonCoreApplication::mtbUsbOnInputsChange(uint8_t addr, const std::vector<uint8_t> &data) {
	if (modules[addr] != nullptr)
		modules[addr]->mtbBusInputsChanged(data);
}

void DaemonCoreApplication::mtbUsbOnDiagStateChange(uint8_t addr, const std::vector<uint8_t> &data) {
	if (modules[addr] != nullptr)
		modules[addr]->mtbBusDiagStateChanged(data);
}

void DaemonCoreApplication::tReconnectTick() {
	if (mtbusb.connected())
		this->t_reconnect.stop();

	const QJsonObject mtbUsbConfig = this->config["mtb-usb"].toObject();
	QString port = mtbUsbConfig["port"].toString();

	if (port == "auto") {
		const std::vector<QSerialPortInfo> &mtbUsbPorts = Mtb::MtbUsb::ports();
		if (mtbUsbPorts.size() != 1)
			return;
	} else {
		QList<QSerialPortInfo> ports(QSerialPortInfo::availablePorts());
		bool found = false;
		for (const QSerialPortInfo &info : ports)
			if (info.portName() == port)
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
	if (!mtbusb.connected())
		return;

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
				if (!this->hasWriteAccess(socket))
					return sendAccessDenied(socket, request);
				if (!mtbusb.connected() || !mtbusb.mtbUsbInfo().has_value())
					return sendError(socket, request, MTB_DEVICE_DISCONNECTED, "Disconnected from MTB-USB!");
				size_t speed = jsonMtbUsb["speed"].toInt();
				if (!Mtb::mtbBusSpeedValid(speed))
					return sendError(socket, request, MTB_INVALID_SPEED, "Invalid MTBbus speed!");
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
		if (!this->hasWriteAccess(socket))
			return sendAccessDenied(socket, request);

		QString filename = this->configFileName;
		if (request.contains("filename"))
			filename = request["filename"].toString();
		bool ok = true;
		try {
			this->saveConfig(filename);
		} catch (...) {
			ok = false;
		}
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

	} else if (command == "load_config") {
		if (!this->hasWriteAccess(socket))
			return sendAccessDenied(socket, request);

		QString filename = this->configFileName;
		if (request.contains("filename"))
			filename = request["filename"].toString();
		bool ok = true;
		try {
			this->loadConfig(filename);
			log("Config file "+filename+" successfully loaded.", Mtb::LogLevel::Info);
		} catch (...) {
			ok = false;
		}
		QJsonObject response {
			{"command", "load_config"},
			{"type", "response"},
			{"status", ok ? "ok" : "error"},
		};
		if (!ok)
			response["error"] = jsonError(MTB_FILE_CANNOT_ACCESS, "Cannot load file "+filename);
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
				jsonModules[QString::number(i)] = modules[i]->moduleInfo(
					request["state"].toBool(), true
				);
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
		// Set config can create new module
		if (!this->hasWriteAccess(socket))
			return sendAccessDenied(socket, request);

		size_t addr = request["address"].toInt();
		uint8_t type = request["type_code"].toInt();
		if (!Mtb::isValidModuleAddress(addr))
			return sendError(socket, request, MTB_MODULE_INVALID_ADDR, "Invalid module address");

		if (modules[addr] == nullptr) {
			if ((type&0xF0) == 0x10)
				modules[addr] = std::make_unique<MtbUni>(addr);
			else if (type == 0x50)
				modules[addr] = std::make_unique<MtbUnis>(addr);
			else
				modules[addr] = std::make_unique<MtbModule>(addr);
		}

		if ((modules[addr]->isActive()) && (type != static_cast<size_t>(modules[addr]->moduleType())))
			return sendError(socket, request, MTB_ALREADY_STARTED, "Cannot change type of active module!");

		modules[addr]->jsonSetConfig(socket, request);

	} else if ((command == "module_specific_command") && ((!request.contains("address")) || (request["address"].toInt() == 0))) {
		// Module-specific broadcast
		if (!this->hasWriteAccess(socket))
			return sendAccessDenied(socket, request);

		const QJsonArray &dataAr = request["data"].toArray();
		std::vector<uint8_t> data;
		for (const auto var : dataAr)
			data.push_back(var.toInt());

		mtbusb.send(
			Mtb::CmdMtbModuleSpecific(
				data,
				{[request, socket](void*) {
					QJsonObject json = jsonOkResponse(request);
					server.send(socket, json);
				}},
				{[socket, request](Mtb::CmdError error, void*) {
					sendError(socket, request, static_cast<int>(error)+0x1000, Mtb::cmdErrorToStr(error));
				}}
			)
		);

	} else if (command.startsWith("module_")) {
		if (!this->hasWriteAccess(socket))
			return sendAccessDenied(socket, request);

		size_t addr = request["address"].toInt();
		if ((Mtb::isValidModuleAddress(addr)) && (modules[addr] != nullptr)) {
			modules[addr]->jsonCommand(socket, request);
		} else {
			sendError(socket, request, MTB_MODULE_INVALID_ADDR, "Invalid module address");
		}

	} else if (command == "reset_my_outputs") {
		if (!this->hasWriteAccess(socket))
			return sendAccessDenied(socket, request);

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

void DaemonCoreApplication::loadConfig(const QString& filename) {
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		throw ConfigNotFound(QString("Configuration file not found!"));
	QString content = file.readAll();
	file.close();

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &parseError);
	if (doc.isNull())
		throw JsonParseError("Unable to parse config file "+filename+": "+parseError.errorString()+" offset: "+QString::number(parseError.offset));
	this->config = doc.object();

	{
		// Load modules
		QJsonObject _modules = this->config["modules"].toObject();
		for (const QString &_addr : _modules.keys()) {
			size_t addr = _addr.toInt();
			QJsonObject module = _modules[_addr].toObject();
			size_t type = module["type"].toInt();

			if ((type&0xF0) == 0x10)
				modules[addr] = std::make_unique<MtbUni>(addr);
			else if (type == 0x50)
				modules[addr] = std::make_unique<MtbUnis>(addr);
			else
				modules[addr] = std::make_unique<MtbModule>(addr);

			modules[addr]->loadConfig(module);
		}
	}

	this->config.remove("modules");

	{
		// Load allowed clients
		const QJsonObject serverConfig = this->config["server"].toObject();
		this->writeAccess.clear();
		for (const auto& value : serverConfig["allowedClients"].toArray())
			this->writeAccess.push_back(QHostAddress(value.toString()));
	}
}

void DaemonCoreApplication::saveConfig(const QString &filename) {
	log("Saving config to "+filename+"...", Mtb::LogLevel::Info);

	QJsonObject root = this->config;
	QJsonObject jsonModules;
	for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
		if (modules[i] != nullptr) {
			QJsonObject module;
			modules[i]->saveConfig(module);
			jsonModules[QString("%3").arg(i, 3, 10, QChar('0'))] = module;
		}
	}
	root["modules"] = jsonModules;

	QJsonDocument doc(root);

	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		throw FileWriteError("Unable to open "+filename+" for writing!");
	file.write(doc.toJson(QJsonDocument::JsonFormat::Indented));
	file.close();
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
	} else if ((setters.size() == 1) && (setters[0] == socket)) {
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

bool DaemonCoreApplication::hasWriteAccess(const QTcpSocket *socket) {
	if (this->writeAccess.empty())
		return true;
	return (std::find(this->writeAccess.begin(), this->writeAccess.end(),
		socket->peerAddress()) != this->writeAccess.end());

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
