#include <QJsonArray>
#include <QJsonObject>
#include "led.h"
#include "mtbusb.h"
#include "main.h"
#include "errors.h"
#include "utils.h"

MtbLed::MtbLed(uint8_t addr) : MtbModule(addr) {
	std::fill(this->whoSetOutput.begin(), this->whoSetOutput.end(), nullptr);
}

/* JSON Module Info --------------------------------------------------------- */

QJsonObject MtbLed::moduleInfo(bool state, bool config) const {
	QJsonObject response = MtbModule::moduleInfo(state, config);
	QJsonObject led;

	if ((config) && (this->config.has_value()))
		led["config"] = this->config.value().json();

	if (state && this->active && !this->busModuleInfo.inBootloader()) {
		led["state"] = QJsonObject{
			{"outputs", ioStateToJson(this->outputsConfirmed)},
			{"inputs", ioStateToJson(this->inputs)},
		};
	}

	response[moduleTypeToStr(this->type)] = led;
	return response;
}

/* Json Set Outputs --------------------------------------------------------- */

void MtbLed::jsonSetOutput(QTcpSocket *socket, const QJsonObject &request) {
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

	QJsonObject outputs = QJsonSafe::safeObject(request, "outputs");
	QMap<size_t, bool> ports; // state per port

	// Validate ports
	bool ok;
	for (const auto &key : outputs.keys()) {
		int port = key.toInt(&ok);
		if ((!ok) || (port < 0) || (port >= static_cast<int>(LED_IO_CNT))) {
			sendError(socket, request, MTB_MODULE_INVALID_PORT, "Invalid port: "+key);
			return;
		}

		try {
			ports[port] = QJsonSafe::safeBool(outputs[key]);
		} catch (const JsonParseError& e) {
			sendError(socket, request, MTB_MODULE_INVALID_PORT, "Invalid port "+key+" content: "+e.what());
			return;
		}
	}

	bool send = (this->outputsWant == this->outputsConfirmed);
	bool changed = false;

	for (const auto &key : outputs.keys()) {
		size_t port = key.toInt();
		if (ports[port] != this->outputsWant[port]) {
			changed = true;
			if ((this->whoSetOutput[port] != nullptr) && (this->whoSetOutput[port] != socket))
				this->mlog("Multiple clients set same output: "+QString::number(port),
				           Mtb::LogLevel::Warning);
			this->whoSetOutput[port] = socket;
		}
		this->outputsWant[port] = ports[port];
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
		response["outputs"] = this->ioStateToJson(this->outputsConfirmed);
		server.send(socket, response);
	}
}

void MtbLed::setOutputs() {
	this->setOutputsSent = this->setOutputsWaiting;
	this->setOutputsWaiting.clear();

	mtbusb.send(
		Mtb::CmdMtbModuleSetOutput(
			this->address, this->ioToMtb(this->outputsWant),
			{[this](uint8_t, const std::vector<uint8_t>& data, void*) {
				this->mtbBusOutputsSet(data);
			}},
			{[this](Mtb::CmdError error, void*) { this->mtbBusOutputsNotSet(error); }}
		)
	);
}

void MtbLed::mtbBusOutputsSet(const std::vector<uint8_t>& data) {
	this->outputsConfirmed = this->mtbDataToIo(data);

	// TODO: check if output really set?

	// Report ok callback to clients
	std::vector<QTcpSocket*> ignore;
	for (const ServerRequest &sr : this->setOutputsSent) {
		QJsonObject response{
			{"command", "module_set_outputs"},
			{"type", "response"},
			{"status", "ok"},
			{"address", this->address},
			{"outputs", this->ioStateToJson(this->outputsConfirmed)},
		};
		if (sr.id.has_value())
			response["id"] = static_cast<int>(sr.id.value());
		server.send(sr.socket, response);
		ignore.push_back(sr.socket);
	}
	this->setOutputsSent.clear();

	// Report outputs changed event to other clients
	this->sendOutputsChanged(ioStateToJson(this->outputsConfirmed), ignore);

	// Send next outputs
	if (this->setOutputsWaiting.empty()) {
		if (this->isFirmwareUpgrading())
			this->fwUpgdInit();
	} else {
		this->setOutputs();
	}
}

QJsonObject MtbLed::ioStateToJson(const std::array<bool, LED_IO_CNT> &state) {
	QJsonArray json;
	uint32_t packed = 0;
	for (size_t i = 0; i < state.size(); i++) {
		json.push_back(state[i]);
		if (state[i])
			packed |= (1 << i);
	}
	return {{"full", json}, {"packed", static_cast<int>(packed)}};
}

