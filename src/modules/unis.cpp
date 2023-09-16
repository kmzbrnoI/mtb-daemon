#include <QJsonArray>
#include <QJsonObject>
#include "unis.h"
#include "../mtbusb/mtbusb.h"
#include "../main.h"
#include "../errors.h"

MtbUnis::MtbUnis(uint8_t addr) : MtbModule(addr) {
	std::fill(this->whoSetOutput.begin(), this->whoSetOutput.end(), nullptr);
}

/* JSON Module Info --------------------------------------------------------- */

QJsonObject MtbUnis::moduleInfo(bool state, bool config) const {
	QJsonObject response = MtbModule::moduleInfo(state, config);

	QJsonObject unis;

	if ((config) && (this->config.has_value()))
		unis["config"] = this->config.value().json();

	if (state && this->active && !this->busModuleInfo.inBootloader()) {
		unis["state"] = QJsonObject{
			{"outputs", outputsToJson(this->outputsConfirmed)},
			{"inputs", inputsToJson(this->inputs)},
			{"inputsPacked", this->inputs},
		};
	}

	response[moduleTypeToStr(this->type)] = unis;
	return response;
}

/* Json Set Outputs --------------------------------------------------------- */

void MtbUnis::jsonSetOutput(QTcpSocket *socket, const QJsonObject &request) {
	if (!this->active) {
		sendError(socket, request, MTB_MODULE_FAILED, "Cannot set output of inactive module!");
		return;
	}
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

	QJsonObject outputs = request["outputs"].toObject();
	bool send = (this->outputsWant == this->outputsConfirmed);
	bool changed = false;

	for (const auto &key : outputs.keys()) {
		size_t port = key.toInt();
		if (port > UNIS_OUT_CNT)
			continue;
		uint8_t code = jsonOutputToByte(outputs[key].toObject());
		if (code != this->outputsWant[port]) {
			changed = true;
			if ((this->whoSetOutput[port] != nullptr) && (this->whoSetOutput[port] != socket))
				this->mlog("Multiple clients set same output: "+QString::number(port), Mtb::LogLevel::Warning);
			this->whoSetOutput[port] = socket;
		}
		this->outputsWant[port] = code;
	}

	if (changed) {
		std::optional<size_t> id;
		if (request.contains("id"))
			id = request["id"].toInt();
		this->setOutputsWaiting.push_back({socket, id});
		if (send)
			this->setOutputs();
	} else {
		QJsonObject response = jsonOkResponse(request);
		response["outputs"] = this->outputsToJson(this->outputsConfirmed);
		server.send(socket, response);
	}
}

uint8_t MtbUnis::jsonOutputToByte(const QJsonObject &json) {
	if (json["type"] == "plain") {
		return (json["value"].toInt() > 0) ? 1 : 0;
	}
	if (json["type"] == "s-com") {
		uint8_t value = json["value"].toInt();
		if (value > 127)
			value = 127;
		return value | 0x80;
	}
	if (json["type"] == "flicker") {
		uint8_t value = flickPerMinToMtbUnisValue(json["value"].toInt());
		if (value == 0)
			value = 1;
		return value | 0x40;
	}
	return 0;
}

void MtbUnis::setOutputs() {
	this->setOutputsSent = this->setOutputsWaiting;
	this->setOutputsWaiting.clear();

	mtbusb.send(
		Mtb::CmdMtbModuleSetOutput(
			this->address, this->mtbBusOutputsData(),
			{[this](uint8_t, const std::vector<uint8_t>& data, void*) {
				this->mtbBusOutputsSet(data);
			}},
			{[this](Mtb::CmdError error, void*) { this->mtbBusOutputsNotSet(error); }}
		)
	);
}

