#ifndef MTBUSB_COMMANDS
#define MTBUSB_COMMANDS

/*
This file defines MTB-USB & MTBbus commands.
Each command type has its own class inherited from Cmd.
See mtbusb.h or README for more documentation.
*/

#include <functional>
#include "mtbusb-common.h"

namespace Mtb {

using StdCallbackFunc = std::function<void(void *data)>;
using StdModuleCallbackFunc = std::function<void(uint8_t addr, void *data)>;
using ErrCallbackFunc = std::function<void(CmdError, void *data)>;
using DataCallbackFunc = std::function<void(uint8_t addr, const std::vector<uint8_t>& outputs, void *data)>;

template <typename F>
struct CommandCallback {
	F const func;
	void *const data;

	CommandCallback(F const func, void *const data = nullptr)
	    : func(func), data(data) {}
};

struct Cmd {
	const CommandCallback<ErrCallbackFunc> onError;

	Cmd(const CommandCallback<ErrCallbackFunc>& onError = {[](CmdError, void*){}}) : onError(onError) {}
	virtual std::vector<uint8_t> getBytes() const = 0;
	virtual QString msg() const = 0;
	virtual ~Cmd() = default;
	virtual bool conflict(const Cmd &) const { return false; }
	virtual bool processUsbResponse(MtbUsbRecvCommand, const std::vector<uint8_t>&) const {
		return false;
	}
		// returns true iff response processed
	virtual void callError(CmdError error) const {
		if (nullptr != onError.func)
			onError.func(error, onError.data);
	}
};

template <typename Target>
bool is(const Cmd &x) {
	return (dynamic_cast<const Target *>(&x) != nullptr);
}

/* MTB-USB commands ----------------------------------------------------------*/

struct CmdMtbUsbInfoRequest : public Cmd {
	const CommandCallback<StdCallbackFunc> onOk; // no special callback here, data could be read from MtbUsb class directly

	CmdMtbUsbInfoRequest(const CommandCallback<StdCallbackFunc> &onOk = {[](void*){}},
	                     const CommandCallback<ErrCallbackFunc> &onError = {[](CmdError, void*){}})
	 : Cmd(onError), onOk(onOk) {}

	std::vector<uint8_t> getBytes() const override { return {0x20}; }
	QString msg() const override { return "MTB-USB Information Request"; }

