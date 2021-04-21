#include <QJsonArray>
#include "module.h"
#include "../main.h"

MtbModule::MtbModule(uint8_t addr) : address(addr), name("Module "+QString::number(addr))
{}

MtbModuleType MtbModule::moduleType() const {
	return this->type;
}

bool MtbModule::isActive() const {
	return this->active;
}

QJsonObject MtbModule::moduleInfo(bool) const {
	QJsonObject obj;
	obj["active"] = this->active;
	obj["address"] = this->address;
	obj["name"] = this->name;
	obj["type_code"] = static_cast<int>(this->type);
	obj["type"] = moduleTypeToStr(this->type);
	obj["fw_upgrading"] = this->isFirmwareUpgrading();

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
	if (this->type == MtbModuleType::Uknown)
		this->type = static_cast<MtbModuleType>(moduleInfo.type);
}

void MtbModule::mtbBusLost() {
	this->active = false;
	this->fwUpgrade.fwUpgrading.reset();
	this->configWriting.reset();

	QJsonObject json{
		{"command", "module_deactivated"},
		{"type", "event"},
		{"modules", QJsonArray{this->address}},
	};

	for (const auto& pair : subscribes[this->address])
		server.send(pair.first, json);
}

void MtbModule::mtbUsbDisconnected() {
	this->active = false;
}

void MtbModule::mtbBusInputsChanged(const std::vector<uint8_t>) {
}

void MtbModule::jsonCommand(QTcpSocket* socket, const QJsonObject& request) {
	QString command = request["command"].toString();

	if (command == "module_set_outputs")
		this->jsonSetOutput(socket, request);
	else if (command == "module_set_config")
		this->jsonSetConfig(socket, request);
	else if (command == "module_upgrade_fw")
		this->jsonUpgradeFw(socket, request);
	else if (command == "module_reboot")
		this->jsonReboot(socket, request);
}

void MtbModule::jsonSetOutput(QTcpSocket*, const QJsonObject&) {}

void MtbModule::jsonSetConfig(QTcpSocket*, const QJsonObject& json) {
	if (json.contains("type"))
		this->type = static_cast<MtbModuleType>(json["type"].toInt());
	if (json.contains("name"))
		this->name = json["name"].toString();
}

void MtbModule::jsonUpgradeFw(QTcpSocket*, const QJsonObject&) {}

void MtbModule::jsonReboot(QTcpSocket* socket, const QJsonObject& request) {
	mtbusb.send(
		Mtb::CmdMtbModuleReboot(
			this->address,
			{[socket, request](uint8_t, void*) {
				QJsonObject response{
					{"command", "module_reboot"},
					{"type", "response"},
					{"status", "ok"},
				};
				if (request.contains("id"))
					response["id"] = request["id"];
				server.send(socket, response);
			}},
			{[socket, request](Mtb::CmdError, void*) {
				sendError(socket, request, MTB_MODULE_NOT_ANSWERED_CMD_GIVING_UP,
				          "Module did not answer reboot command");
			}}
		)
	);
}

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
		server.send(socket, json);
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
			server.send(socket, json);
	}
}

void MtbModule::loadConfig(const QJsonObject& json) {
	this->name = json["name"].toString();
	this->type = static_cast<MtbModuleType>(json["type"].toInt());
}

void MtbModule::saveConfig(QJsonObject& json) const {
	json["name"] = this->name;
	json["type"] = static_cast<int>(this->type);
}

void MtbModule::sendChanged(QTcpSocket* ignore) const {
	QJsonObject json{
		{"command", "module_changed"},
		{"type", "event"},
		{"module_changed", QJsonObject{
			{"address", this->address},
			// TODO: send full module status?
		}}
	};

	for (auto pair : subscribes[this->address]) {
		QTcpSocket* socket = pair.first;
		if (socket != ignore)
			server.send(socket, json);
	}
}

void MtbModule::clientDisconnected(QTcpSocket* socket) {
	if ((this->configWriting.has_value()) && (this->configWriting.value().socket == socket))
		this->configWriting.reset();
}

bool MtbModule::isConfigSetting() const {
	return this->configWriting.has_value();
}

/* Firmware Upgrade ----------------------------------------------------------*/

std::map<size_t, std::vector<uint8_t>> MtbModule::parseFirmware(const QJsonObject& json) {
	std::map<size_t, std::vector<uint8_t>> result;

	for (const QString& key : json.keys()) {
		size_t addr = key.toInt();
		const QString& dataStr = json[key].toString();
		std::vector<uint8_t> data;
		for (int i = 0; i < dataStr.size(); i += 2)
				data.push_back(dataStr.mid(i, 2).toInt(nullptr, 16));

		size_t block = addr / MtbModule::FwUpgrade::BLOCK_SIZE;
		size_t offset = addr % MtbModule::FwUpgrade::BLOCK_SIZE;
		if (result.find(block) == result.end())
			result.emplace(block, std::vector<uint8_t>(MtbModule::FwUpgrade::BLOCK_SIZE, 0xFF));
		for (size_t i = 0; i < data.size(); i++)
			result[block][i+offset] = data[i];
	}

	return result;
}

