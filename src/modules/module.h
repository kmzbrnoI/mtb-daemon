#ifndef MODULE_H
#define MODULE_H

#include <QTcpSocket>
#include <QJsonObject>
#include "../mtbusb/mtbusb-commands.h"

enum class MtbModuleType {
	Univ2ir = 0x10,
	Univ2noIr = 0x11,
	Univ40 = 0x15,
	Univ42 = 0x16,
};

QString moduleTypeToStr(MtbModuleType);

class MtbModule {
protected:
	bool active;
	uint8_t address;
	QString name;
	MtbModuleType type;
	Mtb::ModuleInfo busModuleInfo;

	void sendInputsChanged(QJsonArray inputs) const;
	void sendOutputsChanged(QJsonObject outputs, const std::vector<QTcpSocket*> ignore) const;

public:
	virtual ~MtbModule() {}
	virtual QJsonObject moduleInfo(bool state) const;

	virtual void mtbBusActivate(Mtb::ModuleInfo);
	virtual void mtbBusLost();
	virtual void mtbBusInputsChanged(const std::vector<uint8_t>);

	virtual void jsonCommand(QTcpSocket*, const QJsonObject&);
	virtual void jsonSetOutput(QTcpSocket*, const QJsonObject&);
	virtual void jsonSetConfig(QTcpSocket*, const QJsonObject&);
	virtual void jsonUpgradeFw(QTcpSocket*, const QJsonObject&);

	virtual void loadConfig(const QJsonObject&);
	virtual void saveConfig(QJsonObject&) const;

private:

};

#endif
