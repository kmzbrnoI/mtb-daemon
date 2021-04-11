#ifndef MTBUSB_COMMANDS
#define MTBUSB_COMMANDS

/*
This file defines MTB-USB & MTBbus commands.
Each command type has its own class inherited from Cmd.
See mtbusb.h or README for more documentation.
*/

#include "mtbusb-common.h"

namespace Mtb {

struct Cmd {
	virtual std::vector<uint8_t> getBytes() const = 0;
	virtual QString msg() const = 0;
	virtual ~Cmd() = default;
	virtual bool conflict(const Cmd &) const { return false; }
	virtual bool okResponse() const { return false; }
};

template <typename Target>
bool is(const Cmd &x) {
	return (dynamic_cast<const Target *>(&x) != nullptr);
}

///////////////////////////////////////////////////////////////////////////////

struct CmdMtbUsbInfoRequest : public Cmd {
	std::vector<uint8_t> getBytes() const override { return {0x20}; }
	QString msg() const override { return "MTB-USB Information Request"; }
};

struct CmdMtbUsbChangeSpeed : public Cmd {
	const MtbBusSpeed speed;

	CmdMtbUsbChangeSpeed(const MtbBusSpeed speed) : speed(speed) {}
	std::vector<uint8_t> getBytes() const override { return {0x21, 0x81}; }
	QString msg() const override {
		return "MTB-USB Change MTBbus Speed to "+QString(mtbBusSpeedToInt(speed))+" baud/s";
	}
	bool conflict(const Cmd &cmd) const override { return is<CmdMtbUsbChangeSpeed>(cmd); }
};

}; // namespace Mtb

#endif