void MtbLed::mtbBusOutputsNotSet(Mtb::CmdError error) {
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

void MtbLed::jsonSetConfig(QTcpSocket *socket, const QJsonObject &request) {
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

	// Check validity first
	MtbLedConfig newConfig;
	if (request.contains("config")) // allow to create empty module with empty config
		newConfig = MtbLedConfig(QJsonSafe::safeObject(request, "config"));

	MtbModule::jsonSetConfig(socket, request);

	std::optional<MtbLedConfig> oldConfig = this->configToWrite;
	this->configToWrite.emplace(newConfig);
	this->configWriting = ServerRequest(socket, request);

	if ((this->active) && (oldConfig != this->configToWrite)) {
		mtbusb.send(
			Mtb::CmdMtbModuleSetConfig(
				this->address, this->configToWrite.value().serializeForMtbUsb(),
				{[this](uint8_t, void*) { this->mtbBusConfigWritten(); }},
				{[this](Mtb::CmdError error, void*) { this->mtbBusConfigNotWritten(error); }}
			)
		);
	} else {
		this->mtbBusConfigWritten();
	}
}

void MtbLed::mtbBusConfigWritten() {
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

void MtbLed::mtbBusConfigNotWritten(Mtb::CmdError error) {
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

void MtbLed::jsonUpgradeFw(QTcpSocket *socket, const QJsonObject &request) {
	if (this->isFirmwareUpgrading()) {
		sendError(socket, request, MTB_MODULE_UPGRADING_FW, "Firmware is already being upgraded!");
		return;
	}

	this->fwUpgrade.fwUpgrading = ServerRequest(socket, request);
	this->fwUpgrade.data = parseFirmware(QJsonSafe::safeObject(request, "firmware"));
	this->alignFirmware(this->fwUpgrade.data, PAGE_SIZE);

	if (!this->configWriting.has_value() && this->setOutputsSent.empty())
		this->fwUpgdInit();
}

/* -------------------------------------------------------------------------- */

void MtbLed::resetOutputsOfClient(QTcpSocket *socket) {
	MtbModule::resetOutputsOfClient(socket);

	bool send = false;
	if (this->config.has_value()) {
		for (size_t i = 0; i < LED_IO_CNT; i++) {
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

std::vector<QTcpSocket*> MtbLed::outputSetters() const {
	std::vector<QTcpSocket*> result;
	for (QTcpSocket* socket : this->whoSetOutput)
		if ((socket != nullptr) && (std::find(result.begin(), result.end(), socket) == result.end()))
			result.push_back(socket);
	return result;
}

std::vector<uint8_t> MtbLed::ioToMtb(const std::array<bool, LED_IO_CNT> &state) {
	// Set outputs data based on diff in this->outputsWant
	std::vector<uint8_t> data {0, 0, 0, 0};

	for (size_t i = 0; i < LED_IO_CNT; i++)
		if (state[i])
			data[i/8] |= (1 << (i%8));

	return data;
}

std::array<bool, LED_IO_CNT> MtbLed::mtbDataToIo(const std::vector<uint8_t> &mtbBusData) {
	// 'mtbBusData' could be longer - no problem (used in MtbLedConfig
	std::array<bool, LED_IO_CNT> result{}; // initialize with 'false'
	if (mtbBusData.size() < 4)
		return result; // TODO: report error?

	for (size_t i = 0; i < LED_IO_CNT; i++)
		if (mtbBusData[i/8] & (1 << (i%8)))
			result[i] = true;

	return result;
}

void MtbLed::allOutputsReset() {
	for (size_t i = 0; i < LED_IO_CNT; i++) {
		this->outputsWant[i] = this->config.has_value() ? this->config.value().outputsSafe[i] : false;
		this->outputsConfirmed[i] = this->outputsWant[i];
		this->whoSetOutput[i] = nullptr;
	}
	this->sendOutputsChanged(ioStateToJson(this->outputsConfirmed), {});
}

/* MTB-LED activation ---------------------------------------------------------
 A) Module configuration was previously loaded from file:
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

void MtbLed::mtbBusActivate(Mtb::ModuleInfo info) {
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

void MtbLed::activate() {
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
					this->config.emplace(MtbLedConfig(data));
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

void MtbLed::configSet() {
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

void MtbLed::inputsRead(const std::vector<uint8_t> &data) {
	// Mtb module activation: got info & config set & inputs read → mark module as active
	this->inputs = this->mtbDataToIo(data);

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

void MtbLed::outputsReset() {
	for (size_t i = 0; i < LED_IO_CNT; i++) {
		this->outputsWant[i] = this->config.has_value() ? this->config.value().outputsSafe[i] : false;
		this->outputsConfirmed[i] = this->outputsWant[i];
	}

	for (size_t i = 0; i < LED_IO_CNT; i++)
		this->whoSetOutput[i] = nullptr;

	this->fullyActivated();
}

/* Inputs changed ----------------------------------------------------------- */

void MtbLed::mtbBusInputsChanged(const std::vector<uint8_t> &data) {
	if (this->active || this->activating) {
		this->inputs = this->mtbDataToIo(data);
		this->sendInputsChanged(ioStateToJson(this->inputs));
	}
}

void MtbLed::mtbUsbDisconnected() {
	MtbModule::mtbUsbDisconnected();
	this->allOutputsReset();
	this->inputs.fill(false);
}

/* MtbLedConfig ------------------------------------------------------------- */

std::vector<uint8_t> MtbLedConfig::serializeForMtbUsb() const {
	std::vector<uint8_t> result;
	const std::vector<uint8_t> safeStateSerialized = MtbLed::ioToMtb(this->outputsSafe);
	std::copy(safeStateSerialized.begin(), safeStateSerialized.end(), std::back_inserter(result));
	std::copy(this->brightness.begin(), this->brightness.end(), std::back_inserter(result));
	return result;
}

QJsonObject MtbLedConfig::json() const {
	QJsonObject result;
	{
		QJsonArray array;
		for (bool output : this->outputsSafe)
			array.push_back(output);
		result["outputsSafe"] = array;
	}
	{
		QJsonArray brightness;
		for (uint8_t value : this->brightness)
			brightness.push_back(value);
		result["brightness"] = brightness;
	}

	return result;
}

void MtbLedConfig::fromJson(const QJsonObject &json) {
	{
		const QJsonArray &jsonOutputsSafe = QJsonSafe::safeArray(json, "outputsSafe", LED_IO_CNT);
		for (size_t i = 0; i < LED_IO_CNT; i++)
			this->outputsSafe[i] = QJsonSafe::safeBool(jsonOutputsSafe[i]);
	}

	{
		const QJsonArray &jsonBrightness = QJsonSafe::safeArray(json, "brightness", LED_IO_CNT);
		for (size_t i = 0; i < LED_IO_CNT; i++)
			this->brightness[i] = QJsonSafe::safeUInt(jsonBrightness[i]);
	}
}

void MtbLedConfig::fromMtbUsb(const std::vector<uint8_t> &data) {
	if (data.size() < (LED_IO_CNT+4))
		return;

	this->outputsSafe = MtbLed::mtbDataToIo(data);

	for (size_t i = 0; i < LED_IO_CNT; i++)
		this->brightness[i] = data[i+4];
}

void MtbLed::reactivateCheck() {
	if ((!this->activating) && (this->activationsRemaining > 0) && (!this->active))
		this->activate();
}

/* Configuration ------------------------------------------------------------ */

void MtbLed::loadConfig(const QJsonObject &json) {
	MtbModule::loadConfig(json);
	this->config.emplace(MtbLedConfig(QJsonSafe::safeObject(json, "config")));
}

void MtbLed::saveConfig(QJsonObject &json) const {
	MtbModule::saveConfig(json);
	if (this->config.has_value())
		json["config"] = this->config.value().json();
}

/* Diagnostic Values -------------------------------------------------------- */

QJsonObject MtbLed::dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const {
	switch (dvi) {
		case Mtb::DVCommon::Errors: {
			if (data.size() < 1)
				return {};
			return {
				{"mcutemp_critical", static_cast<bool>(data[0] & 0x4)},
			};
		}

		case Mtb::DVCommon::Warnings: {
			if (data.size() < 2)
				return {};
			return {
				{"extrf", static_cast<bool>(data[0] & 0x1)},
				{"borf", static_cast<bool>(data[0] & 0x2)},
				{"wdrf", static_cast<bool>(data[0] & 0x4)},
				{"timer_miss", static_cast<bool>(data[0] & 0x10)},
				{"tlc_tef", static_cast<bool>(data[1] & 0x01)},
				{"tlc_lod", static_cast<bool>(data[1] & 0x02)},
				{"ts_offset_uncalibrated", static_cast<bool>(data[1] & 0x04)},
				{"mcutemp_high", static_cast<bool>(data[1] & 0x08)},
			};
		}

		case Mtb::DVCommon::MCUTemperature: {
			if (data.size() < 6)
				return {};

			uint16_t raw = (data[1] << 8) | data[0];
			int temp = (data[3] << 8) | data[2];
			int16_t ts_offset = (data[5] << 8) | data[4];
			return {
				{"mcu_temp_celsius", temp},
				{"mcu_temp_raw", raw},
				{"ts_offset", ts_offset},
			};
		}
	}

	return MtbModule::dvRepr(dvi, data);
}
