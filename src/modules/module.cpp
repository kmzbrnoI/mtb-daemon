#include <QJsonArray>
#include "module.h"
#include "main.h"
#include "logging.h"
#include "utils.h"

MtbModule::MtbModule(uint8_t addr) : address(addr), name("Module "+QString::number(addr)) {}

MtbModuleType MtbModule::moduleType() const { return this->type; }

bool MtbModule::isActive() const { return this->active; }
bool MtbModule::isRebooting() const { return this->rebooting.rebooting; }
bool MtbModule::isBeacon() const { return this->beacon; }
bool MtbModule::isActivating() const { return this->activating; }

QJsonObject MtbModule::moduleInfo(bool, bool) const {
	QJsonObject obj;
	obj["address"] = this->address;
	obj["name"] = this->name;
	obj["type_code"] = static_cast<int>(this->type);
	obj["type"] = moduleTypeToStr(this->type);
	obj["fw_upgrading"] = this->isFirmwareUpgrading();
	obj["beacon"] = this->beacon;

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
		obj["bootloader_version"] = this->busModuleInfo.bootloader_version();
		obj["warning"] = (this->busModuleInfo.warning);
		obj["error"] = (this->busModuleInfo.error);
	} else {
		if (this->isRebooting())
			obj["state"] = "rebooting";
		else
			obj["state"] = "inactive";
	}

	return obj;
}

void MtbModule::mtbBusActivate(Mtb::ModuleInfo moduleInfo) {
	this->activationsRemaining = MTB_MODULE_ACTIVATIONS;
	this->busModuleInfo = moduleInfo;
	this->rebooting.activatedByMtbUsb = true;
	this->type = static_cast<MtbModuleType>(moduleInfo.type);
}

void MtbModule::mtbBusLost() {
	this->mlog("Lost", Mtb::LogLevel::Info);
	this->activating = false;
	this->active = false;
	this->activationsRemaining = 0;
	this->sendModuleInfo();
}

void MtbModule::mtbUsbDisconnected() {
	this->active = false;
}

void MtbModule::mtbBusInputsChanged(const std::vector<uint8_t>&) {
}

void MtbModule::mtbBusDiagStateChanged(const std::vector<uint8_t>& data) {
	if (data.size() < 1)
		return;
	this->mtbBusDiagStateChanged(data[0] & 1, data[0] & 2);
}

void MtbModule::mtbBusDiagStateChanged(bool isError, bool isWarning) {
	bool changed = false;

	{
		changed |= (isError != this->busModuleInfo.error);
		this->busModuleInfo.error = isError;
	}

	{
		changed |= (isWarning != this->busModuleInfo.warning);
		this->busModuleInfo.warning = isWarning;
	}

	if (changed) {
		this->sendModuleInfo();
		this->mlog("Module diag state changed: warning="+QString::number(this->busModuleInfo.warning)+", error="+
		           QString::number(this->busModuleInfo.error), Mtb::LogLevel::Warning);
	}
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
	else if (command == "module_beacon")
		this->jsonBeacon(socket, request);
	else if (command == "module_diag")
		this->jsonGetDiag(socket, request);
	else if (command == "module_set_address")
		this->jsonSetAddress(socket, request);
}

void MtbModule::jsonSetOutput(QTcpSocket *socket, const QJsonObject &request) {
	sendError(socket, request, MTB_MODULE_UNSUPPORTED_COMMAND, "This module does not support output setting!");
}

void MtbModule::jsonSetConfig(QTcpSocket*, const QJsonObject &json) {
	if (json.contains("type_code"))
		this->type = static_cast<MtbModuleType>(json["type_code"].toInt());
	if (json.contains("name"))
		this->name = json["name"].toString();
}

