#ifndef MODULE_MTB_UNI_H
#define MODULE_MTB_UNI_H

#include "module.h"

constexpr size_t UNI_IO_CNT = 16;

struct MtbUniConfig {
	std::array<uint8_t, 16> outputsSafe;
	std::array<size_t, 16> inputsDelay; // 0 = 0s, 1 = 0.1s, 15 = 1.5s, min=0, max=15
	uint16_t irs;

	std::vector<uint8_t> serializeForMtbUsb(bool withIrs) const;
	QJsonObject json(bool withIrs) const;
};

class MtbUni : public MtbModule {
protected:
	uint16_t inputs;
	MtbUniConfig config;

	void configSet();
	bool isIrSupport() const;

	void storeInputsState(const std::vector<uint8_t>&);
	void inputsRead(const std::vector<uint8_t>&);

public:
	virtual ~MtbUni() {}
	QJsonObject moduleInfo(bool state) const override;

	void mtbBusActivate(Mtb::ModuleInfo) override;

	void jsonSetOutput(QTcpSocket&, const QJsonObject&) override;
	void jsonSetConfig(QTcpSocket&, const QJsonObject&) override;
	void jsonUpgradeFw(QTcpSocket&, const QJsonObject&) override;
};

#endif