void MtbUnis::mtbBusOutputsSet(const std::vector<uint8_t>& data) {
	this->outputsConfirmed = this->moduleOutputsData(data);

	// TODO: check if output really set?

	// Report ok callback to clients
	std::vector<QTcpSocket*> ignore;
	for (const ServerRequest &sr : this->setOutputsSent) {
		QJsonObject response{
			{"command", "module_set_outputs"},
			{"type", "response"},
			{"status", "ok"},
			{"address", this->address},
			{"outputs", this->outputsToJson(this->outputsConfirmed)},
		};
		if (sr.id.has_value())
			response["id"] = static_cast<int>(sr.id.value());
		server.send(sr.socket, response);
		ignore.push_back(sr.socket);
	}
	this->setOutputsSent.clear();

	// Report outputs changed event to other clients
	this->sendOutputsChanged(outputsToJson(this->outputsConfirmed), ignore);

	// Send next outputs
	if (this->setOutputsWaiting.empty()) {
		if (this->isFirmwareUpgrading())
			this->fwUpgdInit();
	} else {
		this->setOutputs();
	}
}

QJsonObject MtbUnis::outputsToJson(const std::array<uint8_t, UNIS_OUT_CNT> &outputs) {
	QJsonObject result;
	for (size_t i = 0; i < UNIS_IO_CNT; i++) {
		QJsonObject output;

		if ((outputs[i] & 0x80) > 0) {
			output["type"] = "s-com";
			output["value"] = outputs[i] & 0x7F;
		} else if ((outputs[i] & 0x40) > 0) {
			output["type"] = "flicker";
			output["value"] = static_cast<int>(flickMtbUnisToPerMin(outputs[i] & 0xF));
		} else {
			output["type"] = "plain";
			output["value"] = outputs[i] & 1;
		}

		result[QString::number(i)] = output;
	}
	for (size_t i = UNIS_IO_CNT; i < UNIS_OUT_CNT; i++) {
		QJsonObject output;
		output["type"] = "plain";
		output["value"] = outputs[i] & 1;
		result[QString::number(i)] = output;
	}
	return result;
}

QJsonObject MtbUnis::inputsToJson(uint16_t inputs) {
	QJsonArray json;
	uint16_t _inputs = inputs;
	for (size_t i = 0; i < UNIS_IN_CNT; i++) {
		json.push_back(static_cast<bool>(_inputs&1));
		_inputs >>= 1;
	}
	return {{"full", json}, {"packed", inputs}};
}

void MtbUnis::mtbBusOutputsNotSet(Mtb::CmdError error) {
	// Report err callback to clients
	for (const ServerRequest &sr : this->setOutputsSent) {
		QJsonObject response{
			{"command", "module_set_outputs"},
			{"type", "response"},
			{"status", "error"},
			{"error", jsonError(error)},
		};
		if (sr.id.has_value())
			response["id"] = static_cast<int>(sr.id.value());
		server.send(sr.socket, response);
	}
	this->setOutputsSent.clear();

	// TODO: mark module as failed? Do anything else?
	this->outputsConfirmed = this->outputsWant;

	// Send next outputs
	if (this->setOutputsWaiting.empty()) {
		if (this->isFirmwareUpgrading())
			this->fwUpgdInit();
	} else {
		this->setOutputs();
	}
}

/* Json Set Config ---------------------------------------------------------- */

void MtbUnis::jsonSetConfig(QTcpSocket *socket, const QJsonObject &request) {
	if (this->configWriting.has_value()) {
		sendError(socket, request, MTB_MODULE_ALREADY_WRITING, "Another client is writing config now!");
		return;
	}
	if (this->isFirmwareUpgrading()) {
		sendError(socket, request, MTB_MODULE_UPGRADING_FW, "Firmware of module is being upgraded!");
		return;
	}
	if (this->busModuleInfo.inBootloader()) {
		sendError(socket, request, MTB_MODULE_IN_BOOTLOADER, "Module is in bootloader!");
		return;
	}
	this->mlog("json set config", Mtb::LogLevel::Info);
	MtbModule::jsonSetConfig(socket, request);

	std::optional<MtbUnisConfig> oldConfig = this->configToWrite;
	this->configToWrite.emplace(MtbUnisConfig(request["config"].toObject()));
	this->configWriting = ServerRequest(socket, request);

	if ((this->active) && (oldConfig != this->configToWrite)) {
		std::vector<uint8_t> cfg = this->configToWrite.value().serializeForMtbUsb();
		this->mlog("from json to module: serialize config, length: "+QString::number(cfg.size()), Mtb::LogLevel::Info);
		mtbusb.send(
			Mtb::CmdMtbModuleSetConfig(
				this->address, cfg,
				{[this](uint8_t, void*) { this->mtbBusConfigWritten(); }},
				{[this](Mtb::CmdError error, void*) { this->mtbBusConfigNotWritten(error); }}
			)
		);
	} else {
		this->mtbBusConfigWritten();
	}
}

