#ifndef MTBUSB_COMMON_H
#define MTBUSB_COMMON_H

#include <stdexcept>

namespace Mtb {

struct EInvalidSpeed : public std::logic_error {
	EInvalidSpeed(const std::string& str) : logic_error(str) {}
};

enum class MtbBusSpeed {
	br38400 = 1,
	br57600 = 2,
	br115200 = 3,
};

int mtbBusSpeedToInt(MtbBusSpeed speed);

}; // namespace Mtb

#endif
