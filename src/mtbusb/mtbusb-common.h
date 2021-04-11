#ifndef MTBUSB_COMMON_H
#define MTBUSB_COMMON_H

#include <stdexcept>
#include <QString>

namespace Mtb {

struct MtbUsbError: public std::logic_error {
	MtbUsbError(const std::string& str) : std::logic_error(str) {}
	MtbUsbError(const QString& str) : logic_error(str.toStdString()) {}
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

struct EInvalidAddress : public MtbUsbError {
	EInvalidAddress() : MtbUsbError(std::string("Invalid MTBbus module address!")) {}
	EInvalidAddress(size_t addr) : MtbUsbError("Invalid MTBbus module address: "+std::to_string(addr)+"!") {}
	EInvalidAddress(const std::string &str) : MtbUsbError(str) {}
};

bool isValidModuleAddress(size_t addr);
void checkValidModuleAddress(size_t addr);

}; // namespace Mtb

#endif
