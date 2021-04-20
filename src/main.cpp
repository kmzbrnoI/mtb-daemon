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

Mtb::MtbUsb mtbusb;
DaemonServer server;
std::array<std::unique_ptr<MtbModule>, Mtb::_MAX_MODULES> modules;
std::array<std::map<QTcpSocket*, bool>, Mtb::_MAX_MODULES> subscribes;
Mtb::LogLevel DaemonCoreApplication::loglevel = Mtb::LogLevel::Info;

int main(int argc, char *argv[]) {
	DaemonCoreApplication a(argc, argv);
	return a.exec();
}

DaemonCoreApplication::DaemonCoreApplication(int &argc, char **argv)
     : QCoreApplication(argc, argv) {
	QObject::connect(&server, SIGNAL(jsonReceived(QTcpSocket*, const QJsonObject&)),
	                 this, SLOT(serverReceived(QTcpSocket*, const QJsonObject&)));

	QObject::connect(&t_reconnect, SIGNAL(timeout()), this, SLOT(tReconnectTick()));

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
				}},
				{"mtb-usb", QJsonObject{
					{"port", "auto"},
				}},
			};
			this->saveConfig(configFileName);
		} else {
			log("Config file "+configFileName+" successfully loaded.", Mtb::LogLevel::Info);
		}
	}

	Mtb::LogLevel loglevel = static_cast<Mtb::LogLevel>(this->config["loglevel"].toInt());
	mtbusb.loglevel = loglevel;
	DaemonCoreApplication::loglevel = loglevel;

	{ // Start server
		const QJsonObject serverConfig = this->config["server"].toObject();
		size_t port = serverConfig["port"].toInt();
		QHostAddress host(serverConfig["host"].toString());
		log("Starting server: "+host.toString()+":"+QString::number(port)+"...", Mtb::LogLevel::Info);
		server.listen(host, port);
	}

	this->mtbUsbConnect();
	if (!mtbusb.connected())
		this->t_reconnect.start(T_RECONNECT_PERIOD);
}

/* MTB-USB handling ----------------------------------------------------------*/

