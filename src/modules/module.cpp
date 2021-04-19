#include <QJsonArray>
#include "module.h"
#include "../main.h"

QJsonObject MtbModule::moduleInfo(bool) const {
	QJsonObject obj;
	obj["active"] = this->active;
	obj["address"] = this->address;
	obj["name"] = this->name;
	obj["type_code"] = static_cast<int>(this->type);
	obj["type"] = moduleTypeToStr(this->type);

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

	QJsonObject response {
		{"command", "module_activated"},
		{"type", "event"},
		{"modules", QJsonArray{this->address}},
	};

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

QString moduleTypeToStr(MtbModuleType type) {
	switch (type) {
	case MtbModuleType::Univ2ir: return "MTB-UNI v2 IR";
	case MtbModuleType::Univ2noIr: return "MTB-UNI v2";
	case MtbModuleType::Univ40: return "MTB-UNI v4";
	case MtbModuleType::Univ42: return "MTB-UNI v4";
	default: return "Unknown type";
	}
}

void MtbModule::sendInputsChanged(QJsonArray inputs) const {
	QJsonObject json{
		{"command", "module_inputs_changed"},
		{"type", "event"},
		{"module_inputs_changed", QJsonObject{
			{"address", this->address},
			{"inputs", inputs},
		}}
	};

	for (auto pair : subscribes[this->address]) {
		QTcpSocket* socket = pair.first;
		server.send(*socket, json);
	}
}

void MtbModule::sendOutputsChanged(QJsonObject outputs,
                                   const std::vector<QTcpSocket*> ignore) const {
	QJsonObject json{
		{"command", "module_outputs_changed"},
		{"type", "event"},
		{"module_outputs_changed", QJsonObject{
			{"address", this->address},
			{"outputs", outputs},
		}}
	};

	for (auto pair : subscribes[this->address]) {
		QTcpSocket* socket = pair.first;
		if (std::find(ignore.begin(), ignore.end(), socket) == ignore.end())
			server.send(*socket, json);
	}
}
