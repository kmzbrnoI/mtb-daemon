#ifndef MODULE_H
#define MODULE_H

#include <QTcpSocket>
#include <QJsonObject>
#include "../mtbusb/mtbusb-commands.h"
#include "../server.h"
#include "../errors.h"

enum class MtbModuleType {
	Uknown = 0x00,
	Univ2ir = 0x10,
	Univ2noIr = 0x11,
	Univ40 = 0x15,
	Univ42 = 0x16,
};

QString moduleTypeToStr(MtbModuleType);

class MtbModule {
protected:
	bool active = false;
	uint8_t address;
	QString name;
	MtbModuleType type = MtbModuleType::Uknown;
	Mtb::ModuleInfo busModuleInfo;
	std::optional<ServerRequest> configWriting;

	struct Rebooting {
		bool rebooting;
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

	void sendInputsChanged(QJsonArray inputs) const;
	void sendOutputsChanged(QJsonObject outputs, const std::vector<QTcpSocket*> ignore) const;
	void sendModuleInfo(QTcpSocket* ignore = nullptr) const;

	bool isFirmwareUpgrading() const;
	bool isConfigSetting() const;

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

public:
	MtbModule(uint8_t addr);
	virtual ~MtbModule() {}

	MtbModuleType moduleType() const;
	bool isActive() const;
	bool isRebooting() const;

	virtual QJsonObject moduleInfo(bool state, bool config) const;

	virtual void mtbBusActivate(Mtb::ModuleInfo);
	virtual void mtbBusLost();
	virtual void mtbBusInputsChanged(const std::vector<uint8_t>);
	virtual void mtbUsbDisconnected();

	virtual void jsonCommand(QTcpSocket*, const QJsonObject&);
	virtual void jsonSetOutput(QTcpSocket*, const QJsonObject&);
	virtual void jsonSetConfig(QTcpSocket*, const QJsonObject&);
	virtual void jsonUpgradeFw(QTcpSocket*, const QJsonObject&);
	virtual void jsonReboot(QTcpSocket*, const QJsonObject&);
	virtual void jsonSpecificCommand(QTcpSocket*, const QJsonObject&);
	virtual void clientDisconnected(QTcpSocket*);

	virtual void loadConfig(const QJsonObject&);
	virtual void saveConfig(QJsonObject&) const;

private:

};

#endif
