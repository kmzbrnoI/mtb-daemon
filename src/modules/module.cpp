#include <QJsonArray>
#include "module.h"
#include "../main.h"

QJsonObject MtbModule::moduleInfo(bool state) const {
	QJsonObject obj;
	obj["address"] = this->address;
	obj["type_code"] = static_cast<int>(this->type);
	obj["active"] = this->active;

	if (this->active) {
		if (this->busModuleInfo.bootloader_unint)
			obj["state"] = "bootloader_err";
		else if (this->busModuleInfo.bootloader_int)
			obj["state"] = "bootloader_int";
		else
			obj["state"] = "active";

		obj["firmware_version"] = this->busModuleInfo.fw_version();
		obj["protocol_version"] = this->busModuleInfo.proto_version();
	} else {
		obj["state"] = "inactive";
	}

	return obj;
}

void MtbModule::mtbBusActivate(Mtb::ModuleInfo moduleInfo) {
	this->busModuleInfo = moduleInfo;
}

void MtbModule::mtbBusLost() {
	this->active = false;

	QJsonObject response;
	QJsonArray modules{this->address};
	response["command"] = "module_activated";
	response["type"] = "event";
	response["modules"] = modules;
	for (const auto& pair : subscribes[this->address])
		server.send(*pair.first, response);
}

void MtbModule::mtbBusInputsChanged(const std::vector<uint8_t>) {
}

void MtbModule::jsonCommand(QTcpSocket* socket, const QJsonObject& request) {
	QString command = request["command"].toString();

	if (command == "module_set_output")
		this->jsonSetOutput(socket, request);
	else if (command == "module_set_config")
		this->jsonSetConfig(socket, request);
	else if (command == "module_uprade_fw")
		this->jsonUpgradeFw(socket, request);
}

void MtbModule::jsonSetOutput(QTcpSocket*, const QJsonObject&) {}
void MtbModule::jsonSetConfig(QTcpSocket*, const QJsonObject&) {}
void MtbModule::jsonUpgradeFw(QTcpSocket*, const QJsonObject&) {}
