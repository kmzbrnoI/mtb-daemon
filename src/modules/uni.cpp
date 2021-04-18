#include "uni.h"
#include "../mtbusb/mtbusb.h"
#include "../main.h"

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

void MtbUni::mtbBusGotInfo(uint8_t addr, Mtb::ModuleInfo, void*) {
	assert(addr == this->address);

	mtbusb.send(
		Mtb::CmdMtbModuleInfoRequest(
			this->address,
			{[this](uint8_t addr, Mtb::ModuleInfo moduleInfo, void* data){ this->mtbBusGotInfo(addr, moduleInfo, data); }}
			// no error handler: module stays disabled
		)
	);
}
