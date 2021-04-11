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

/* MTB-USB commands ----------------------------------------------------------*/

struct CmdMtbUsbInfoRequest : public Cmd {
	std::vector<uint8_t> getBytes() const override { return {0x20}; }
	QString msg() const override { return "MTB-USB Information Request"; }
};

struct CmdMtbUsbChangeSpeed : public Cmd {
	const MtbBusSpeed speed;

	CmdMtbUsbChangeSpeed(const MtbBusSpeed speed) : speed(speed) {}
	std::vector<uint8_t> getBytes() const override {
		return {0x21, static_cast<uint8_t>(speed)};
	}
	QString msg() const override {
		return "MTB-USB Change MTBbus Speed to "+QString(mtbBusSpeedToInt(speed))+" baud/s";
	}
	bool conflict(const Cmd &cmd) const override { return is<CmdMtbUsbChangeSpeed>(cmd); }
};

struct CmdMtbUsbActiveModulesRequest : public Cmd {
	std::vector<uint8_t> getBytes() const override { return {0x22}; }
	QString msg() const override { return "MTB-USB Active Modules Requst"; }
};

struct CmdMtbUsbForward : public Cmd {
	static constexpr uint8_t usbCommandCode = 0x10;
	const uint8_t module;

	CmdMtbUsbForward(uint8_t module) : module(module) {
		if (module == 0)
			throw EInvalidAddress(module);
	}
};

/* MTBbus commands -----------------------------------------------------------*/

struct CmdMtbModuleInfoRequest : public CmdMtbUsbForward {
	CmdMtbModuleInfoRequest(uint8_t module) : CmdMtbUsbForward(module) {}
	std::vector<uint8_t> getBytes() const override { return {usbCommandCode, module, 0x20}; }
	QString msg() const override { return "Module "+QString(module)+" Information Request"; }
};

}; // namespace Mtb

#endif
