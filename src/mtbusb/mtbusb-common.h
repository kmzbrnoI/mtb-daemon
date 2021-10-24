#ifndef MTBUSB_COMMON_H
#define MTBUSB_COMMON_H

#include <stdexcept>
#include <QString>

namespace Mtb {

struct MtbUsbError : public std::logic_error {
	MtbUsbError(const std::string &str) : std::logic_error(str) {}
	MtbUsbError(const QString &str) : logic_error(str.toStdString()) {}
};

struct EInvalidSpeed : public MtbUsbError {
	EInvalidSpeed() : MtbUsbError(std::string("Invalid MTBbus speed!")) {}
	EInvalidSpeed(const std::string &str) : MtbUsbError(str) {}
};

enum class MtbBusSpeed {
	br38400 = 1,
	br57600 = 2,
	br115200 = 3,
};

int mtbBusSpeedToInt(MtbBusSpeed speed);
MtbBusSpeed intToMtbBusSpeed(int speed);
bool mtbBusSpeedValid(int speed);

struct EInvalidAddress : public MtbUsbError {
	EInvalidAddress() : MtbUsbError(std::string("Invalid MTBbus module address!")) {}
	EInvalidAddress(size_t addr) : MtbUsbError("Invalid MTBbus module address: "+std::to_string(addr)+"!") {}
	EInvalidAddress(const std::string &str) : MtbUsbError(str) {}
};

bool isValidModuleAddress(size_t addr);
void checkValidModuleAddress(size_t addr);

enum class MtbUsbRecvCommand {
	Ack = 0x01,
	Error = 0x02,
	MtbBusForward = 0x10,
	MtbUsbInfo = 0x20,
	ActiveModules = 0x22,
	NewModule = 0x23,
	ModuleFailed = 0x24,
};

enum class MtbBusRecvCommand {
	Acknowledgement = 0x01,
	Error = 0x02,
	ModuleInfo = 0x03,
	ModuleConfig = 0x04,
	InputChanged = 0x10,
	InputState = 0x11,
	OutputSet = 0x12,
	DiagInfo = 0xD0,
	FWWriteFlashStatus = 0xF2,
	ModuleSpecific = 0xFE,
};

bool isBusEvent(const MtbBusRecvCommand&);

enum class MtbUsbRecvError {
	NoResponse = 0x01,
	FullBuffer = 0x02,
};

enum class MtbBusRecvError {
	UnknownCommand = 0x01,
	UnsupportedCommand = 0x02,
	BadAddress = 0x03,
};

QString mtbBusRecvErrorToStr(MtbBusRecvError);

enum class CmdError {
	UnknownCommand = 0x01,
	UnsupportedCommand = 0x02,
	BadAddress = 0x03,
	SerialPortClosed = 0x10,
	UsbNoResponse = 0x11,
	BusNoResponse = 0x12,
	HistoryConflict = 0x13,
};

QString cmdErrorToStr(CmdError);

} // namespace Mtb

#endif