void MtbUnis::mtbBusConfigWritten() {
	this->mlog("mtbBusConfigWritten()", Mtb::LogLevel::Info);
	this->config = this->configToWrite;
	const ServerRequest request = this->configWriting.value();
	this->configWriting.reset();
	this->sendModuleInfo(request.socket, true);

	QJsonObject response{
		{"command", "module_set_config"},
		{"type", "response"},
		{"status", "ok"},
		{"address", this->address},
	};
	if (request.id.has_value())
		response["id"] = static_cast<int>(request.id.value());
	server.send(request.socket, response);

	if (this->isFirmwareUpgrading())
		this->fwUpgdInit();
}

void MtbUnis::mtbBusConfigNotWritten(Mtb::CmdError error) {
	const ServerRequest request = this->configWriting.value();
	this->configWriting.reset();

	QJsonObject response{
		{"command", "module_set_config"},
		{"type", "response"},
		{"status", "error"},
		{"address", this->address},
		{"error", jsonError(error)},
	};
	if (request.id.has_value())
		response["id"] = static_cast<int>(request.id.value());
	server.send(request.socket, response);

	if (this->isFirmwareUpgrading())
		this->fwUpgdInit();
}

/* Json Upgrade Firmware ---------------------------------------------------- */

void MtbUnis::jsonUpgradeFw(QTcpSocket *socket, const QJsonObject &request) {
	if (this->isFirmwareUpgrading()) {
		sendError(socket, request, MTB_MODULE_UPGRADING_FW, "Firmware is already being upgraded!");
		return;
	}

	this->fwUpgrade.fwUpgrading = ServerRequest(socket, request);
	this->fwUpgrade.data = parseFirmware(request["firmware"].toObject());
	this->alignFirmware(this->fwUpgrade.data, UNIS_PAGE_SIZE);

	if (!this->configWriting.has_value() && this->setOutputsSent.empty())
		this->fwUpgdInit();
}

void MtbUnis::alignFirmware(std::map<size_t, std::vector<uint8_t>> &fw, size_t pageSize) {
	const size_t blocksPerPage = pageSize / MtbModule::FwUpgrade::BLOCK_SIZE;
	std::vector<size_t> blocks;
	for (auto const &imap : fw)
		blocks.push_back(imap.first);
	for (size_t block : blocks) {
		size_t page = block / blocksPerPage;
		for (size_t i = 0; i < blocksPerPage; i++) {
			if (fw.find((page*blocksPerPage)+i) == fw.end())
				fw.emplace((page*blocksPerPage) + i, std::vector<uint8_t>(MtbModule::FwUpgrade::BLOCK_SIZE, 0xFF));
		}
	}
}

/* -------------------------------------------------------------------------- */

void MtbUnis::resetOutputsOfClient(QTcpSocket *socket) {
	MtbModule::resetOutputsOfClient(socket);

	bool send = false;
	if (this->config.has_value()) {
		for (size_t i = 0; i < UNIS_OUT_CNT; i++) {
			if (this->whoSetOutput[i] == socket) {
				this->outputsWant[i] = this->config.value().outputsSafe[i];
				this->whoSetOutput[i] = nullptr;
				send = true;
			}
		}
	}

	if (send) {
		this->setOutputsWaiting.push_back({nullptr});
		if (this->setOutputsSent.empty())
			this->setOutputs();
	}
}

