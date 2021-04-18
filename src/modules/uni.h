#ifndef MODULE_MTB_UNI_H
#define MODULE_MTB_UNI_H

#include "module.h"
#include "../server.h"

constexpr size_t UNI_IO_CNT = 16;

struct MtbUniConfig {
	std::array<uint8_t, UNI_IO_CNT> outputsSafe;
	std::array<size_t, UNI_IO_CNT> inputsDelay; // 0 = 0s, 1 = 0.1s, 15 = 1.5s, min=0, max=15
	uint16_t irs;

	std::vector<uint8_t> serializeForMtbUsb(bool withIrs) const;
	QJsonObject json(bool withIrs) const;
};

class MtbUni : public MtbModule {
protected:
	uint16_t inputs;
	std::array<uint8_t, UNI_IO_CNT> outputsWant;
	std::array<uint8_t, UNI_IO_CNT> outputsConfirmed;
	MtbUniConfig config;

	std::vector<ServerRequest> setOutputsWaiting;
	std::vector<ServerRequest> setOutputsSent;

	void configSet();
	bool isIrSupport() const;

	void storeInputsState(const std::vector<uint8_t>&);
	void inputsRead(const std::vector<uint8_t>&);
	void outputsReset();
	void outputsSet(uint8_t, const std::vector<uint8_t>&);
	static QJsonObject outputsToJson(const std::array<uint8_t, UNI_IO_CNT>&);

	void mtbBusSetOutputs();
	void mtbBusOutputsSet(const std::vector<uint8_t>& data);
	void mtbBusOutputsNotSet(Mtb::CmdError);

	std::vector<uint8_t> mtbBusOutputsData() const;
	static std::array<uint8_t, UNI_IO_CNT> moduleOutputsData(const std::vector<uint8_t> mtbBusData);

	static uint8_t flickPerMinToMtbUniValue(size_t flickPerMin);
	static size_t flickMtbUniToPerMin(uint8_t mtbUniFlick);

public:
	virtual ~MtbUni() {}
	QJsonObject moduleInfo(bool state) const override;

	void mtbBusActivate(Mtb::ModuleInfo) override;

	void jsonSetOutput(QTcpSocket*, const QJsonObject&) override;
	void jsonSetConfig(QTcpSocket*, const QJsonObject&) override;
	void jsonUpgradeFw(QTcpSocket*, const QJsonObject&) override;
};

#endif
