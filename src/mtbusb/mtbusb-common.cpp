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

MtbBusSpeed intToMtbBusSpeed(int speed) {
	if (speed == 57600)
		return MtbBusSpeed::br57600;
	if (speed == 115200)
		return MtbBusSpeed::br115200;
	return MtbBusSpeed::br38400;
}

bool mtbBusSpeedValid(int speed) {
	return (speed == 38400) || (speed == 57600) || (speed == 115200);
}

bool isValidModuleAddress(size_t addr) {
	return (addr > 0) && (addr < 256);
}

void checkValidModuleAddress(size_t addr) {
	if (!isValidModuleAddress(addr))
		throw EInvalidAddress(addr);
}

bool isBusEvent(const MtbBusRecvCommand &command) {
	return (command == MtbBusRecvCommand::InputChanged);
}

QString mtbBusRecvErrorToStr(MtbBusRecvError error) {
	switch (error) {
	case MtbBusRecvError::UnknownCommand:
		return "unknown command";
	case MtbBusRecvError::UnsupportedCommand:
		return "unsupported command";
	case MtbBusRecvError::BadAddress:
		return "bad address";
	default:
		return "unknown error";
	}
}

QString cmdErrorToStr(CmdError cmdError) {
	switch (cmdError) {
	case CmdError::SerialPortClosed:
		return "Serial port closed";
	case CmdError::UsbNoResponse:
		return "No response from MTB-USB module";
	case CmdError::BusNoResponse:
		return "No response from MTBbus module";
	case CmdError::UnknownCommand:
		return "Unknown command";
	case CmdError::UnsupportedCommand:
		return "Unsupported command";
	default:
		return "Unknown error";
	}
}

} // namespace Mtb