std::vector<QTcpSocket*> MtbUnis::outputSetters() const {
	std::vector<QTcpSocket*> result;
	for (QTcpSocket* socket : this->whoSetOutput)
		if ((socket != nullptr) && (std::find(result.begin(), result.end(), socket) == result.end()))
			result.push_back(socket);
	return result;
}

std::vector<uint8_t> MtbUnis::mtbBusOutputsData() const {
	// Set outputs data based on diff in this->outputsWant
	const std::array<uint8_t, UNIS_OUT_CNT> &outputs = this->outputsWant;
	std::vector<uint8_t> data {0, 0, 0, 0, 0, 0};

	for (size_t i = 0; i < UNIS_IO_CNT; i++) {
		if ((outputs[i] & 0xC0) > 0) {
			// Non-plain output
			if (i < 8)
				data[1] |= (1 << i);
			else
				data[0] |= (1 << (i-8));
			data.push_back(outputs[i]);
		} else {
			// Plain outputs
			if (outputs[i] > 0) {
				if (i < 8)
					data[5] |= (1 << i);
				else
					data[4] |= (1 << (i-8));
			}
		}
	}
	for (size_t i = UNIS_IO_CNT; i < UNIS_OUT_CNT; i++) {
		// virtual outputs
		if (outputs[i] > 0) {
			if (i < (16+8))
				data[3] |= (1 << (i-16));
			else
				data[2] |= (1 << (i-24));
		}
	}
	QString msg;
	msg.clear();
	for (size_t i=0; i<data.size(); i++) {
		msg.append(QString::number(data[i], 16));
		msg.append(" ");
	}
	return data;
}

std::array<uint8_t, UNIS_OUT_CNT> MtbUnis::moduleOutputsData(const std::vector<uint8_t> &mtbBusData) {
	std::array<uint8_t, UNIS_OUT_CNT> result;
	if (mtbBusData.size() < 6) {
		return result; // TODO: report error?
	}

	uint16_t mask = (mtbBusData[0] << 8) | mtbBusData[1];
	uint32_t fullOutputs = (mtbBusData[2] << 24) | (mtbBusData[3] << 16) | (mtbBusData[4] << 8) | mtbBusData[5];
	size_t j = 6;
	// real outputs - full status mask
	for (size_t i = 0; i < UNIS_IO_CNT; i++) {
		if (((mask >> i) & 1) == 0) {
			result[i] = (fullOutputs >> i) & 1;
		} else if (j < mtbBusData.size()) {
			result[i] = mtbBusData[j];
			j++;
		}
	}
	// virtual outputs - only binary states
	for (size_t i = UNIS_IO_CNT; i < UNIS_OUT_CNT; i++) {
		result[i] = (fullOutputs >> i) & 1;
	}

	return result;
}

void MtbUnis::allOutputsReset() {
	for (size_t i = 0; i < UNIS_OUT_CNT; i++) {
		this->outputsWant[i] = this->config.has_value() ? this->config.value().outputsSafe[i] : 0;
		this->outputsConfirmed[i] = this->outputsWant[i];
		this->whoSetOutput[i] = nullptr;
	}
	this->sendOutputsChanged(outputsToJson(this->outputsConfirmed), {});
}

/* MTB-UNI activation ---------------------------------------------------------
 A) Module configuration was previously laoded from file:
 * 1) General information are read
 * 2) Config is SET
 * 3) Inputs are get
 * 4) Outputs are reset
 B) Module configuration was NOT previously loaded from file:
 * 1) General information are read
 * 2) Config is GET
 * 3) Inputs are get
 * 4) Outputs are reset
 */

void MtbUnis::mtbBusActivate(Mtb::ModuleInfo info) {
	// Mtb module activated, got info → set config, then get inputs
	MtbModule::mtbBusActivate(info);
	if (info.inBootloader()) {
		// In bootloader → mark as active, don't do anything else
		this->mlog("Module is in bootloader!", Mtb::LogLevel::Info);
		this->outputsReset();
		return;
	}
	this->activate();
}

