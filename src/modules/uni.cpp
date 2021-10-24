#include <QJsonArray>
#include <QJsonObject>
#include "uni.h"
#include "../mtbusb/mtbusb.h"
#include "../main.h"
#include "../errors.h"

MtbUni::MtbUni(uint8_t addr) : MtbModule(addr) {
	std::fill(this->whoSetOutput.begin(), this->whoSetOutput.end(), nullptr);
}

bool MtbUni::isIrSupport() const { return this->type == MtbModuleType::Univ2ir; }

size_t MtbUni::pageSize() const {
	if ((this->type == MtbModuleType::Univ2ir) || (this->type == MtbModuleType::Univ2noIr))
		return 128;
	return 256;
}

/* JSON Module Info --------------------------------------------------------- */

QJsonObject MtbUni::moduleInfo(bool state, bool config, bool diag) const {
	QJsonObject response = MtbModule::moduleInfo(state, config, diag);

	QJsonObject uni{
		{"ir", this->isIrSupport()},
	};

	if (config)
		uni["config"] = this->config.json(this->isIrSupport(), false);

	if (state && this->active && !this->busModuleInfo.inBootloader()) {
		uni["state"] = QJsonObject{
			{"outputs", outputsToJson(this->outputsConfirmed)},
			{"inputs", inputsToJson(this->inputs)},
			{"inputsPacked", this->inputs},
		};
	}

	if (diag)
		uni["warnings"] = this->warnToJson();

	response[moduleTypeToStr(this->type)] = uni;
	return response;
}

/* Json Set Outputs --------------------------------------------------------- */

