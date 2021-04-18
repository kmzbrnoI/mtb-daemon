#include <QSerialPort>
#include <QJsonArray>
#include <iostream>
#include "main.h"
#include "mtbusb/mtbusb-common.h"
#include "modules/uni.h"
#include "errors.h"

Mtb::MtbUsb mtbusb;
DaemonServer server;
std::array<std::unique_ptr<MtbModule>, Mtb::_MAX_MODULES> modules;
std::array<std::map<QTcpSocket*, bool>, Mtb::_MAX_MODULES> subscribes;

DaemonCoreApplication::DaemonCoreApplication(int &argc, char **argv)
     : QCoreApplication(argc, argv) {
	//if (argc < 2)
	//	throw std::logic_error("No port specified!");

	server.listen(QHostAddress::Any, 3000);
	QObject::connect(&server, SIGNAL(jsonReceived(QTcpSocket*, const QJsonObject&)),
	                 this, SLOT(serverReceived(QTcpSocket*, const QJsonObject&)));

	QObject::connect(&mtbusb, SIGNAL(onLog(QString, Mtb::LogLevel)),
	                 this, SLOT(mtbUsbLog(QString, Mtb::LogLevel)));
	QObject::connect(&mtbusb, SIGNAL(onConnect()), this, SLOT(mtbUsbConnect()));
	QObject::connect(&mtbusb, SIGNAL(onDisconnect()), this, SLOT(mtbUsbDisconnect()));
	QObject::connect(&mtbusb, SIGNAL(onNewModule(uint8_t)), this, SLOT(mtbUsbNewModule(uint8_t)));
	QObject::connect(&mtbusb, SIGNAL(onModuleFail(uint8_t)), this, SLOT(mtbUsbModuleFail(uint8_t)));
	QObject::connect(&mtbusb, SIGNAL(onModuleInputsChange(uint8_t, const std::vector<uint8_t>&)),
	                 this, SLOT(mtbUsbInputsChange(uint8_t, const std::vector<uint8_t>&)));

	mtbusb.loglevel = Mtb::LogLevel::Debug;
	//mtbusb.connect(argv[1], 115200, QSerialPort::FlowControl::NoFlowControl);
}

/* MTB-USB handling ----------------------------------------------------------*/

void log(const QString& message, Mtb::LogLevel loglevel) {
	(void)loglevel;
	qDebug() << "[" << QTime::currentTime().toString("hh:mm:ss,zzz") << "] " << message;
}

void DaemonCoreApplication::mtbUsbLog(QString message, Mtb::LogLevel loglevel) {
	log(message, loglevel);
}

void DaemonCoreApplication::mtbUsbConnect() {
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
	const auto activeModules = mtbusb.activeModules().value();
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
			{[](Mtb::CmdError cmdError, void*) {
				log("Did not get info from newly discovered module, module keeps disabled.",
				    Mtb::LogLevel::Error);
			}}
		)
	);
}

void DaemonCoreApplication::moduleGotInfo(uint8_t addr, Mtb::ModuleInfo info) {
	if ((info.type&0xF0) == 0x10) {
		if (modules[addr] == nullptr) {
			modules[addr] = std::make_unique<MtbUni>();
		} else {
			if (dynamic_cast<MtbUni*>(modules[addr].get()) == nullptr)
				modules[addr] = std::make_unique<MtbUni>();
		}
		modules[addr]->mtbBusActivate(info);
	} else {
		log("Unknown module type: "+QString::number(addr)+": 0x"+
			QString::number(info.type, 16)+"!", Mtb::LogLevel::Warning);
	}
}


void DaemonCoreApplication::mtbUsbDidNotGetModules(Mtb::CmdError) {
	log("Did not get active modules from MTB-USB, disconnecting...", Mtb::LogLevel::Info);
	mtbusb.disconnect();
}

void DaemonCoreApplication::mtbUsbDisconnect() {
}

void DaemonCoreApplication::mtbUsbNewModule(uint8_t addr) {
	this->activateModule(addr);
}

void DaemonCoreApplication::mtbUsbModuleFail(uint8_t addr) {
}

void DaemonCoreApplication::mtbUsbInputsChange(uint8_t addr, const std::vector<uint8_t>& data) {
}

/* JSON server handling ------------------------------------------------------*/

void DaemonCoreApplication::serverReceived(QTcpSocket* socket, const QJsonObject& request) {
	QString command = request["command"].toString();
	std::optional<size_t> id;
	if (request.contains("id"))
		id = request["id"].toInt();

	if (command == "status") {
		this->sendStatus(*socket, id);

	} else if (command == "module") {
		QJsonObject response;
		if (id)
			response["id"] = static_cast<int>(id.value());
		response["command"] = "module";
		response["type"] = "response";

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
		QJsonObject response;
		if (id)
			response["id"] = static_cast<int>(id.value());
		response["command"] = "module";
		response["type"] = "response";

		for (size_t i = 0; i < Mtb::_MAX_MODULES; i++) {
			if (modules[i] != nullptr)
				response[QString::number(i)] = modules[i]->moduleInfo(request["state"].toBool());
		}

		server.send(*socket, response);

	} else if (command == "module_subscribe") {
		QJsonObject response;
		if (id)
			response["id"] = static_cast<int>(id.value());
		response["command"] = "module_subscribe";
		response["type"] = "response";
		response["status"] = "ok";

		for (const auto& value : response["addresses"].toArray()) {
			size_t addr = value.toInt();
			if (Mtb::isValidModuleAddress(addr)) {
				subscribes[addr].insert_or_assign(socket, true);
			} else {
				response["status"] = "error";
				response["error"] = DaemonServer::error(MTB_MODULE_INVALID_ADDR, "Invalid module address");
				break;
			}
		}

		server.send(*socket, response);

	} else if (command == "module_unsubscribe") {
		QJsonObject response;
		if (id)
			response["id"] = static_cast<int>(id.value());
		response["command"] = "module_unsubscribe";
		response["type"] = "response";
		response["status"] = "ok";

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

	} else if (command.startsWith("module_")) {
		size_t addr = request["address"].toInt();
		if ((Mtb::isValidModuleAddress(addr)) && (modules[addr] != nullptr)) {
			modules[addr]->jsonCommand(*socket, request);
		} else {
			QJsonObject response;
			response["status"] = "error";
			response["error"] = DaemonServer::error(MTB_MODULE_INVALID_ADDR, "Invalid module address");
			server.send(*socket, response);
		}
	}
}

void DaemonCoreApplication::sendStatus(QTcpSocket& socket, std::optional<size_t> id) {
	QJsonObject response;
	if (id)
		response["id"] = static_cast<int>(id.value());
	response["command"] = "status";
	response["type"] = "response";
	response["status"] = "ok";

	QJsonObject status;
	bool connected = (mtbusb.connected() && mtbusb.mtbUsbInfo().has_value() &&
	                  mtbusb.activeModules().has_value());
	status["connected"] = connected;
	if (connected) {
		const Mtb::MtbUsbInfo& mtbusbinfo = mtbusb.mtbUsbInfo().value();
		const std::array<bool, Mtb::_MAX_MODULES>& activeModules = mtbusb.activeModules().value();
		QJsonObject mtbusb;
		mtbusb["type"] = mtbusbinfo.type;
		mtbusb["speed"] = Mtb::mtbBusSpeedToInt(mtbusbinfo.speed);
		mtbusb["firmware_version"] = mtbusbinfo.fw_version();
		mtbusb["protocol_version"] = mtbusbinfo.proto_version();

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

int main(int argc, char *argv[]) {
	DaemonCoreApplication a(argc, argv);
	return a.exec();
}
