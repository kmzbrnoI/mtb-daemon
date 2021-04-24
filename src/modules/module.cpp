#include <QJsonArray>
#include "module.h"
#include "../main.h"

MtbModule::MtbModule(uint8_t addr) : address(addr), name("Module "+QString::number(addr)) {}

MtbModuleType MtbModule::moduleType() const { return this->type; }

bool MtbModule::isActive() const { return this->active; }

bool MtbModule::isRebooting() const { return this->rebooting.rebooting; }

QJsonObject MtbModule::moduleInfo(bool, bool) const {
	QJsonObject obj;
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
		else if (this->isFirmwareUpgrading())
			obj["state"] = "fw_upgrading";
		else
			obj["state"] = "active";

		obj["firmware_version"] = this->busModuleInfo.fw_version();
		obj["protocol_version"] = this->busModuleInfo.proto_version();
	} else {
		if (this->isRebooting())
			obj["state"] = "rebooting";
		else
			obj["state"] = "inactive";
	}

	return obj;
}

void MtbModule::mtbBusActivate(Mtb::ModuleInfo moduleInfo) {
	this->busModuleInfo = moduleInfo;
	this->rebooting.activatedByMtbUsb = true;
	if (this->type == MtbModuleType::Uknown)
		this->type = static_cast<MtbModuleType>(moduleInfo.type);
}

void MtbModule::mtbBusLost() {
	this->active = false;
	this->sendModuleInfo();
}

void MtbModule::mtbUsbDisconnected() {
	this->active = false;
}

void MtbModule::mtbBusInputsChanged(const std::vector<uint8_t>&) {
}

void MtbModule::jsonCommand(QTcpSocket *socket, const QJsonObject &request) {
	QString command = request["command"].toString();

	if (command == "module_set_outputs")
		this->jsonSetOutput(socket, request);
	else if (command == "module_set_config")
		this->jsonSetConfig(socket, request);
	else if (command == "module_upgrade_fw")
		this->jsonUpgradeFw(socket, request);
	else if (command == "module_reboot")
		this->jsonReboot(socket, request);
	else if (command == "module_specific_command")
		this->jsonSpecificCommand(socket, request);
}

void MtbModule::jsonSetOutput(QTcpSocket*, const QJsonObject&) {}

void MtbModule::jsonSetConfig(QTcpSocket*, const QJsonObject &json) {
	if (json.contains("type"))
		this->type = static_cast<MtbModuleType>(json["type"].toInt());
	if (json.contains("name"))
		this->name = json["name"].toString();
}

void MtbModule::jsonUpgradeFw(QTcpSocket*, const QJsonObject&) {}