void MtbModule::jsonSetAddress(QTcpSocket *socket, const QJsonObject &request) {
	if (this->isFirmwareUpgrading()) {
		sendError(socket, request, MTB_MODULE_UPGRADING_FW, "Firmware of module is being upgraded!");
		return;
	}
	if (this->busModuleInfo.inBootloader()) {
		sendError(socket, request, MTB_MODULE_IN_BOOTLOADER, "Module is in bootloader!");
		return;
	}
	if (this->isConfigSetting()) {
		sendError(socket, request, MTB_MODULE_CONFIG_SETTING, "Configuration of module is being changed!");
		return;
	}

	uint8_t newaddr = request["new_address"].toInt(1);
	mtbusb.send(
		Mtb::CmdMtbModuleChangeAddr(
			this->address, newaddr,
			{[socket, request](uint8_t, void*) {
				QJsonObject response = jsonOkResponse(request);
				server.send(socket, response);
			}},
			{[socket, request](Mtb::CmdError error, void*) {
				sendError(socket, request, error);
			}}
		)
	);
}

void MtbModule::jsonUpgradeFw(QTcpSocket *socket, const QJsonObject &request) {
	sendError(socket, request, MTB_MODULE_UNSUPPORTED_COMMAND, "This module does not support firmware upgrading!");
}

