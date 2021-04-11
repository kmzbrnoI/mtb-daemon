#include "mtbusb-common.h"

namespace Mtb {

int mtbBusSpeedToInt(MtbBusSpeed speed) {
	if (speed == MtbBusSpeed::br38400)
		return 38400;
	if (speed == MtbBusSpeed::br57600)
		return 57600;
	if (speed == MtbBusSpeed::br115200)
		return 115200;
	throw EInvalidSpeed("Invalid MTBbus speed!");
}

}; // namespace Mtb