bool MtbModule::isFirmwareUpgrading() const {
	return this->fwUpgrade.fwUpgrading.has_value();
}

void MtbModule::fwUpgdInit() {
	log("Initializing firmware upgrade of module "+QString::number(this->address), Mtb::LogLevel::Info);
	this->sendChanged(this->fwUpgrade.fwUpgrading.value().socket);

	if (this->busModuleInfo.inBootloader()) {
		// Skip rebooting to bootloader
		log("Module already in bootloader, skipping reboot", Mtb::LogLevel::Info);
		this->fwUpgdGotInfo(this->busModuleInfo);
		return;
	}

	// Reboot to bootloader
	mtbusb.send(
		Mtb::CmdMtbModuleFwUpgradeReq(
			this->address,
			{[this](uint8_t, void*) { this->fwUpgdReqAck(); }},
			{[this](Mtb::CmdError, void*) { this->fwUpgdError("Unable to reboot module to bootloader!"); }}
		)
	);
}

void MtbModule::fwUpgdReqAck() {
	// Wait for module to reboot & initialize communication
	// Check if module is in bootloader
	QTimer::singleShot(200, [this](){
		mtbusb.send(
			Mtb::CmdMtbModuleInfoRequest(
				this->address,
				{[this](uint8_t, Mtb::ModuleInfo info, void*) { this->fwUpgdGotInfo(info); }},
				{[this](Mtb::CmdError, void*) { this->fwUpgdError("Unable to get rebooted module information"); }}
			)
		);
	});
}

void MtbModule::fwUpgdGotInfo(Mtb::ModuleInfo info) {
	this->busModuleInfo = info;
	if (!this->busModuleInfo.inBootloader()) {
		this->fwUpgdError("Module rebooted, but not in bootloader!");
		return;
	}

	this->fwUpgrade.toWrite = this->fwUpgrade.data.begin();
	this->fwUpgdGetStatus();
}

void MtbModule::fwUpgdGetStatus() {
	mtbusb.send(
		Mtb::CmdMtbModuleFwWriteFlashStatusRequest(
			this->address,
			{[this](uint8_t, Mtb::FwWriteFlashStatus status, void*) { this->fwUpgdGotStatus(status); }},
			{[this](Mtb::CmdError, void*) { this->fwUpgdError("Unable to get write flash status"); }}
		)
	);
}

void MtbModule::fwUpgdGotStatus(Mtb::FwWriteFlashStatus status) {
	if (status == Mtb::FwWriteFlashStatus::WritingFlash) {
		this->fwUpgdGetStatus();
		return;
	}

	if (this->fwUpgrade.toWrite == this->fwUpgrade.data.end()) {
		log("Whole firmware sent", Mtb::LogLevel::Info);
		return;
	}

	// TODO: page is counted in uint16_t for ATmega. Move the calculation to module types somehow
	uint16_t fwAddr = (*this->fwUpgrade.toWrite).first * MtbModule::FwUpgrade::BLOCK_SIZE;
	const std::vector<uint8_t>& fwBlob = (*this->fwUpgrade.toWrite).second;

	log("Writing page from address 0x"+QString::number(fwAddr, 16)+"...", Mtb::LogLevel::Commands);

	mtbusb.send(
		Mtb::CmdMtbModuleFwWriteFlash(
			this->address, fwAddr, fwBlob,
			{[this](uint8_t, void*) { this->fwUpgdGetStatus(); }},
			{[this](Mtb::CmdError error, void*) {
				if (error == Mtb::CmdError::BadAddress)
					this->fwUpgdError("Bad address!");
				else
					this->fwUpgdError("Unable to write flash!");
			}}
		)
	);
	++(this->fwUpgrade.toWrite);
}

void MtbModule::fwUpgdError(const QString& error, size_t code) {
	QJsonObject json{
		{"command", "module_upgrade_fw"},
		{"type", "response"},
		{"status", "error"},
		{"error", jsonError(code, error)},
	};
	const ServerRequest request = this->fwUpgrade.fwUpgrading.value();
	if (request.id.has_value())
		json["id"] = static_cast<int>(request.id.value());
	server.send(request.socket, json);

	this->fwUpgrade.fwUpgrading.reset();
	this->fwUpgrade.data.clear();
	this->sendChanged(request.socket);
}