void MtbModule::jsonReboot(QTcpSocket *socket, const QJsonObject &request) {
	if (this->isRebooting())
		return sendError(socket, request, MTB_MODULE_REBOOTING, "Already rebooting!");

	this->reboot(
		{[this, socket, request]() {
			this->mlog("Module successfully rebooted", Mtb::LogLevel::Info);
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
	case MtbModuleType::Unis10: return "MTB-UNIS";
	case MtbModuleType::Rc: return "MTB-RC";
	default: return "Unknown type";
	}
}

void MtbModule::sendInputsChanged(QJsonObject inputs) const {
	QJsonObject json{
		{"command", "module_inputs_changed"},
		{"type", "event"},
		{"module_inputs_changed", QJsonObject{
			{"address", this->address},
			{"type", moduleTypeToStr(this->type)},
			{"type_code", static_cast<int>(this->type)},
			{"inputs", inputs},
		}}
	};

	for (auto pair : subscribes[this->address]) {
		QTcpSocket* socket = pair.first;
		server.send(socket, json);
	}
}

void MtbModule::sendOutputsChanged(QJsonObject outputs, const std::vector<QTcpSocket*>& ignore) const {
	QJsonObject json{
		{"command", "module_outputs_changed"},
		{"type", "event"},
		{"module_outputs_changed", QJsonObject{
			{"address", this->address},
			{"type", moduleTypeToStr(this->type)},
			{"type_code", static_cast<int>(this->type)},
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

void MtbModule::sendModuleInfo(QTcpSocket *ignore, bool sendConfig) const {
	QJsonObject json{
		{"command", "module"},
		{"type", "event"},
		{"module", this->moduleInfo(true, sendConfig)},
	};

	for (auto pair : subscribes[this->address]) {
		QTcpSocket *socket = pair.first;
		if (socket != ignore)
			server.send(socket, json);
	}
}

void MtbModule::resetOutputsOfClient(QTcpSocket*) {}

void MtbModule::clientDisconnected(QTcpSocket *socket) {
	if ((this->configWriting.has_value()) && (this->configWriting.value().socket == socket))
		this->configWriting->socket = nullptr;
	if ((this->fwUpgrade.fwUpgrading.has_value()) && (this->fwUpgrade.fwUpgrading.value().socket == socket))
		this->fwUpgrade.fwUpgrading->socket = nullptr;
}

std::vector<QTcpSocket*> MtbModule::outputSetters() const {
	return {};
}

bool MtbModule::isConfigSetting() const { return this->configWriting.has_value(); }

void MtbModule::jsonGetDiag(QTcpSocket *socket, const QJsonObject &request) {
	uint8_t dv_num = 0;
	if (request.contains("DVnum")) {
		dv_num = request["DVnum"].toInt();
	} else {
		std::optional<uint8_t> dv = this->StrToDV(request["DVkey"].toString());
		if (!dv)
			return sendError(socket, request, MTB_INVALID_DV, "Unknown DV!");
		dv_num = dv.value();
	}

	mtbusb.send(
		Mtb::CmdMtbModuleGetDiagValue(
			this->address, dv_num,
			{[this, socket, request](uint8_t, uint8_t dvi, const std::vector<uint8_t> &data, void*) {
				QJsonObject response = jsonOkResponse(request);
				response["DVnum"] = dvi;
				response["DVkey"] = this->DVToStr(dvi);
				response["DVvalue"] = this->dvRepr(dvi, data);

				QJsonArray dataAr;
				for (const uint8_t byte : data)
					dataAr.push_back(byte);
				response["DVvalueRaw"] = dataAr;

				server.send(socket, response);

				if (dvi == Mtb::DVCommon::State) {
					this->mtbBusDiagStateChanged(data);
				} else if ((dvi == Mtb::DVCommon::Errors) || (dvi == Mtb::DVCommon::Warnings)) {
					bool anyNonZero = false;
					for (uint8_t byte : data)
						if (byte != 0)
							anyNonZero = true;

					if (dvi == Mtb::DVCommon::Errors)
						this->mtbBusDiagStateChanged(anyNonZero, this->busModuleInfo.warning);
					else if (dvi == Mtb::DVCommon::Warnings)
						this->mtbBusDiagStateChanged(this->busModuleInfo.error, anyNonZero);
				}
			}},
			{[socket, request](Mtb::CmdError error, void*) {
				sendError(socket, request, error);
			}}
		)
	);
}

/* Firmware Upgrade ----------------------------------------------------------*/

std::map<size_t, std::vector<uint8_t>> MtbModule::parseFirmware(const QJsonObject &json) {
	std::map<size_t, std::vector<uint8_t>> result;

	for (const QString &key : json.keys()) {
		size_t addr = key.toInt();
		const QString &dataStr = json[key].toString();
		std::vector<uint8_t> data;
		for (int i = 0; i < dataStr.size(); i += 2)
			data.push_back(dataStr.mid(i, 2).toInt(nullptr, 16));

		for (size_t i = 0; i < data.size(); i++) {
			size_t block = (addr+i) / MtbModule::FwUpgrade::BLOCK_SIZE;
			size_t offset = (addr+i) % MtbModule::FwUpgrade::BLOCK_SIZE;
			if (result.find(block) == result.end())
				result.emplace(block, std::vector<uint8_t>(MtbModule::FwUpgrade::BLOCK_SIZE, 0xFF));

			result[block][offset] = data[i];
		}
	}

	return result;
}

bool MtbModule::isFirmwareUpgrading() const { return this->fwUpgrade.fwUpgrading.has_value(); }

void MtbModule::fwUpgdInit() {
	this->mlog("Initializing firmware upgrade", Mtb::LogLevel::Info);
	this->sendModuleInfo(this->fwUpgrade.fwUpgrading.value().socket);

	if (this->busModuleInfo.inBootloader()) {
		// Skip rebooting to bootloader
		this->mlog("Module already in bootloader, skipping reboot", Mtb::LogLevel::Info);
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
	if (!this->busModuleInfo.inBootloader())
		return this->fwUpgdError("Module rebooted, but not in bootloader!");

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

	if (this->fwUpgrade.toWrite == this->fwUpgrade.data.end())
		return fwUpgdAllWritten();

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
	this->mlog("Firmware programming finished, rebooting module...", Mtb::LogLevel::Info);

	this->reboot(
		{[this]() { this->fwUpgdRebooted(); }},
		{[this]() { this->fwUpgdError("Unable to reboot module back to operation!"); }}
	);
}

void MtbModule::fwUpgdRebooted() {
	if (this->busModuleInfo.inBootloader())
		return this->fwUpgdError("Module rebooted after upgrade, but it stayed in bootloader!");

	this->mlog("Firmware successfully upgraded", Mtb::LogLevel::Info);

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
	this->sendModuleInfo(request.socket, true);
}

void MtbModule::reboot(std::function<void()> onOk, std::function<void()> onError) {
	if (this->isRebooting())
		return;

	this->rebooting.rebooting = true;
	this->rebooting.onOk = onOk;
	this->rebooting.onError = onError;
	this->rebooting.activatedByMtbUsb = false;
	this->mtbBusLost();

	this->sendModuleInfo(nullptr, true);

	mtbusb.send(
		Mtb::CmdMtbModuleReboot(
			this->address,
			{[this](uint8_t, void*) {
				QTimer::singleShot(1000, [this](){
					if (this->rebooting.activatedByMtbUsb)
						return;
					mtbusb.send(
						Mtb::CmdMtbModuleInfoRequest(
							this->address,
							{[this](uint8_t, Mtb::ModuleInfo info, void*) { this->mtbBusActivate(info); }},
							{[this](Mtb::CmdError, void*) {
								this->rebooting.rebooting = false;
								this->sendModuleInfo(nullptr, true);
								this->rebooting.onError();
							}}
						)
					);
				});
			}},
			{[this](Mtb::CmdError, void*) {
				this->rebooting.rebooting = false;
				this->sendModuleInfo(nullptr, true);
				this->rebooting.onError();
			}}
		)
	);
}

void MtbModule::fullyActivated() {
	this->activating = false;
	this->activationsRemaining = 0;
	this->active = true;
	this->mlog("Activated", Mtb::LogLevel::Info);
	this->sendModuleInfo(nullptr, true);

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
				json["response"] = response;
				server.send(socket, json);
				return true;
			}},
			{[socket, request](Mtb::CmdError error, void*) {
				sendError(socket, request, static_cast<int>(error)+0x1000, Mtb::cmdErrorToStr(error));
			}}
		)
	);
}

void MtbModule::jsonBeacon(QTcpSocket *socket, const QJsonObject &request) {
	bool beacon = request["beacon"].toBool();

	mtbusb.send(
		Mtb::CmdMtbModuleBeacon(
			this->address, beacon,
			{[this, socket, request, beacon](uint8_t, void*) {
				this->beacon = beacon;
				QJsonObject response = jsonOkResponse(request);
				response["beacon"] = beacon;
				server.send(socket, response);
			}},
			{[socket, request](Mtb::CmdError error, void*) {
				sendError(socket, request, error);
			}}
		)
	);
}

void MtbModule::allOutputsReset() {}

void MtbModule::reactivateCheck() {}

void MtbModule::activationError(Mtb::CmdError) {
	this->activating = false;
	if (this->activationsRemaining > 0) {
		this->activationsRemaining--;
		if (this->activationsRemaining <= 0)
			this->mlog("Out of attempts for activation!", Mtb::LogLevel::Error);
	}
}

void MtbModule::mlog(const QString& message, Mtb::LogLevel loglevel) const {
	log("Module "+QString::number(this->address)+": "+message, loglevel);
}

QJsonObject MtbModule::dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const {
	if (data.size() < 1)
		return {};

	switch (dvi) {
		case Mtb::DVCommon::Version:
			return {{"version", QString::number((data[0] >> 4) & 0x0F) + "." + QString::number(data[0] & 0x0F)}};

		case Mtb::DVCommon::State:
			return {
				{"warnings", static_cast<bool>(data[0] & 2)},
				{"errors", static_cast<bool>(data[0] & 1)},
			};

		case Mtb::DVCommon::Uptime: {
			int uptime = 0;
			for (size_t i = 0; i < data.size(); i++) {
				uptime <<= 8;
				uptime |= data[i];
			}
			return {{"uptime_seconds", uptime}};
		}

		case Mtb::DVCommon::Warnings:
			return {
				{"extrf", static_cast<bool>(data[0] & 0x1)},
				{"borf", static_cast<bool>(data[0] & 0x2)},
				{"wdrf", static_cast<bool>(data[0] & 0x4)},
				{"timer_miss", static_cast<bool>(data[0] & 0x10)},
				{"vcc_oscilating", static_cast<bool>(data[0] & 0x20)},
			};

		case Mtb::DVCommon::MtbBusReceived:
		case Mtb::DVCommon::MtbBusBadCrc:
		case Mtb::DVCommon::MtbBusSent:
			if (data.size() == 4)
				return {{Mtb::DVCommonToStr(dvi), static_cast<int>(pack<uint32_t>(data))}};
			break;
	}

	return {};
}

QString MtbModule::DVToStr(uint8_t dv) const {
	return Mtb::DVCommonToStr(dv);
}

std::optional<uint8_t> MtbModule::StrToDV(const QString &str) const {
	return Mtb::StrToDVCommon(str);
}
