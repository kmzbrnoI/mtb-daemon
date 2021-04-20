#ifndef MODULE_H
#define MODULE_H

#include <QTcpSocket>
#include <QJsonObject>
#include "../mtbusb/mtbusb-commands.h"
#include "../server.h"

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

	struct FwUpgrade {
		std::optional<ServerRequest> fwUpgrading;
		std::map<size_t, std::vector<uint8_t>> data; // address to data map
		size_t writtenPage;
	};
	FwUpgrade fwUpgrade;

	void sendInputsChanged(QJsonArray inputs) const;
	void sendOutputsChanged(QJsonObject outputs, const std::vector<QTcpSocket*> ignore) const;
	void sendChanged(QTcpSocket* ignore = nullptr) const;

	bool isFirmwareUpgrading() const;
	bool isConfigSetting() const;

	void fwUpgdInit();

public:
	MtbModule(uint8_t addr);
	virtual ~MtbModule() {}

	MtbModuleType moduleType() const;
	bool isActive() const;

	virtual QJsonObject moduleInfo(bool state) const;

	virtual void mtbBusActivate(Mtb::ModuleInfo);
	virtual void mtbBusLost();
	virtual void mtbBusInputsChanged(const std::vector<uint8_t>);
	virtual void mtbUsbDisconnected();

	virtual void jsonCommand(QTcpSocket*, const QJsonObject&);
	virtual void jsonSetOutput(QTcpSocket*, const QJsonObject&);
	virtual void jsonSetConfig(QTcpSocket*, const QJsonObject&);
	virtual void jsonUpgradeFw(QTcpSocket*, const QJsonObject&);
	virtual void clientDisconnected(QTcpSocket*);

	virtual void loadConfig(const QJsonObject&);
	virtual void saveConfig(QJsonObject&) const;

private:

};

#endif