void DaemonCoreApplication::mtbUsbConnect() {
	const QJsonObject mtbUsbConfig = this->config["mtb-usb"].toObject();
	QString port = mtbUsbConfig["port"].toString();

	if (port == "auto") {
		const std::vector<QSerialPortInfo>& mtbUsbPorts = Mtb::MtbUsb::ports();
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

void log(const QString& message, Mtb::LogLevel loglevel) {
	DaemonCoreApplication::log(message, loglevel);
}

void DaemonCoreApplication::log(const QString& message, Mtb::LogLevel loglevel) {
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
		{"command", "mtbusb_connect"},
		{"type", "event"},
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

void DaemonCoreApplication::activateModule(uint8_t addr) {
	log("New module "+QString::number(addr)+" discovered, activating...", Mtb::LogLevel::Info);
	mtbusb.send(
		Mtb::CmdMtbModuleInfoRequest(
			addr,
			{[this](uint8_t addr, Mtb::ModuleInfo info, void*) { this->moduleGotInfo(addr, info); }},
			{[](Mtb::CmdError, void*) {
				log("Did not get info from newly discovered module, module keeps disabled.",
				    Mtb::LogLevel::Error);
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
		{"command", "mtbusb_disconnect"},
		{"type", "event"},
	});

	for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
		if (modules[i] != nullptr)
			modules[i]->mtbUsbDisconnected();

	this->t_reconnect.start(T_RECONNECT_PERIOD);
}

void DaemonCoreApplication::mtbUsbOnNewModule(uint8_t addr) {
	this->activateModule(addr);
}

void DaemonCoreApplication::mtbUsbOnModuleFail(uint8_t addr) {
	if (modules[addr] != nullptr)
		modules[addr]->mtbBusLost();
}

void DaemonCoreApplication::mtbUsbOnInputsChange(uint8_t addr, const std::vector<uint8_t>& data) {
	if (modules[addr] != nullptr)
		modules[addr]->mtbBusInputsChanged(data);
}

void DaemonCoreApplication::tReconnectTick() {
	if (mtbusb.connected())
		this->t_reconnect.stop();

	const QJsonObject mtbUsbConfig = this->config["mtb-usb"].toObject();
	QString port = mtbUsbConfig["port"].toString();
	const std::vector<QSerialPortInfo>& mtbUsbPorts = Mtb::MtbUsb::ports();

	if (port == "auto") {
		if (mtbUsbPorts.size() != 1)
			return;
	} else {
		bool found = false;
		for (const QSerialPortInfo& portInfo :  mtbUsbPorts)
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

/* JSON server handling ------------------------------------------------------*/

void DaemonCoreApplication::serverReceived(QTcpSocket* socket, const QJsonObject& request) {
	QString command = request["command"].toString();
	std::optional<size_t> id;
	if (request.contains("id"))
		id = request["id"].toInt();

	if (command == "status") {
		this->sendStatus(*socket, id);

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

		server.send(*socket, response);

	} else if (command == "module") {
		QJsonObject response{
			{"command", "module"},
			{"type", "response"},
		};
		if (id)
			response["id"] = static_cast<int>(id.value());

		size_t addr = request["address"].toInt();
		if ((Mtb::isValidModuleAddress(addr)) && (modules[addr] != nullptr)) {
			response["module"] = modules[addr]->moduleInfo(request["state"].toBool());
			response["status"] = "ok";
		} else {
			response["status"] = "error";
			response["error"] = DaemonServer::error(MTB_MODULE_INVALID_ADDR, "Invalid module address");
		}

		server.send(*socket, response);

	} else if (command == "modules") {
		QJsonObject response {
			{"command", "module"},
			{"type", "response"},
		};
		if (id)
			response["id"] = static_cast<int>(id.value());

		for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
			if (modules[i] != nullptr)
				response[QString::number(i)] = modules[i]->moduleInfo(request["state"].toBool());
		}

		server.send(*socket, response);

	} else if (command == "module_subscribe") {
		QJsonArray addresses;
		QJsonObject response {
			{"command", "module_subscribe"},
			{"type", "response"},
			{"status", "ok"},
		};
		if (id)
			response["id"] = static_cast<int>(id.value());

		for (const auto& value : request["addresses"].toArray()) {
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
		server.send(*socket, response);

	} else if (command == "module_unsubscribe") {
		QJsonObject response {
			{"command", "module_unsubscribe"},
			{"type", "response"},
			{"status", "ok"},
		};
		if (id)
			response["id"] = static_cast<int>(id.value());

		for (const auto& value : response["addresses"].toArray()) {
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

		server.send(*socket, response);

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
			QJsonObject response{
				{"command", command},
				{"type", "response"},
				{"status", "error"},
				{"error", DaemonServer::error(MTB_MODULE_INVALID_ADDR, "Invalid module address")},
			};
			server.send(*socket, response);
		}
	}
}

void DaemonCoreApplication::sendStatus(QTcpSocket& socket, std::optional<size_t> id) {
	QJsonObject response {
		{"command", "status"},
		{"type", "response"},
		{"status", "ok"},
	};
	if (id)
		response["id"] = static_cast<int>(id.value());

	QJsonObject status;
	bool connected = (mtbusb.connected() && mtbusb.mtbUsbInfo().has_value() &&
	                  mtbusb.activeModules().has_value());
	status["connected"] = connected;
	if (connected) {
		const Mtb::MtbUsbInfo& mtbusbinfo = mtbusb.mtbUsbInfo().value();
		const std::array<bool, Mtb::_MAX_MODULES>& activeModules = mtbusb.activeModules().value();
		QJsonObject mtbusb {
			{"type", mtbusbinfo.type},
			{"speed", Mtb::mtbBusSpeedToInt(mtbusbinfo.speed)},
			{"firmware_version", mtbusbinfo.fw_version()},
			{"protocol_version", mtbusbinfo.proto_version()},
		};

		QJsonArray jsonActiveModules;
		for (size_t i = 0; i < Mtb::_MAX_MODULES; i++)
			if (activeModules[i])
				jsonActiveModules.push_back(static_cast<int>(i));

		mtbusb["active_modules"] = jsonActiveModules;
		status["mtb-usb"] = mtbusb;
	}
	response["status"] = status;

	server.send(socket, response);
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
		for (const QString& _addr : _modules.keys()) {
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

bool DaemonCoreApplication::saveConfig(const QString& filename) {
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
