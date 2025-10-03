#ifndef _MODULE_H_
#define _MODULE_H_

#include <QTcpSocket>
#include <QJsonObject>
#include "mtbusb.h"
#include "server.h"
#include "errors.h"

enum class MtbModuleType {
	Unknown = 0x00,
	Univ2ir = 0x10,
	Univ2noIr = 0x11,
	Univ40 = 0x15,
	Univ42 = 0x16,
	Unis10 = 0x50,
	Rc = 0x30,
};

constexpr size_t MTB_MODULE_ACTIVATIONS = 5;
QString moduleTypeToStr(MtbModuleType);

class MtbModule {
protected:
	bool active = false;
	uint8_t address;
	QString name;
	MtbModuleType type = MtbModuleType::Unknown;
	Mtb::ModuleInfo busModuleInfo;
	std::optional<ServerRequest> configWriting;
	bool beacon = false;
	size_t activationsRemaining = 0;
	bool activating = false;

	struct Rebooting {
		bool rebooting = false;
		bool activatedByMtbUsb;
		std::function<void()> onOk;
		std::function<void()> onError;
	};
	Rebooting rebooting;

	struct FwUpgrade {
		static constexpr size_t BLOCK_SIZE = 64;
		using FirmwareStorage = std::map<size_t, std::vector<uint8_t>>;
		std::optional<ServerRequest> fwUpgrading;
		FirmwareStorage data; // block to data map
		FirmwareStorage::iterator toWrite;
	};
	FwUpgrade fwUpgrade;

	void sendInputsChanged(QJsonObject inputs) const;
	void sendOutputsChanged(QJsonObject outputs, const std::vector<QTcpSocket*> &ignore) const;
	void sendModuleInfo(QTcpSocket *ignore = nullptr, bool sendConfig = false) const;

	virtual void jsonSetOutput(QTcpSocket*, const QJsonObject&);
	virtual void jsonUpgradeFw(QTcpSocket*, const QJsonObject&);
	virtual void jsonReboot(QTcpSocket*, const QJsonObject&);
	virtual void jsonSpecificCommand(QTcpSocket*, const QJsonObject&);
	virtual void jsonBeacon(QTcpSocket*, const QJsonObject&);
	virtual void jsonGetDiag(QTcpSocket*, const QJsonObject&);

	void fwUpgdInit();
	void fwUpgdError(const QString&, size_t code = MTB_MODULE_FWUPGD_ERROR);
	void fwUpgdReqAck();
	void fwUpgdGotInfo(Mtb::ModuleInfo);
	void fwUpgdGetStatus();
	void fwUpgdGotStatus(Mtb::FwWriteFlashStatus);
	void fwUpgdAllWritten();
	void fwUpgdRebooted();

	static std::map<size_t, std::vector<uint8_t>> parseFirmware(const QJsonObject&);

	void reboot(std::function<void()> onOk, std::function<void()> onError);
	void fullyActivated();
	void activationError(Mtb::CmdError);

	void mlog(const QString& message, Mtb::LogLevel) const;

	virtual QJsonObject dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const;

	void mtbBusDiagStateChanged(bool isError, bool isWarning);

public:
	MtbModule(uint8_t addr);
	virtual ~MtbModule() = default;

	MtbModuleType moduleType() const;
	bool isActive() const;
	bool isRebooting() const;
	bool isBeacon() const;
	bool isActivating() const;
	bool isFirmwareUpgrading() const;
	bool isConfigSetting() const;

	virtual QJsonObject moduleInfo(bool state, bool config) const;

	virtual void mtbBusActivate(Mtb::ModuleInfo);
	virtual void mtbBusLost();
	virtual void mtbBusInputsChanged(const std::vector<uint8_t>&);
	virtual void mtbBusDiagStateChanged(const std::vector<uint8_t>&);
	virtual void mtbUsbDisconnected();

	virtual void jsonCommand(QTcpSocket*, const QJsonObject&, bool hasWriteAccess);
	virtual void jsonSetConfig(QTcpSocket*, const QJsonObject&);
	virtual void jsonSetAddress(QTcpSocket*, const QJsonObject&);

	virtual void loadConfig(const QJsonObject&);
	virtual void saveConfig(QJsonObject&) const;

	virtual std::vector<QTcpSocket*> outputSetters() const;
	virtual void resetOutputsOfClient(QTcpSocket*);
	virtual void allOutputsReset();
	virtual void clientDisconnected(QTcpSocket*);
	virtual bool fwDeprecated() const;

	virtual void reactivateCheck();

	virtual QString DVToStr(uint8_t dv) const;
	virtual std::optional<uint8_t> StrToDV(const QString&) const;

private:

};

#endif
