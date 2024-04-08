#include "mtbusb-common.h"

namespace Mtb {

int mtbBusSpeedToInt(MtbBusSpeed speed) {
	if (speed == MtbBusSpeed::br38400)
		return 38400;
	if (speed == MtbBusSpeed::br57600)
		return 57600;
	if (speed == MtbBusSpeed::br115200)
		return 115200;
	if (speed == MtbBusSpeed::br230400)
		return 230400;
	throw EInvalidSpeed();
}

MtbBusSpeed intToMtbBusSpeed(int speed) {
	if (speed == 57600)
		return MtbBusSpeed::br57600;
	if (speed == 115200)
		return MtbBusSpeed::br115200;
	if (speed == 230400)
		return MtbBusSpeed::br230400;
	return MtbBusSpeed::br38400;
}

bool mtbBusSpeedValid(int speed, uint16_t mtbusbFWver) {
	return ((speed == 38400) || (speed == 57600) || (speed == 115200) ||
	        ((speed == 230400) && (mtbusbFWver >= 0x0103)));
}

bool isValidModuleAddress(size_t addr) {
	return (addr > 0) && (addr < 256);
}

void checkValidModuleAddress(size_t addr) {
	if (!isValidModuleAddress(addr))
		throw EInvalidAddress(addr);
}

bool isBusEvent(const MtbBusRecvCommand &command) {
	return (command == MtbBusRecvCommand::InputChanged) || (command == MtbBusRecvCommand::DiagValue);
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

QString DVToStr(uint8_t dv) {
	switch (dv) {
	case DV::Version: return "version";
	case DV::State: return "state";
	case DV::Uptime: return "uptime";
	case DV::Errors: return "errors";
	case DV::Warnings: return "warnings";
	case DV::MCUVoltage: return "mcu_voltage";
	case DV::MCUTemperature: return "mcu_temperature";
	default: return "unknown";
	}
}

std::optional<DV> StrToDV(const QString& str) {
	if (str == "version")
		return {DV::Version};
	if (str == "state")
		return {DV::State};
	if (str == "uptime")
		return {DV::Uptime};
	if (str == "errors")
		return {DV::Errors};
	if (str == "warnings")
		return {DV::Warnings};
	if (str == "mcu_voltage")
		return {DV::MCUVoltage};
	if (str == "mcu_temperature")
		return {DV::MCUTemperature};
	return {};
}

} // namespace Mtb
