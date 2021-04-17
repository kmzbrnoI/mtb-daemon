#ifndef MODULE_MTB_UNI_H
#define MODULE_MTB_UNI_H

#include "module.h"

class MtbUni : public MtbModule {
protected:
	uint16_t inputs;

public:
	virtual ~MtbUni() {}
	QJsonObject moduleInfo() const override;
};

#endif