void MtbUni::jsonSetOutput(QTcpSocket *socket, const QJsonObject &request) {
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
		if (port > 15)
			continue;
		uint8_t code = jsonOutputToByte(outputs[key].toObject());
		if (code != this->outputsWant[port]) {
			changed = true;
			if ((this->whoSetOutput[port] != nullptr) && (this->whoSetOutput[port] != socket))
				this->mlog("Multiple clients set same output: "+QString::number(port),
				           Mtb::LogLevel::Warning);
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

uint8_t MtbUni::jsonOutputToByte(const QJsonObject &json) {
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
		uint8_t value = flickPerMinToMtbUniValue(json["value"].toInt());
		if (value == 0)
			value = 1;
		return value | 0x40;
	}
	return 0;
}

void MtbUni::setOutputs() {
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

void MtbUni::mtbBusOutputsSet(const std::vector<uint8_t>& data) {
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

QJsonObject MtbUni::outputsToJson(const std::array<uint8_t, UNI_IO_CNT> &outputs) {
	QJsonObject result;
	for (size_t i = 0; i < UNI_IO_CNT; i++) {
		QJsonObject output;

		if ((outputs[i] & 0x80) > 0) {
			output["type"] = "s-com";
			output["value"] = outputs[i] & 0x7F;
		} else if ((outputs[i] & 0x40) > 0) {
			output["type"] = "flicker";
			output["value"] = static_cast<int>(flickMtbUniToPerMin(outputs[i] & 0xF));
		} else {
			output["type"] = "plain";
			output["value"] = outputs[i] & 1;
		}

		result[QString::number(i)] = output;
	}
	return result;
}

QJsonObject MtbUni::inputsToJson(uint16_t inputs) {
	QJsonArray json;
	uint16_t _inputs = inputs;
	for (size_t i = 0; i < UNI_IO_CNT; i++) {
		json.push_back(static_cast<bool>(_inputs&1));
		_inputs >>= 1;
	}
	return {{"full", json}, {"packed", inputs}};
}

QJsonObject MtbUni::warnToJson() const {
	return {
		{"extrf", this->warnings.sep.extrf},
		{"borf", this->warnings.sep.borf},
		{"wdrf", this->warnings.sep.wdrf},
		{"missed_timer", this->warnings.sep.missed_timer},
	};
}

void MtbUni::mtbBusOutputsNotSet(Mtb::CmdError error) {
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

void MtbUni::jsonSetConfig(QTcpSocket *socket, const QJsonObject &request) {
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

	MtbModule::jsonSetConfig(socket, request);

	MtbUniConfig oldConfig = this->configToWrite;
	this->configToWrite.fromJson(request["config"].toObject());
	this->configWriting = ServerRequest(socket, request);

	if ((this->active) && (oldConfig != this->configToWrite)) {
		mtbusb.send(
			Mtb::CmdMtbModuleSetConfig(
				this->address, this->configToWrite.serializeForMtbUsb(this->isIrSupport()),
				{[this](uint8_t, void*) { this->mtbBusConfigWritten(); }},
				{[this](Mtb::CmdError error, void*) { this->mtbBusConfigNotWritten(error); }}
			)
		);
	} else {
		this->mtbBusConfigWritten();
	}
}

void MtbUni::mtbBusConfigWritten() {
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

void MtbUni::mtbBusConfigNotWritten(Mtb::CmdError error) {
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

void MtbUni::jsonUpgradeFw(QTcpSocket *socket, const QJsonObject &request) {
	if (!this->active) {
		sendError(socket, request, MTB_MODULE_FAILED, "Cannot upgrade FW of inactive module!");
		return;
	}
	if (this->isFirmwareUpgrading()) {
		sendError(socket, request, MTB_MODULE_UPGRADING_FW, "Firmware is already being upgraded!");
		return;
	}

	this->fwUpgrade.fwUpgrading = ServerRequest(socket, request);
	this->fwUpgrade.data = parseFirmware(request["firmware"].toObject());
	this->alignFirmware(this->fwUpgrade.data, this->pageSize());

	if (!this->configWriting.has_value() && this->setOutputsSent.empty())
		this->fwUpgdInit();
}

void MtbUni::alignFirmware(std::map<size_t, std::vector<uint8_t>> &fw, size_t pageSize) {
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

void MtbUni::resetOutputsOfClient(QTcpSocket *socket) {
	MtbModule::resetOutputsOfClient(socket);

	bool send = false;
	for (size_t i = 0; i < UNI_IO_CNT; i++) {
		if (this->whoSetOutput[i] == socket) {
			this->outputsWant[i] = this->config.outputsSafe[i];
			this->whoSetOutput[i] = nullptr;
			send = true;
		}
	}

	if (send) {
		this->setOutputsWaiting.push_back({nullptr});
		if (this->setOutputsSent.empty())
			this->setOutputs();
	}
}

std::vector<QTcpSocket*> MtbUni::outputSetters() const {
	std::vector<QTcpSocket*> result;
	for (QTcpSocket* socket : this->whoSetOutput)
		if ((socket != nullptr) && (std::find(result.begin(), result.end(), socket) == result.end()))
			result.push_back(socket);
	return result;
}

std::vector<uint8_t> MtbUni::mtbBusOutputsData() const {
	// Set outputs data based on diff in this->outputsWant
	const std::array<uint8_t, UNI_IO_CNT> &outputs = this->outputsWant;
	std::vector<uint8_t> data {0, 0, 0, 0};

	for (size_t i = 0; i < UNI_IO_CNT; i++) {
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
					data[3] |= (1 << i);
				else
					data[2] |= (1 << (i-8));
			}
		}
	}

	return data;
}

std::array<uint8_t, UNI_IO_CNT> MtbUni::moduleOutputsData(const std::vector<uint8_t> &mtbBusData) {
	std::array<uint8_t, UNI_IO_CNT> result;
	if (mtbBusData.size() < 4)
		return result; // TODO: report error?

	uint16_t mask = (mtbBusData[0] << 8) | mtbBusData[1];
	uint16_t fullOutputs = (mtbBusData[2] << 8) | mtbBusData[3];
	size_t j = 4;
	for (size_t i = 0; i < UNI_IO_CNT; i++) {
		if ((mask&1) == 0) {
			result[i] = fullOutputs&1;
		} else if (j < mtbBusData.size()) {
			result[i] = mtbBusData[j];
			j++;
		}

		mask >>= 1;
		fullOutputs >>= 1;
	}

	return result;
}

void MtbUni::allOutputsReset() {
	for (size_t i = 0; i < UNI_IO_CNT; i++) {
		this->outputsWant[i] = this->config.outputsSafe[i];
		this->outputsConfirmed[i] = this->outputsWant[i];
		this->whoSetOutput[i] = nullptr;
	}
	this->sendOutputsChanged(outputsToJson(this->outputsConfirmed), {});
}

/* MTB-UNI activation ---------------------------------------------------------
 A) Module configuration was previously laoded from file:
 * 1) General information are read
    - Errors | warnings present? Read detailed diagnostic info.
 * 2) Config is SET
 * 3) Inputs are get
 * 4) Outputs are reset
 B) Module configuration was NOT previously loaded from file:
 * 1) General information are read
    - Errors | warnings present? Read detailed diagnostic info.
 * 2) Config is GET
 * 3) Inputs are get
 * 4) Outputs are reset
 */

void MtbUni::mtbBusActivate(Mtb::ModuleInfo info) {
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

void MtbUni::activate() {
	this->activating = true;

	if (this->busModuleInfo.warning || this->busModuleInfo.error) {
		this->mlog("Module warning="+QString::number(this->busModuleInfo.warning)+", error="+
		           QString::number(this->busModuleInfo.error), Mtb::LogLevel::Warning);
		mtbusb.send(
			Mtb::CmdMtbModuleGetDiag(
				this->address,
				{[this](uint8_t, const std::vector<uint8_t> &data, void*) {
					this->mtbBusDiagChanged(data);
					this->diagGot();
				}},
				{[this](Mtb::CmdError error, void*) {
					this->mlog("Unable to get module diagnostics.", Mtb::LogLevel::Error);
					this->activationError(error);
				}}
			)
		);
	} else {
		this->diagGot();
	}
}

void MtbUni::diagGot() {
	if (this->configLoaded) {
		this->mlog("Config previously loaded from file, setting to module...", Mtb::LogLevel::Info);
		mtbusb.send(
			Mtb::CmdMtbModuleSetConfig(
				this->address, this->config.serializeForMtbUsb(this->isIrSupport()),
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
					this->config.fromMtbUsb(data);
					this->configLoaded = true;
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

void MtbUni::configSet() {
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

void MtbUni::inputsRead(const std::vector<uint8_t> &data) {
	// Mtb module activation: got info & config set & inputs read → mark module as active
	this->storeInputsState(data);

	mtbusb.send(
		Mtb::CmdMtbModuleResetOutputs(
			this->address,
			{[this](uint8_t, void*) { this->outputsReset(); }},
			{[this](Mtb::CmdError error, void*) {
				this->mlog("Unable to reset new module outputs.",
				    Mtb::LogLevel::Error);
				this->activationError(error);
			}}
		)
	);
}

void MtbUni::storeInputsState(const std::vector<uint8_t> &data) {
	if (data.size() >= 2)
		this->inputs = (data[0] << 8) | data[1];
}

void MtbUni::outputsReset() {
	this->outputsWant = this->config.outputsSafe;
	this->outputsConfirmed = this->outputsWant;

	for (size_t i = 0; i < UNI_IO_CNT; i++)
		this->whoSetOutput[i] = nullptr;

	this->fullyActivated();
}

/* Inputs changed ----------------------------------------------------------- */

void MtbUni::mtbBusInputsChanged(const std::vector<uint8_t> &data) {
	if (this->active || this->activating) {
		this->storeInputsState(data);
		this->sendInputsChanged(inputsToJson(this->inputs));
	}
}

void MtbUni::mtbUsbDisconnected() {
	MtbModule::mtbUsbDisconnected();
	this->allOutputsReset();
	this->inputs = 0;
}

void MtbUni::mtbBusDiagChanged(const std::vector<uint8_t> &data) {
	if (data.size() < 1)
		return;

	uint8_t errorsCount = data[0];
	if ((errorsCount == 0) && (data.size() > 1))
		this->warnings.all = data[1];

	MtbModule::mtbBusDiagChanged(data);
}

/* -------------------------------------------------------------------------- */

std::vector<uint8_t> MtbUniConfig::serializeForMtbUsb(bool withIrs) const {
	std::vector<uint8_t> result;
	std::copy(this->outputsSafe.begin(), this->outputsSafe.end(), std::back_inserter(result));
	for (size_t i = 0; i < 8; i++)
		result.push_back(this->inputsDelay[2*i] | (this->inputsDelay[2*i+1] << 4));

	if (withIrs) {
		result.push_back(this->irs >> 8);
		result.push_back(this->irs & 0xFF);
	}

	return result;
}

QJsonObject MtbUniConfig::json(bool withIrs, bool file) const {
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

	if (withIrs) {
		QJsonArray array;
		uint16_t irs = this->irs;
		for (size_t i = 0; i < UNI_IO_CNT; i++) {
			array.push_back(static_cast<bool>(irs & 1));
			irs >>= 1;
		}
		result["irs"] = array;
		if (!file)
			result["irsPacked"] = this->irs;
	}

	return result;
}

void MtbUniConfig::fromJson(const QJsonObject &json) {
	const QJsonArray &jsonOutputsSafe = json["outputsSafe"].toArray();
	const QJsonArray &jsonInputsDelay = json["inputsDelay"].toArray();
	const QJsonArray &jsonIrs = json["irs"].toArray();
	this->irs = 0;
	for (size_t i = 0; i < UNI_IO_CNT; i++) {
		if (i < static_cast<size_t>(jsonOutputsSafe.size()))
			this->outputsSafe[i] = MtbUni::jsonOutputToByte(jsonOutputsSafe[i].toObject());
		if (i < static_cast<size_t>(jsonInputsDelay.size()))
			this->inputsDelay[i] = jsonInputsDelay[i].toDouble()*10;
		if (i < static_cast<size_t>(jsonIrs.size())) {
			if (jsonIrs[i].toBool())
				this->irs |= (1 << i);
		}
	}
}

void MtbUniConfig::fromMtbUsb(const std::vector<uint8_t> &data) {
	if (data.size() < 24)
		return;
	for (size_t i = 0; i < UNI_IO_CNT; i++)
		this->outputsSafe[i] = data[i];
	for (size_t i = 0; i < UNI_IO_CNT; i++)
		this->inputsDelay[i] = ((i%2 == 0) ? data[i/2] : data[i/2] >> 4) & 0x0F;

	if (data.size() >= 26)
		this->irs = (data[24] << 8) | data[25];
	else
		this->irs = 0;
}

uint8_t MtbUni::flickPerMinToMtbUniValue(size_t flickPerMin) {
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

size_t MtbUni::flickMtbUniToPerMin(uint8_t mtbUniFlick) {
	switch (mtbUniFlick) {
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

void MtbUni::reactivateCheck() {
	if ((!this->activating) && (this->activationsRemaining > 0) && (!this->active))
		this->activate();
}

/* Configuration ------------------------------------------------------------ */

void MtbUni::loadConfig(const QJsonObject &json) {
	MtbModule::loadConfig(json);
	this->config.fromJson(json["config"].toObject());
	this->configLoaded = true;
}

void MtbUni::saveConfig(QJsonObject &json) const {
	MtbModule::saveConfig(json);
	if (this->configLoaded)
		json["config"] = this->config.json(this->isIrSupport(), true);
}