void MtbModule::jsonReboot(QTcpSocket *socket, const QJsonObject &request) {
	if (this->isRebooting()) {
		sendError(socket, request, MTB_MODULE_REBOOTING, "Already rebooting!");
		return;
	}

	this->reboot(
		{[this, socket, request]() {
			log("Module successfully rebooted", Mtb::LogLevel::Info);
			QJsonObject response{
				{"command", "module_reboot"},
				{"type", "response"},
				{"status", "ok"},
				{"address", this->address},
			};
			if (request.contains("id"))
				response["id"] = request["id"];
			server.send(socket, response);
		}},
		{[this, socket, request]() {
			sendError(socket, request, MTB_BUS_NO_RESPONSE,
			          "Unable to reboot module");
			this->mtbBusLost();
		}}
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

void MtbModule::sendInputsChanged(QJsonObject inputs) const {
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
                                   const std::vector<QTcpSocket*>& ignore) const {
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

void MtbModule::loadConfig(const QJsonObject &json) {
	this->name = json["name"].toString();
	this->type = static_cast<MtbModuleType>(json["type"].toInt());
}

void MtbModule::saveConfig(QJsonObject &json) const {
	json["name"] = this->name;
	json["type"] = static_cast<int>(this->type);
}

void MtbModule::sendModuleInfo(QTcpSocket *ignore) const {
	QJsonObject json{
		{"command", "module"},
		{"type", "event"},
		{"module", this->moduleInfo(true, false)},
	};

	for (auto pair : subscribes[this->address]) {
		QTcpSocket *socket = pair.first;
		if (socket != ignore)
			server.send(socket, json);
	}
}

void MtbModule::clientDisconnected(QTcpSocket *socket) {
	if ((this->configWriting.has_value()) && (this->configWriting.value().socket == socket))
		this->configWriting.reset();
}

bool MtbModule::isConfigSetting() const { return this->configWriting.has_value(); }

/* Firmware Upgrade ----------------------------------------------------------*/

std::map<size_t, std::vector<uint8_t>> MtbModule::parseFirmware(const QJsonObject &json) {
	std::map<size_t, std::vector<uint8_t>> result;

	for (const QString &key : json.keys()) {
		size_t addr = key.toInt();
		const QString &dataStr = json[key].toString();
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

bool MtbModule::isFirmwareUpgrading() const { return this->fwUpgrade.fwUpgrading.has_value(); }

void MtbModule::fwUpgdInit() {
	log("Initializing firmware upgrade of module "+QString::number(this->address), Mtb::LogLevel::Info);
	this->sendModuleInfo(this->fwUpgrade.fwUpgrading.value().socket);

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
		fwUpgdAllWritten();
		return;
	}

	uint16_t fwAddr = (*this->fwUpgrade.toWrite).first * MtbModule::FwUpgrade::BLOCK_SIZE;
	const std::vector<uint8_t> &fwBlob = (*this->fwUpgrade.toWrite).second;

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

void MtbModule::fwUpgdError(const QString &error, size_t code) {
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
	this->sendModuleInfo(request.socket);
}

void MtbModule::fwUpgdAllWritten() {
	log("Firmware programming finished, rebooting module...", Mtb::LogLevel::Info);

	this->reboot(
		{[this]() { this->fwUpgdRebooted(); }},
		{[this]() { this->fwUpgdError("Unable to reboot module back to operation!"); }}
	);
}

void MtbModule::fwUpgdRebooted() {
	if (this->busModuleInfo.inBootloader()) {
		this->fwUpgdError("Module rebooted after upgrade, but it stayed in bootloader!");
		return;
	}

	log("Firmware successfully upgraded", Mtb::LogLevel::Info);

	QJsonObject json{
		{"command", "module_upgrade_fw"},
		{"type", "response"},
		{"status", "ok"},
		{"address", this->address},
	};
	const ServerRequest request = this->fwUpgrade.fwUpgrading.value();
	if (request.id.has_value())
		json["id"] = static_cast<int>(request.id.value());
	server.send(request.socket, json);

	this->fwUpgrade.fwUpgrading.reset();
	this->fwUpgrade.data.clear();
	this->sendModuleInfo(request.socket);
}

void MtbModule::reboot(std::function<void()> onOk, std::function<void()> onError) {
	if (this->isRebooting())
		return;

	this->rebooting.rebooting = true;
	this->rebooting.onOk = onOk;
	this->rebooting.onError = onError;
	this->rebooting.activatedByMtbUsb = false;
	this->mtbBusLost();

	this->sendModuleInfo();

	mtbusb.send(
		Mtb::CmdMtbModuleReboot(
			this->address,
			{[this](uint8_t, void*) {
				QTimer::singleShot(500, [this](){
					if (this->rebooting.activatedByMtbUsb)
						return;
					mtbusb.send(
						Mtb::CmdMtbModuleInfoRequest(
							this->address,
							{[this](uint8_t, Mtb::ModuleInfo info, void*) { this->mtbBusActivate(info); }},
							{[this](Mtb::CmdError, void*) {
								this->rebooting.rebooting = false;
								this->sendModuleInfo();
								this->rebooting.onError();
							}}
						)
					);
				});
			}},
			{[this](Mtb::CmdError, void*) {
				this->rebooting.rebooting = false;
				this->sendModuleInfo();
				this->rebooting.onError();
			}}
		)
	);
}

void MtbModule::fullyActivated() {
	this->active = true;
	log("Module "+QString::number(this->address)+" activated", Mtb::LogLevel::Info);
	this->sendModuleInfo();

	if (this->isRebooting()) {
		this->rebooting.rebooting = false;
		this->rebooting.onOk();
	}
}

void MtbModule::jsonSpecificCommand(QTcpSocket *socket, const QJsonObject &request) {
	const QJsonArray &dataAr = request["data"].toArray();
	std::vector<uint8_t> data;
	for (const auto var : dataAr)
		data.push_back(var.toInt());

	mtbusb.send(
		Mtb::CmdMtbModuleSpecific(
			this->address, data,
			{[request, socket](uint8_t, Mtb::MtbBusRecvCommand command, const std::vector<uint8_t>& data, void*) -> bool {
				QJsonObject json = jsonOkResponse(request);
				QJsonObject response = json["response"].toObject();
				response["command_code"] = static_cast<int>(command);
				QJsonArray dataAr;
				for (const uint8_t byte : data)
					dataAr.push_back(byte);
				response["data"] = dataAr;
				server.send(socket, json);
				return true;
			}},
			{[socket, request](Mtb::CmdError error, void*) {
				sendError(socket, request, static_cast<int>(error)+0x1000, Mtb::cmdErrorToStr(error));
			}}
		)
	);
}
