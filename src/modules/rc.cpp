#include <QJsonArray>
#include <QJsonObject>
#include "utils.h"
#include "rc.h"
#include "mtbusb.h"
#include "main.h"
#include "errors.h"

MtbRc::MtbRc(uint8_t addr) : MtbModule(addr) {
}

/* JSON Module Info --------------------------------------------------------- */

QJsonObject MtbRc::moduleInfo(bool state, bool config) const {
	QJsonObject response = MtbModule::moduleInfo(state, config);
	QJsonObject rc;

	if (state && this->active && !this->busModuleInfo.inBootloader())
		rc["state"] = QJsonObject{{"inputs", this->inputsToJson()}};

	response[moduleTypeToStr(this->type)] = rc;
	return response;
}

QJsonObject MtbRc::inputsToJson() const {
	QJsonArray arrayOfInputs;
	for (const auto& input : this->inputs) {
		QJsonArray addrsPerInput;
		for (const DccAddr addr : input)
			addrsPerInput.push_back(addr);
		arrayOfInputs.push_back(addrsPerInput);
	}
	return {{"ports", arrayOfInputs}};
}

/* Json Set Config ---------------------------------------------------------- */

void MtbRc::jsonSetConfig(QTcpSocket *socket, const QJsonObject &request) {
	// Just set general MtbModule configuration (e.g. name of the module)

	if (this->isFirmwareUpgrading()) {
		sendError(socket, request, MTB_MODULE_UPGRADING_FW, "Firmware of module is being upgraded!");
		return;
	}
	if (this->busModuleInfo.inBootloader()) {
		sendError(socket, request, MTB_MODULE_IN_BOOTLOADER, "Module is in bootloader!");
		return;
	}

	MtbModule::jsonSetConfig(socket, request);
}

/* Json Upgrade Firmware ---------------------------------------------------- */

void MtbRc::jsonUpgradeFw(QTcpSocket *socket, const QJsonObject &request) {
	if (this->isFirmwareUpgrading()) {
		sendError(socket, request, MTB_MODULE_UPGRADING_FW, "Firmware is already being upgraded!");
		return;
	}

	sendError(socket, request, MTB_MODULE_UNSUPPORTED_COMMAND, "Firmware upgrading not yet implemented for MTB-RC!");
	return;

	// TODO
	/*this->fwUpgrade.fwUpgrading = ServerRequest(socket, request);
	this->fwUpgrade.data = parseFirmware(request["firmware"].toObject());
	this->alignFirmware(this->fwUpgrade.data, this->pageSize());

	if (!this->configWriting.has_value() && this->setOutputsSent.empty())
		this->fwUpgdInit();*/
}

void MtbRc::alignFirmware(std::map<size_t, std::vector<uint8_t>> &fw, size_t pageSize) {
	// TODO
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

/* MTB-RC activation ---------------------------------------------------------
 * 1) General information are read
 * 2) Inputs are get
 */

void MtbRc::mtbBusActivate(Mtb::ModuleInfo info) {
	// Mtb module activated, got info → get inputs
	MtbModule::mtbBusActivate(info);

	if (info.inBootloader()) {
		// In bootloader → mark as active, don't do anything else
		this->mlog("Module is in bootloader!", Mtb::LogLevel::Info);
		return;
	}

	this->activate();
}

void MtbRc::activate() {
	this->activating = true;

	if (this->busModuleInfo.warning || this->busModuleInfo.error)
		this->mlog("Module warning="+QString::number(this->busModuleInfo.warning)+", error="+
		           QString::number(this->busModuleInfo.error), Mtb::LogLevel::Warning);

	// Mtb module activation: got info → read inputs
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

void MtbRc::inputsRead(const std::vector<uint8_t> &data) {
	// Mtb module activation: got info & config set & inputs read → mark module as active
	this->storeInputsState(data);
	this->fullyActivated();
}

void MtbRc::storeInputsState(const std::vector<uint8_t> &data) {
	for (auto& input : this->inputs)
		input.clear();

	for (size_t i = 0; i+1 < data.size(); i += 2) {
		size_t input = (data[i] >> 5);
		DccAddr addr = data[i+1] | ((data[i] & 0x1F) << 8);
		this->inputs[input].insert(addr);
	}
}

/* Inputs changed ----------------------------------------------------------- */

void MtbRc::mtbBusInputsChanged(const std::vector<uint8_t> &data) {
	if (this->active || this->activating) {
		this->storeInputsState(data);
		this->sendInputsChanged(this->inputsToJson());
	}
}

void MtbRc::mtbUsbDisconnected() {
	MtbModule::mtbUsbDisconnected();
	for (auto& input : this->inputs)
		input.clear();
}

/* -------------------------------------------------------------------------- */

void MtbRc::reactivateCheck() {
	if ((!this->activating) && (this->activationsRemaining > 0) && (!this->active))
		this->activate();
}

/* Diagnostic Values -------------------------------------------------------- */

QJsonObject MtbRc::dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const {
	switch (dvi) {
	}

	return MtbModule::dvRepr(dvi, data);
}
