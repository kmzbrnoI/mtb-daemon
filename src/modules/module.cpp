#include "module.h"
#include "../main.h"

QJsonObject MtbModule::moduleInfo() const {
	QJsonObject obj;
	obj["address"] = this->address;
	obj["type_code"] = static_cast<int>(this->type);
	obj["active"] = this->active;

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

void MtbModule::mtbBusActivate() {
	mtbusb.send(
		Mtb::CmdMtbModuleInfoRequest(
			this->address,
			{[this](uint8_t addr, Mtb::ModuleInfo moduleInfo, void* data){ this->mtbBusGotInfo(addr, moduleInfo, data); }}
			// no error handler: module stays disabled
		)
	);
}

void MtbModule::mtbBusLost() {
	this->active = false;
	// TODO: call event to client
}

void MtbModule::mtbBusInputsChanged(const std::vector<uint8_t>) {
}

void MtbModule::mtbBusGotInfo(uint8_t addr, Mtb::ModuleInfo moduleInfo, void*) {
	assert(addr == this->address);
	this->busModuleInfo = moduleInfo;
}
