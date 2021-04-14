#include "mtbusb-common.h"

namespace Mtb {

int mtbBusSpeedToInt(MtbBusSpeed speed) {
	if (speed == MtbBusSpeed::br38400)
		return 38400;
	if (speed == MtbBusSpeed::br57600)
		return 57600;
	if (speed == MtbBusSpeed::br115200)
		return 115200;
	throw EInvalidSpeed();
}

bool isValidModuleAddress(size_t addr) {
	return (addr > 0) && (addr < 256);
}

void checkValidModuleAddress(size_t addr) {
	if (!isValidModuleAddress(addr))
		throw EInvalidAddress(addr);
}

bool isBusEvent(const MtbBusRecvCommand& command) {
	return (command == MtbBusRecvCommand::InputChanged);
}

}; // namespace Mtb