void MtbUnis::activate() {
	this->activating = true;
	if (this->busModuleInfo.warning || this->busModuleInfo.error)
		this->mlog("Module warning="+QString::number(this->busModuleInfo.warning)+", error="+
		           QString::number(this->busModuleInfo.error), Mtb::LogLevel::Warning);

	if (this->config.has_value()) {
		this->mlog("Config previously loaded from file, setting to module...", Mtb::LogLevel::Info);
		mtbusb.send(
			Mtb::CmdMtbModuleSetConfig(
				this->address, this->config.value().serializeForMtbUsb(),
				{[this](uint8_t, void*) { this->configSet(); }},
				{[this](Mtb::CmdError error, void*) {
					this->mlog("Unable to set module config.", Mtb::LogLevel::Error);
					this->activationError(error);
				}}
			)
		);
	} else {
		this->mlog("Config of this module not loaded from file, getting config from module...", Mtb::LogLevel::Info);
		mtbusb.send(
			Mtb::CmdMtbModuleGetConfig(
				this->address,
				{[this](uint8_t, const std::vector<uint8_t>& data, void*) {
					this->mlog("activate: got config from module, length: "+QString::number(data.size()), Mtb::LogLevel::Info);
					this->config.emplace(MtbUnisConfig(data));
					this->configSet();
				}},
				{[this](Mtb::CmdError error, void*) {
					this->mlog("Unable to get module config.", Mtb::LogLevel::Error);
					this->activationError(error);
				}}
			)
		);
	}
}

void MtbUnis::configSet() {
	// Mtb module activation: got info & config set → read inputs
	mtbusb.send(
		Mtb::CmdMtbModuleGetInputs(
			this->address,
			{[this](uint8_t, const std::vector<uint8_t>& data, void*) { this->inputsRead(data); }},
			{[this](Mtb::CmdError error, void*) {
				this->mlog("Unable to get new module inputs.", Mtb::LogLevel::Error);
				this->activationError(error);
			}}
		)
	);
}

void MtbUnis::inputsRead(const std::vector<uint8_t> &data) {
	// Mtb module activation: got info & config set & inputs read → mark module as active
	this->storeInputsState(data);
	mtbusb.send(
		Mtb::CmdMtbModuleResetOutputs(
			this->address,
			{[this](uint8_t, void*) { this->outputsReset(); }},
			{[this](Mtb::CmdError error, void*) {
				this->mlog("Unable to reset new module outputs.", Mtb::LogLevel::Error);
				this->activationError(error);
			}}
		)
	);
}

void MtbUnis::storeInputsState(const std::vector<uint8_t> &data) {
	if (data.size() >= 2)
		this->inputs = (data[0] << 8) | data[1];
}

void MtbUnis::outputsReset() {
	for (size_t i = 0; i < UNIS_OUT_CNT; i++) {
		this->outputsWant[i] = this->config.has_value() ? this->config.value().outputsSafe[i] : 0;
		this->outputsConfirmed[i] = this->outputsWant[i];
	}

	for (size_t i = 0; i < UNIS_OUT_CNT; i++)
		this->whoSetOutput[i] = nullptr;
	this->fullyActivated();
}

/* Inputs changed ----------------------------------------------------------- */

void MtbUnis::mtbBusInputsChanged(const std::vector<uint8_t> &data) {
	if (this->active || this->activating) {
		this->storeInputsState(data);
		this->sendInputsChanged(inputsToJson(this->inputs));
	}
}

void MtbUnis::mtbUsbDisconnected() {
	MtbModule::mtbUsbDisconnected();
	this->allOutputsReset();
	this->inputs = 0;
}

/* -------------------------------------------------------------------------- */

