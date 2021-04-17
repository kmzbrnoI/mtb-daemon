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

	mtbusb.loglevel = Mtb::LogLevel::Debug;
	/*mtbusb.connect(argv[1], 115200, QSerialPort::FlowControl::NoFlowControl);

	mtbusb.send(
		Mtb::CmdMtbModuleInfoRequest(
			1,
			{[](uint8_t module, Mtb::ModuleInfo, void*) {
				std::cout << "Got module " << module << " info" << std::endl;
			}},
			{[](Mtb::CmdError cmdError, void*) {
				std::cout << "Error callback: "+Mtb::cmdErrorToStr(cmdError).toStdString()+"!" << std::endl;
			}}
		)
	);

	mtbusb.send(
		Mtb::CmdMtbModuleFwWriteFlashStatusRequest(
			1,
			{[](uint8_t addr, Mtb::FwWriteFlashStatus, void*) { std::cout << "Ok" << std::endl; }},
			{[](Mtb::CmdError cmdError, void*) {
				std::cout << "Error callback: "+Mtb::cmdErrorToStr(cmdError).toStdString()+"!" << std::endl;
			}}
		)
	);*/
}

void DaemonCoreApplication::mtbUsbLog(QString message, Mtb::LogLevel loglevel) {
	(void)loglevel;
	std::cout << "[" << QTime::currentTime().toString("hh:mm:ss,zzz").toStdString()
	          << "] " << message.toStdString() << std::endl;
}

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
