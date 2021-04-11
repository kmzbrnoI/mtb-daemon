#ifndef MTBUSB_COMMON_H
#define MTBUSB_COMMON_H

#include <stdexcept>

namespace Mtb {

struct EInvalidSpeed : public std::logic_error {
	EInvalidSpeed() : logic_error("Invalid MTBbus speed!") {}
	EInvalidSpeed(const std::string &str) : logic_error(str) {}
};

enum class MtbBusSpeed {
	br38400 = 1,
	br57600 = 2,
	br115200 = 3,
};

int mtbBusSpeedToInt(MtbBusSpeed speed);

struct EInvalidAddress : public std::logic_error {
	EInvalidAddress() : logic_error("Invalid MTBbus module address!") {}
	EInvalidAddress(size_t addr) : logic_error("Invalid MTBbus module address: "+std::to_string(addr)+"!") {}
	EInvalidAddress(const std::string &str) : logic_error(str) {}
};

bool isValidModuleAddress(size_t addr);
void checkValidModuleAddress(size_t addr);

}; // namespace Mtb

#endif
