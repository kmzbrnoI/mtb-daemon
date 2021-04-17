#ifndef MODULE_MTB_UNI_H
#define MODULE_MTB_UNI_H

#include "module.h"

class MtbUni : public MtbModule {
protected:
	uint16_t inputs;

public:
	virtual ~MtbUni() {}
	QJsonObject moduleInfo(bool state) const override;

	void jsonSetOutput(QTcpSocket&, const QJsonObject&) override;
	void jsonSetConfig(QTcpSocket&, const QJsonObject&) override;
	void jsonUpgradeFw(QTcpSocket&, const QJsonObject&) override;
};

#endif