	bool processUsbResponse(MtbUsbRecvCommand usbCommand, const std::vector<uint8_t>&) const override {
		if (usbCommand == MtbUsbRecvCommand::MtbUsbInfo) {
			onOk.func(onOk.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbUsbChangeSpeed : public Cmd {
	const MtbBusSpeed speed;
	const CommandCallback<StdCallbackFunc> onOk;

	CmdMtbUsbChangeSpeed(const MtbBusSpeed speed, const CommandCallback<StdCallbackFunc> &onOk = {[](void*){}},
	                     const CommandCallback<ErrCallbackFunc> &onError = {[](CmdError, void*){}})
	  : Cmd(onError), speed(speed), onOk(onOk) {}

	std::vector<uint8_t> getBytes() const override {
		return {0x21, static_cast<uint8_t>(speed)};
	}
	QString msg() const override {
		return "MTB-USB Change MTBbus Speed to "+QString::number(mtbBusSpeedToInt(speed))+" baud/s";
	}
	bool conflict(const Cmd &cmd) const override { return is<CmdMtbUsbChangeSpeed>(cmd); }

	bool processUsbResponse(MtbUsbRecvCommand usbCommand, const std::vector<uint8_t>&) const override {
		if (usbCommand == MtbUsbRecvCommand::Ack) {
			onOk.func(onOk.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbUsbActiveModulesRequest : public Cmd {
	const CommandCallback<StdCallbackFunc> onOk;

	CmdMtbUsbActiveModulesRequest(const CommandCallback<StdCallbackFunc> &onOk = {[](void*){}},
	                              const CommandCallback<ErrCallbackFunc> &onError = {[](CmdError, void*){}})
	  : Cmd(onError), onOk(onOk) {}

	std::vector<uint8_t> getBytes() const override { return {0x22}; }
	QString msg() const override { return "MTB-USB Active Modules Requst"; }

	bool processUsbResponse(MtbUsbRecvCommand usbCommand, const std::vector<uint8_t>&) const override {
		if (usbCommand == MtbUsbRecvCommand::ActiveModules) {
			onOk.func(onOk.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbUsbForward : public Cmd {
	static constexpr uint8_t usbCommandCode = 0x10;
	const uint8_t module;
	const uint8_t busCommandCode;

	CmdMtbUsbForward(uint8_t module, uint8_t busCommandCode,
	                 const CommandCallback<ErrCallbackFunc> &onError = {[](CmdError, void*){}})
	 : Cmd(onError), module(module), busCommandCode(busCommandCode) {
		if (module == 0)
			throw EInvalidAddress(module);
	}
	CmdMtbUsbForward(uint8_t busCommandCode,
	                 const CommandCallback<ErrCallbackFunc> &onError = {[](CmdError, void*){}})
	 : Cmd(onError), module(0), busCommandCode(busCommandCode) {} // broadcat

	virtual bool processBusResponse(MtbBusRecvCommand, const std::vector<uint8_t>&) const {
		return false;
	}

	bool broadcast() const { return this->module == 0; }
};

struct CmdMtbUsbPing : public Cmd {
	const CommandCallback<StdCallbackFunc> onOk;

	CmdMtbUsbPing(const CommandCallback<StdCallbackFunc> &onOk = {[](void*){}},
	              const CommandCallback<ErrCallbackFunc> &onError = {[](CmdError, void*){}})
	  : Cmd(onError), onOk(onOk) {}

	std::vector<uint8_t> getBytes() const override { return {0x30}; }
	QString msg() const override { return "MTB-USB Ping"; }

	bool processUsbResponse(MtbUsbRecvCommand usbCommand, const std::vector<uint8_t>&) const override {
		if (usbCommand == MtbUsbRecvCommand::Ack) {
			onOk.func(onOk.data);
			return true;
		}
		return false;
	}
};

/* MTBbus commands -----------------------------------------------------------*/

struct ModuleInfo {
	uint8_t type;
	bool bootloader_int;
	bool bootloader_unint;
	uint8_t fw_major;
	uint8_t fw_minor;
	uint8_t proto_major;
	uint8_t proto_minor;

	QString fw_version() const { return QString::number(fw_major)+"."+QString::number(fw_minor); }
	QString proto_version() const { return QString::number(proto_major)+"."+QString::number(proto_minor); }
	bool inBootloader() const { return this->bootloader_int || this->bootloader_unint; }
};

using ModuleInfoCallbackFunc = std::function<void(uint8_t module, ModuleInfo info, void *data)>;

struct CmdMtbModuleInfoRequest : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0x02;
	const CommandCallback<ModuleInfoCallbackFunc> onInfo;

	CmdMtbModuleInfoRequest(uint8_t module,
	                        const CommandCallback<ModuleInfoCallbackFunc> onInfo = {[](uint8_t, ModuleInfo, void*) {}},
	                        const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), onInfo(onInfo) {}
	std::vector<uint8_t> getBytes() const override { return {usbCommandCode, module, _busCommandCode}; }
	QString msg() const override { return "Module "+QString::number(module)+" Information Request"; }

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>& data) const override {
		if ((busCommand == MtbBusRecvCommand::ModuleInfo) && (data.size() >= 6)) {
			ModuleInfo info;
			info.type = data[0];
			info.bootloader_int = data[1] & 1;
			info.bootloader_unint = (data[1] >> 1) & 1;
			info.fw_major = data[2];
			info.fw_minor = data[3];
			info.proto_major = data[4];
			info.proto_minor = data[5];
			onInfo.func(module, info, onInfo.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbModuleSetConfig : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0x03;
	std::vector<uint8_t> data;
	const CommandCallback<StdModuleCallbackFunc> onOk;

	CmdMtbModuleSetConfig(uint8_t module, const std::vector<uint8_t> &data,
	                      const CommandCallback<StdModuleCallbackFunc> onOk = {[](uint8_t, void*) {}},
	                      const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), onOk(onOk) {
		this->data = {usbCommandCode, module, _busCommandCode};
		std::copy(data.begin(), data.end(), std::back_inserter(this->data));
	}
	std::vector<uint8_t> getBytes() const override { return data; }
	QString msg() const override { return "Module "+QString::number(module)+" set configuration"; }

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>&) const override {
		if (busCommand == MtbBusRecvCommand::Acknowledgement) {
			onOk.func(module, onOk.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbModuleGetConfig : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0x04;
	const CommandCallback<DataCallbackFunc> onGet;

	CmdMtbModuleGetConfig(uint8_t module,
	                      const CommandCallback<DataCallbackFunc> onGet = {[](uint8_t, const std::vector<uint8_t>&, void*) {}},
	                      const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), onGet(onGet) {}
	std::vector<uint8_t> getBytes() const override { return {usbCommandCode, module, _busCommandCode}; }
	QString msg() const override { return "Module "+QString::number(module)+" get configuration"; }

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>& data) const override {
		if (busCommand == MtbBusRecvCommand::ModuleConfig) {
			onGet.func(module, data, onGet.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbModuleBeacon : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0x05;
	const bool state;
	const CommandCallback<StdModuleCallbackFunc> onOk;

	CmdMtbModuleBeacon(uint8_t module, bool state,
	                   const CommandCallback<StdModuleCallbackFunc> onOk = {[](uint8_t, void*) {}},
	                   const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), state(state), onOk(onOk) {}
	std::vector<uint8_t> getBytes() const override {
		return {usbCommandCode, module, _busCommandCode, state};
	}
	QString msg() const override {
		return "Module "+QString::number(module)+" beacon " + (state ? "on" : "off");
	}

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>&) const override {
		if (busCommand == MtbBusRecvCommand::Acknowledgement) {
			onOk.func(module, onOk.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbModuleGetInputs : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0x10;
	const CommandCallback<DataCallbackFunc> onGet;

	CmdMtbModuleGetInputs(uint8_t module,
	                      const CommandCallback<DataCallbackFunc> onGet = {[](uint8_t, const std::vector<uint8_t>&, void*) {}},
	                      const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), onGet(onGet) {}
	std::vector<uint8_t> getBytes() const override { return {usbCommandCode, module, _busCommandCode}; }
	QString msg() const override { return "Module "+QString::number(module)+" get inputs"; }

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>& data) const override {
		if (busCommand == MtbBusRecvCommand::InputState) {
			onGet.func(module, data, onGet.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbModuleSetOutput : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0x11;
	std::vector<uint8_t> data;
	const CommandCallback<DataCallbackFunc> onSet;

	CmdMtbModuleSetOutput(uint8_t module, const std::vector<uint8_t> &data,
	                      const CommandCallback<DataCallbackFunc> onSet = {[](uint8_t, const std::vector<uint8_t>&, void*) {}},
	                      const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), onSet(onSet) {
		this->data = {usbCommandCode, module, _busCommandCode};
		std::copy(data.begin(), data.end(), std::back_inserter(this->data));
	}
	std::vector<uint8_t> getBytes() const override { return data; }
	QString msg() const override { return "Module "+QString::number(module)+" set output"; }

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t> &data) const override {
		if (busCommand == MtbBusRecvCommand::OutputSet) {
			onSet.func(module, data, onSet.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbModuleResetOutputs : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0x12;
	const CommandCallback<StdModuleCallbackFunc> onOkModule = {[](uint8_t, void*) {}};
	const CommandCallback<StdCallbackFunc> onOkBroadcast = {[](void*){}};

	CmdMtbModuleResetOutputs(uint8_t module,
	                         const CommandCallback<StdModuleCallbackFunc> onOk = {[](uint8_t, void*){}},
	                         const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*){}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), onOkModule(onOk) {}
	CmdMtbModuleResetOutputs(const CommandCallback<StdCallbackFunc> onOk = {[](void*){}},
	                         const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*){}})
	 : CmdMtbUsbForward(_busCommandCode, onError), onOkBroadcast(onOk) {}
	std::vector<uint8_t> getBytes() const override { return {usbCommandCode, module, _busCommandCode}; }
	QString msg() const override {
		if (this->broadcast())
			return "Reset outputs of all modules";
		return "Module "+QString::number(module)+" reset outputs";
	}

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>&) const override {
		if (this->broadcast()) {
			if (busCommand == MtbBusRecvCommand::Acknowledgement) {
				onOkBroadcast.func(onOkBroadcast.data);
				return true;
			}
		} else {
			if (busCommand == MtbBusRecvCommand::Acknowledgement) {
				onOkModule.func(module, onOkModule.data);
				return true;
			}
		}
		return false;
	}
};

struct CmdMtbModuleChangeAddr : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0x20;
	const uint8_t newAddr;
	const CommandCallback<StdCallbackFunc> onOk;

	CmdMtbModuleChangeAddr(uint8_t module, uint8_t newAddr,
	                       const CommandCallback<StdCallbackFunc> onOk = {[](void*) {}},
	                       const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), newAddr(newAddr), onOk(onOk) {}
	std::vector<uint8_t> getBytes() const override { return {usbCommandCode, module, _busCommandCode, newAddr}; }
	QString msg() const override {
		return "Module "+QString::number(module)+" change address to "+QString::number(newAddr);
	}

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>&) const override {
		if (busCommand == MtbBusRecvCommand::Acknowledgement) {
			onOk.func(onOk.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbModuleChangeSpeed : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0xE0;
	const MtbBusSpeed speed;
	const CommandCallback<StdModuleCallbackFunc> onOkModule = {[](uint8_t, void*) {}};
	const CommandCallback<StdCallbackFunc> onOkBroadcast = {[](void*){}};

	CmdMtbModuleChangeSpeed(uint8_t module, MtbBusSpeed speed,
	                        const CommandCallback<StdModuleCallbackFunc> onOk = {[](uint8_t, void*) {}},
	                        const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), speed(speed), onOkModule(onOk) {}
	CmdMtbModuleChangeSpeed(MtbBusSpeed speed,
	                        const CommandCallback<StdCallbackFunc> onOk = {[](void*) {}},
	                        const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(_busCommandCode, onError), speed(speed), onOkBroadcast(onOk) {}
	std::vector<uint8_t> getBytes() const override {
		return {usbCommandCode, module, _busCommandCode, static_cast<uint8_t>(speed)};
	}
	QString msg() const override {
		if (this->broadcast())
			return "All modules change speed to "+QString::number(mtbBusSpeedToInt(speed));
		return "Module "+QString::number(module)+" change speed to "+QString::number(mtbBusSpeedToInt(speed));
	}

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>&) const override {
		if (this->broadcast()) {
			if (busCommand == MtbBusRecvCommand::Acknowledgement) {
				onOkBroadcast.func(onOkBroadcast.data);
				return true;
			}
		} else {
			if (busCommand == MtbBusRecvCommand::Acknowledgement) {
				onOkModule.func(module, onOkModule.data);
				return true;
			}
		}
		return false;
	}
};

struct CmdMtbModuleFwUpgradeReq : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0xF0;
	const CommandCallback<StdModuleCallbackFunc> onOk;

	CmdMtbModuleFwUpgradeReq(uint8_t module,
	                         const CommandCallback<StdModuleCallbackFunc> onOk = {[](uint8_t, void*) {}},
	                         const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError),  onOk(onOk) {}
	std::vector<uint8_t> getBytes() const override { return {usbCommandCode, module, _busCommandCode}; }
	QString msg() const override {
		return "Module "+QString::number(module)+" firmware upgrade request";
	}

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>&) const override {
		if (busCommand == MtbBusRecvCommand::Acknowledgement) {
			onOk.func(module, onOk.data);
			return true;
		}
		return false;
	}
};

struct CmdMtbModuleFwWriteFlash : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0xF1;
	std::vector<uint8_t> data;
	const CommandCallback<StdModuleCallbackFunc> onOk;
	const uint16_t flashAddr;

	CmdMtbModuleFwWriteFlash(
		uint8_t module,
		uint16_t flashAddr,
		const std::vector<uint8_t> &data,
		const CommandCallback<StdModuleCallbackFunc> onOk = {[](uint8_t, void*) {}},
		const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}}
	) : CmdMtbUsbForward(module, _busCommandCode, onError), onOk(onOk), flashAddr(flashAddr) {
		this->data = {usbCommandCode, module, _busCommandCode, static_cast<uint8_t>(flashAddr>>8),
		              static_cast<uint8_t>(flashAddr&0xFF)};
		std::copy(data.begin(), data.end(), std::back_inserter(this->data));
	}
	std::vector<uint8_t> getBytes() const override { return this->data; }
	QString msg() const override {
		return "Module "+QString::number(module)+" firmware write flash 0x"+
		       QString::number(this->flashAddr, 16).rightJustified(4, '0');;
	}

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>&) const override {
		if (busCommand == MtbBusRecvCommand::Acknowledgement) {
			onOk.func(module, onOk.data);
			return true;
		}
		return false;
	}
};

enum class FwWriteFlashStatus {
	FlashWriten = 0x00,
	WritingFlash = 0x01,
};

using FwWriteFlashStatusCallbackFunc = std::function<void(uint8_t addr, FwWriteFlashStatus, void *data)>;

struct CmdMtbModuleFwWriteFlashStatusRequest : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0xF2;
	const CommandCallback<FwWriteFlashStatusCallbackFunc> onResponse;

	CmdMtbModuleFwWriteFlashStatusRequest(
		uint8_t module,
		const CommandCallback<FwWriteFlashStatusCallbackFunc> onResponse = {[](uint8_t, FwWriteFlashStatus, void*) {}},
		const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}}
	) : CmdMtbUsbForward(module, _busCommandCode, onError),  onResponse(onResponse) {}
	std::vector<uint8_t> getBytes() const override { return {usbCommandCode, module, _busCommandCode}; }
	QString msg() const override {
		return "Module "+QString::number(module)+" firmware write flash status request";
	}

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>& data) const override {
		if ((busCommand == MtbBusRecvCommand::FWWriteFlashStatus) && (!data.empty())) {
			onResponse.func(module, static_cast<FwWriteFlashStatus>(data[0]), onResponse.data);
			return true;
		}
		return false;
	}
};

// Should return true iff response if valid.
using SpecificCallbackFunc =
    std::function<bool(uint8_t addr, MtbBusRecvCommand busCommand, const std::vector<uint8_t> &outputs, void *data)>;

struct CmdMtbModuleSpecific : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0xFE;
	std::vector<uint8_t> data;
	const CommandCallback<SpecificCallbackFunc> onResponse = {[](uint8_t, MtbBusRecvCommand, const std::vector<uint8_t>&, void*) { return true; }};
	const CommandCallback<StdCallbackFunc> onOkBroadcast = {[](void*) {}};

	CmdMtbModuleSpecific(
		uint8_t module,
		const std::vector<uint8_t> &data,
		const CommandCallback<SpecificCallbackFunc> onResponse = {[](uint8_t, MtbBusRecvCommand, const std::vector<uint8_t>&, void*) { return true; }},
		const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}}
	) : CmdMtbUsbForward(module, _busCommandCode, onError), onResponse(onResponse) {
		this->data = {usbCommandCode, module, _busCommandCode};
		std::copy(data.begin(), data.end(), std::back_inserter(this->data));
	}

	CmdMtbModuleSpecific(
		const std::vector<uint8_t> &data,
		const CommandCallback<StdCallbackFunc> onOk = {[](void*) {}},
		const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*) {}}
	) : CmdMtbUsbForward(_busCommandCode, onError), onOkBroadcast(onOk) {
		this->data = {usbCommandCode, 0, _busCommandCode};
		std::copy(data.begin(), data.end(), std::back_inserter(this->data));
	}

	std::vector<uint8_t> getBytes() const override { return this->data; }

	QString msg() const override {
		return "Module "+QString::number(module)+" specific command";
	}

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>& data) const override {
		if (this->broadcast()) {
			if (busCommand == MtbBusRecvCommand::Acknowledgement) {
				onOkBroadcast.func(onOkBroadcast.data);
				return true;
			}
			return false;
		}
		return onResponse.func(module, busCommand, data, onResponse.data);
	}
};

struct CmdMtbModuleReboot : public CmdMtbUsbForward {
	static constexpr uint8_t _busCommandCode = 0xFF;
	const CommandCallback<StdModuleCallbackFunc> onOkModule = {[](uint8_t, void*) {}};
	const CommandCallback<StdCallbackFunc> onOkBroadcast = {[](void*){}};

	CmdMtbModuleReboot(uint8_t module,
	                   const CommandCallback<StdModuleCallbackFunc> onOk = {[](uint8_t, void*){}},
	                   const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*){}})
	 : CmdMtbUsbForward(module, _busCommandCode, onError), onOkModule(onOk) {}
	CmdMtbModuleReboot(const CommandCallback<StdCallbackFunc> onOk = {[](void*){}},
	                   const CommandCallback<ErrCallbackFunc> onError = {[](CmdError, void*){}})
	 : CmdMtbUsbForward(_busCommandCode, onError), onOkBroadcast(onOk) {}

	std::vector<uint8_t> getBytes() const override { return {usbCommandCode, module, _busCommandCode}; }
	QString msg() const override {
		if (this->broadcast())
			return "Reboot all modules";
		return "Module "+QString::number(module)+" reboot request";
	}

	bool processBusResponse(MtbBusRecvCommand busCommand, const std::vector<uint8_t>&) const override {
		if (this->broadcast()) {
			if (busCommand == MtbBusRecvCommand::Acknowledgement) {
				onOkBroadcast.func(onOkBroadcast.data);
				return true;
			}
		} else {
			if (busCommand == MtbBusRecvCommand::Acknowledgement) {
				onOkModule.func(module, onOkModule.data);
				return true;
			}
		}
		return false;
	}
};

} // namespace Mtb

#endif
