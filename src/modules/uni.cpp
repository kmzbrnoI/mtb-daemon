#include <QJsonArray>
#include <QJsonObject>
#include "uni.h"
#include "../mtbusb/mtbusb.h"
#include "../main.h"
#include "../errors.h"

MtbUni::MtbUni(uint8_t addr) : MtbModule(addr) { }

bool MtbUni::isIrSupport() const {
	return this->type == MtbModuleType::Univ2ir;
}

/* JSON Module Info --------------------------------------------------------- */

QJsonObject MtbUni::moduleInfo(bool state) const {
	QJsonObject response = MtbModule::moduleInfo(state);

	QJsonObject uni{
		{"ir", this->isIrSupport()},
		{"config", this->config.json(this->isIrSupport())},
	};

	if (state && this->active) {
		uni["state"] = QJsonObject{
			{"outputs", outputsToJson(this->outputsConfirmed)},
			{"inputs", inputsToJson(this->inputs)},
		};
	}

	response["MTB-UNI"] = uni;
	return response;
}

/* Json Set Outputs --------------------------------------------------------- */

void MtbUni::jsonSetOutput(QTcpSocket* socket, const QJsonObject& request) {
	if (!this->active) {
		sendError(socket, request, MTB_MODULE_FAILED, "Cannot set output of inactive module!");
		return;
	}

	QJsonObject outputs = request["outputs"].toObject();
	bool send = (this->outputsWant == this->outputsConfirmed);

	for (const auto& key : outputs.keys()) {
		size_t port = key.toInt();
		if (port > 15)
			continue;
		this->outputsWant[port] = jsonOutputToByte(outputs[key].toObject());
	}

	std::optional<size_t> id;
	if (request.contains("id"))
		id = request["id"].toInt();
	this->setOutputsWaiting.push_back({socket, id});

	if (send)
		this->mtbBusSetOutputs();
}

uint8_t MtbUni::jsonOutputToByte(const QJsonObject& json) {
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
		return value;
	}
	return 0;
}

void MtbUni::mtbBusSetOutputs() {
	this->setOutputsSent = this->setOutputsWaiting;

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
	for (const ServerRequest& sr : this->setOutputsSent) {
		QJsonObject response{
			{"command", "module_set_outputs"},
			{"type", "response"},
			{"status", "ok"},
			{"outputs", this->outputsToJson(this->outputsConfirmed)},
		};
		if (sr.id.has_value())
			response["id"] = static_cast<int>(sr.id.value());
		server.send(*sr.socket, response);
		ignore.push_back(sr.socket);
	}
	this->setOutputsSent.clear();

	// Report outputs changed event to other clients
	this->sendOutputsChanged(outputsToJson(this->outputsConfirmed), ignore);

	// Send next outputs
	if (!this->setOutputsWaiting.empty())
		this->mtbBusSetOutputs();
}

QJsonObject MtbUni::outputsToJson(const std::array<uint8_t, UNI_IO_CNT>& outputs) {
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

QJsonArray MtbUni::inputsToJson(uint16_t inputs) {
	QJsonArray json;
	for (size_t i = 0; i < UNI_IO_CNT; i++) {
		json.push_back(static_cast<bool>(inputs&1));
		inputs >>= 1;
	}
	return json;
}

void MtbUni::mtbBusOutputsNotSet(Mtb::CmdError) {
	// Report err callback to clients
	for (const ServerRequest& sr : this->setOutputsSent) {
		QJsonObject response{
			{"command", "module_set_outputs"},
			{"type", "response"},
			{"status", "error"},
			{"error", jsonError(MTB_MODULE_NOT_ANSWERED_CMD_GIVING_UP,
		                              "No response to SetOutput command!")},
		};
		if (sr.id.has_value())
			response["id"] = static_cast<int>(sr.id.value());
		server.send(*sr.socket, response);
	}
	this->setOutputsSent.clear();

	// TODO: mark module as failed? Do anything else?
	this->outputsConfirmed = this->outputsWant;
}

/* Json Set Config ---------------------------------------------------------- */

void MtbUni::jsonSetConfig(QTcpSocket*, const QJsonObject&) {
}

/* Json Upgrade Fw ---------------------------------------------------------- */

void MtbUni::jsonUpgradeFw(QTcpSocket* socket, const QJsonObject& request) {
	if (!this->active) {
		sendError(socket, request, MTB_MODULE_FAILED, "Cannot upgrade FW of inactive module!");
		return;
	}
}

/* -------------------------------------------------------------------------- */

std::vector<uint8_t> MtbUni::mtbBusOutputsData() const {
	// Set outputs data based on diff in this->outputsWant
	const std::array<uint8_t, UNI_IO_CNT>& outputs = this->outputsWant;
	std::vector<uint8_t> data{0, 0, 0, 0};

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
					data[3] = (1 << i);
				else
					data[2] = (1 << (i-8));
			}
		}
	}

	return data;
}

