#include <QJsonArray>
#include <QJsonObject>
#include "uni.h"
#include "../mtbusb/mtbusb.h"
#include "../main.h"

bool MtbUni::isIrSupport() const {
	return this->type == MtbModuleType::Univ2ir;
}

QJsonObject MtbUni::moduleInfo(bool state) const {
	QJsonObject response = MtbModule::moduleInfo(state);
	return response;
}

void MtbUni::jsonSetOutput(QTcpSocket&, const QJsonObject&) {
}

void MtbUni::jsonSetConfig(QTcpSocket&, const QJsonObject&) {
}

void MtbUni::jsonUpgradeFw(QTcpSocket&, const QJsonObject&) {
}

void MtbUni::mtbBusActivate(Mtb::ModuleInfo info) {
	// Mtb module activated, got info → set config, then get inputs
	MtbModule::mtbBusActivate(info);

	mtbusb.send(
		Mtb::CmdMtbModuleSetConfig(
			this->address, this->config.serializeForMtbUsb(this->isIrSupport()),
			{[this](uint8_t, void*) { this->configSet(); }},
			{[](Mtb::CmdError, void*) {
				log("Unable to set module config, module keeps disabled.", Mtb::LogLevel::Error);
			}}
		)
	);
}

void MtbUni::configSet() {
	// Mtb module activation: got info & config set → read inputs
	this->active = true;

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

	QJsonObject json;
	QJsonArray array{this->address};
	json["command"] = "module_activated";
	json["type"] = "event";
	json["modules"] = array;

	for (auto pair : subscribes[this->address]) {
		QTcpSocket* socket = pair.first;
		server.send(*socket, json);
	}
}

void MtbUni::storeInputsState(const std::vector<uint8_t>& data) {
	if (data.size() >= 2)
		this->inputs = (data[0] << 8) | data[1];
}

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
		for (size_t i = 0; i < 16; i++) {
			array.push_back(static_cast<bool>(irs & 1));
			irs >>= 1;
		}
		result["irs"] = array;
	}

	return result;
}