std::vector<uint8_t> MtbUnisConfig::serializeForMtbUsb() const {
	std::vector<uint8_t> result;
	std::copy(this->outputsSafe.begin(), this->outputsSafe.end(), std::back_inserter(result));
	for (size_t i = 0; i < 8; i++)
		result.push_back(this->inputsDelay[2*i] | (this->inputsDelay[2*i+1] << 4));
	result.push_back(this->servoEnabledMask & 0x3F);
	for (size_t i = 0; i < UNIS_SERVO_OUT_CNT; i++) {
		result.push_back(this->servoPosition[i]);
	}
	for (size_t i = 0; i < UNIS_SERVO_CNT; i++) {
		result.push_back(this->servoSpeed[i]);
	}
	return result;
}

QJsonObject MtbUnisConfig::json() const {
	QJsonObject result;
	{
		QJsonArray array;
		for (uint8_t output : this->outputsSafe) {
			QJsonObject outputs;
			if ((output & 0x80) > 0) {
				outputs["type"] = "s-com";
				outputs["value"] = output & 0x7F;
			} else if ((output & 0x40) > 0) {
				outputs["type"] = "flicker";
				outputs["value"] = output & 0x0F;
			} else {
				outputs["type"] = "plain";
				outputs["value"] = output & 1;
			}
			array.push_back(outputs);
		}
		result["outputsSafe"] = array;
	}

	{
		QJsonArray array;
		for (size_t delay : this->inputsDelay)
			array.push_back(delay/10.0);
		result["inputsDelay"] = array;
	}
	result["servoEnabledMask"] = this->servoEnabledMask;
	{
		QJsonArray array;
		for (uint16_t position : this->servoPosition)
			array.push_back(position);
		result["servoPosition"] = array;
	}
	{
		QJsonArray array;
		for (uint8_t speed : this->servoSpeed)
			array.push_back(speed);
		result["servoSpeed"] = array;
	}
	return result;
}

void MtbUnisConfig::fromJson(const QJsonObject &json) {
	const QJsonArray &jsonOutputsSafe = json["outputsSafe"].toArray();
	const QJsonArray &jsonInputsDelay = json["inputsDelay"].toArray();
	const QJsonArray &jsonServoPosition = json["servoPosition"].toArray();
	const QJsonArray &jsonServoSpeed = json["servoSpeed"].toArray();

	for (size_t i = 0; i < UNIS_IN_CNT; i++) {
		if (i < static_cast<size_t>(jsonInputsDelay.size()))
			this->inputsDelay[i] = jsonInputsDelay[i].toDouble()*10;
	}
	for (size_t i = 0; i < UNIS_OUT_CNT; i++) {
		if (i < static_cast<size_t>(jsonOutputsSafe.size()))
			this->outputsSafe[i] = MtbUnis::jsonOutputToByte(jsonOutputsSafe[i].toObject());
	}
	this->servoEnabledMask = json["servoEnabledMask"].toInt(0);
	for (size_t i = 0; i < UNIS_SERVO_OUT_CNT; i++) {
		if (i < static_cast<size_t>(jsonServoPosition.size()))
			this->servoPosition[i] = jsonServoPosition[i].toInt(127);
	}
	for (size_t i = 0; i < UNIS_SERVO_CNT; i++) {
		if (i < static_cast<size_t>(jsonServoSpeed.size()))
			this->servoSpeed[i] = jsonServoSpeed[i].toInt(20);
	}
}

void MtbUnisConfig::fromMtbUsb(const std::vector<uint8_t> &data) {
	if (data.size() < 67)
		return;
	uint8_t pos = 0;
	for (size_t i = 0; i < (UNIS_OUT_CNT); i++)
		this->outputsSafe[i] = data[pos+i];
	pos = UNIS_OUT_CNT;
	for (size_t i = 0; i < (UNIS_IO_CNT); i++)
		this->inputsDelay[i] = ((i%2 == 0) ? data[pos+i/2] : data[pos+i/2] >> 4) & 0x0F;
	pos += UNIS_IO_CNT/2;
	this->servoEnabledMask = data[pos];
	pos++;
	for (size_t i = 0; i < (UNIS_SERVO_OUT_CNT); i++) {
		this->servoPosition[i] = data[pos+i];
	}
	pos += UNIS_SERVO_OUT_CNT;
	for (size_t i = 0; i < (UNIS_SERVO_CNT); i++) {
		this->servoSpeed[i] = data[pos+i];
	}
	//pos += UNIS_SERVO_CNT;
}