std::array<uint8_t, UNI_IO_CNT> MtbUni::moduleOutputsData(const std::vector<uint8_t> mtbBusData) {
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

/* MTB-UNI activation ---------------------------------------------------------
 A) Module configuration was previously laoded from file:
 * 1) General information are read
 * 2) Config is SET
 * 3) Inputs are get
 * 4) Outputs are reset
 B) Module configuration was NOT previously loaded from file:
 * 1) General information are read
 * 2) Config is GET
 * 3) Inputs are get
 * 4) Outputs are reset
 */

void MtbUni::mtbBusActivate(Mtb::ModuleInfo info) {
	// Mtb module activated, got info → set config, then get inputs
	MtbModule::mtbBusActivate(info);

	if (this->configLoaded) {
		log("Config previously loaded from file, setting to module...", Mtb::LogLevel::Info);
		mtbusb.send(
			Mtb::CmdMtbModuleSetConfig(
				this->address, this->config.serializeForMtbUsb(this->isIrSupport()),
				{[this](uint8_t, void*) { this->configSet(); }},
				{[](Mtb::CmdError, void*) {
					log("Unable to set module config, module keeps disabled.", Mtb::LogLevel::Error);
				}}
			)
		);
	} else {
		log("Config of this module not loaded from file, getting config from module...", Mtb::LogLevel::Info);
		mtbusb.send(
			Mtb::CmdMtbModuleGetConfig(
				this->address,
				{[this](uint8_t, const std::vector<uint8_t>& data, void*) {
					this->config.fromMtbUsb(data);
					this->configLoaded = true;
					this->configSet();
				}},
				{[](Mtb::CmdError, void*) {
					log("Unable to get module config, module keeps disabled.", Mtb::LogLevel::Error);
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
			{[](Mtb::CmdError, void*) {
				log("Unable to get new module inputs, module keeps disabled.", Mtb::LogLevel::Error);
			}}
		)
	);
}

void MtbUni::inputsRead(const std::vector<uint8_t>& data) {
	// Mtb module activation: got info & config set & inputs read → mark module as active
	this->storeInputsState(data);

	mtbusb.send(
		Mtb::CmdMtbModuleResetOutputs(
			this->address,
			{[this](uint8_t, void*) { this->outputsReset(); }},
			{[](Mtb::CmdError, void*) {
				log("Unable to reset new module outputs, module keeps disabled.",
				    Mtb::LogLevel::Error);
			}}
		)
	);
}

void MtbUni::storeInputsState(const std::vector<uint8_t>& data) {
	if (data.size() >= 2)
		this->inputs = (data[0] << 8) | data[1];
}

void MtbUni::outputsReset() {
	this->outputsWant = this->config.outputsSafe;
	this->outputsConfirmed = this->outputsWant;
	this->active = true;
	log("Module "+QString::number(this->address)+" activated", Mtb::LogLevel::Info);

	QJsonObject json{
		{"command", "module_activated"},
		{"type", "event"},
		{"modules", QJsonArray{this->address}}, // single module
	};

	for (auto pair : subscribes[this->address])
		server.send(*pair.first, json);
}

/* Inputs changed ----------------------------------------------------------- */

void MtbUni::mtbBusInputsChanged(const std::vector<uint8_t> data) {
	this->storeInputsState(data);
	this->sendInputsChanged(inputsToJson(this->inputs));
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

QJsonObject MtbUniConfig::json(bool withIrs) const {
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
	}

	return result;
}

void MtbUniConfig::fromJson(const QJsonObject& json) {
	const QJsonArray& jsonOutputsSafe = json["outputsSafe"].toArray();
	const QJsonArray& jsonInputsDelay = json["outputsSafe"].toArray();
	for (size_t i = 0; i < UNI_IO_CNT; i++) {
		if (i < static_cast<size_t>(jsonOutputsSafe.size()))
			this->outputsSafe[i] = MtbUni::jsonOutputToByte(jsonOutputsSafe[i].toObject());
		if (i < static_cast<size_t>(jsonInputsDelay.size()))
			this->inputsDelay[i] = jsonInputsDelay[i].toDouble()*10;
	}
}

void MtbUniConfig::fromMtbUsb(const std::vector<uint8_t>& data) {
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

/* Configuration ------------------------------------------------------------ */

void MtbUni::loadConfig(const QJsonObject& json) {
	this->config.fromJson(json["config"].toObject());
	this->configLoaded = true;
}

void MtbUni::saveConfig(QJsonObject& json) const {
	if (this->configLoaded)
		json["config"] = this->config.json(this->isIrSupport());
}