uint8_t MtbUnis::flickPerMinToMtbUnisValue(size_t flickPerMin) {
	switch (flickPerMin) {
	case 60: return 1;
	case 120: return 2;
	case 180: return 3;
	case 240: return 4;
	case 300: return 5;
	case 600: return 6;
	case 33: return 7;
	case 66: return 8;
	default: return 0;
	}
}

size_t MtbUnis::flickMtbUnisToPerMin(uint8_t MtbUnisFlick) {
	switch (MtbUnisFlick) {
	case 1: return 60;
	case 2: return 120;
	case 3: return 180;
	case 4: return 240;
	case 5: return 300;
	case 6: return 600;
	case 7: return 33;
	case 8: return 66;
	default: return 0;
	}
}

void MtbUnis::reactivateCheck() {
	if ((!this->activating) && (this->activationsRemaining > 0) && (!this->active))
		this->activate();
}

/* Configuration ------------------------------------------------------------ */

void MtbUnis::loadConfig(const QJsonObject &json) {
	this->mlog("func loadConfig", Mtb::LogLevel::Info);
	MtbModule::loadConfig(json);
	this->config.emplace(MtbUnisConfig(json["config"].toObject()));
}

void MtbUnis::saveConfig(QJsonObject &json) const {
	this->mlog("func saveConfig", Mtb::LogLevel::Info);
	MtbModule::saveConfig(json);
	if (this->config.has_value())
		json["config"] = this->config.value().json();
}

/* Diagnostic Values -------------------------------------------------------- */

QJsonObject MtbUnis::dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const {
	if (data.size() < 1)
		return {};

	switch (dvi) {
		case Mtb::DV::Version:
			return {{"version", QString::number((data[0] >> 4) & 0x0F) + "." + QString::number(data[0] & 0x0F)}};

		case Mtb::DV::State:
			return {
				{"warnings", static_cast<bool>(data[0] & 2)},
				{"errors", static_cast<bool>(data[0] & 1)},
			};

		case Mtb::DV::Uptime: {
			int uptime = 0;
			for (size_t i = 0; i < data.size(); i++) {
				uptime <<= 8;
				uptime |= data[i];
			}
			return {{"uptime_seconds", uptime}};
		}

		case Mtb::DV::Warnings:
			return {
				{"extrf", static_cast<bool>(data[0] & 0x1)},
				{"borf", static_cast<bool>(data[0] & 0x2)},
				{"wdrf", static_cast<bool>(data[0] & 0x4)},
				{"timer_miss", static_cast<bool>(data[0] & 0x10)},
				{"vcc_oscilating", static_cast<bool>(data[0] & 0x20)},
			};

		case Mtb::DV::MCUVoltage: {
			if (data.size() < 2)
				return {};

			uint16_t raw = (data[0] << 8) | data[1];
			float value = (this->adcbg() * 1024) / raw;
			float value_min = (this->adcbg()*0.9 * 1024) / raw;
			float value_max = (this->adcbg()*1.1 * 1024) / raw;
			return {
				{"mcu_voltage", value},
				{"mcu_voltage_min", value_min},
				{"mcu_voltage_max", value_max},
				{"mcu_voltage_raw", raw},
			};
		}

		case Mtb::DV::MCUTemperature: {
			if (data.size() < 4)
				return {};

			uint16_t raw = (data[0] << 8) | data[1];
			int8_t ts_offset = data[2];
			uint8_t ts_gain = data[3];
			float temp = ((raw-(273+100-ts_offset))*128 / ts_gain) + 25;
			return {
				{"mcu_temp_celsius", temp},
				{"mcu_temp_raw", raw},
				{"mcu_ts_offset", ts_offset},
				{"mcu_ts_gain", ts_gain},
			};
		}
	}

	return {};
}

float MtbUnis::adcbg() const {
	return 1.22;
}
