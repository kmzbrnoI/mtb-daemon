#ifndef _MODULE_MTB_UNI_H_
#define _MODULE_MTB_UNI_H_

#include "module.h"
#include "server.h"

constexpr size_t UNI_IO_CNT = 16;
constexpr uint16_t UNIv2_FW_DEPRECATED = 0x0103; // FW <= UNI_DEPRECATED is marked as deprecated
constexpr uint16_t UNIv4_FW_DEPRECATED = 0x0104; // FW <= UNI_DEPRECATED is marked as deprecated

struct MtbUniConfig {
	std::array<uint8_t, UNI_IO_CNT> outputsSafe = {0, };
	std::array<size_t, UNI_IO_CNT> inputsDelay = {0, }; // 0 = 0s, 1 = 0.1s, 15 = 1.5s, min=0, max=15
	uint16_t irs = 0;

	MtbUniConfig() {} // default config
	MtbUniConfig(const QJsonObject& json) { this->fromJson(json); }
	MtbUniConfig(const std::vector<uint8_t>& mtbUsbData) { this->fromMtbUsb(mtbUsbData); }

	std::vector<uint8_t> serializeForMtbUsb(bool withIrs) const;
	void fromMtbUsb(const std::vector<uint8_t>&);

	void fromJson(const QJsonObject&);
	QJsonObject json(bool withIrs, bool file) const;

	bool operator==(const MtbUniConfig& other) const {
		return ((this->outputsSafe == other.outputsSafe) &&
		        (this->inputsDelay == other.inputsDelay) &&
		        (this->irs == other.irs));
	}

	bool operator!=(const MtbUniConfig& other) const { return !(*this == other); }
};

class MtbUni : public MtbModule {
	friend MtbUniConfig;

protected:
	uint16_t inputs;
	std::array<uint8_t, UNI_IO_CNT> outputsWant;
	std::array<uint8_t, UNI_IO_CNT> outputsConfirmed;
	std::optional<MtbUniConfig> config;
	std::optional<MtbUniConfig> configToWrite;
	std::array<QTcpSocket*, UNI_IO_CNT> whoSetOutput;

	std::vector<ServerRequest> setOutputsWaiting;
	std::vector<ServerRequest> setOutputsSent;

	void configSet();
	bool isIrSupport() const;
	size_t pageSize() const;
	bool isUniv2() const;
	bool isUniv4() const;

	void storeInputsState(const std::vector<uint8_t>&);
	void inputsRead(const std::vector<uint8_t>&);
	void outputsReset();
	void outputsSet(uint8_t, const std::vector<uint8_t>&);
	static QJsonObject outputsToJson(const std::array<uint8_t, UNI_IO_CNT>&);
	static QJsonObject inputsToJson(uint16_t inputs);

	void jsonSetOutput(QTcpSocket*, const QJsonObject&) override;
	void jsonUpgradeFw(QTcpSocket*, const QJsonObject&) override;

	void setOutputs();
	void mtbBusOutputsSet(const std::vector<uint8_t> &data);
	void mtbBusOutputsNotSet(Mtb::CmdError);
	void mtbBusConfigWritten();
	void mtbBusConfigNotWritten(Mtb::CmdError);

	void activate();

	std::vector<uint8_t> mtbBusOutputsData() const;
	static std::array<uint8_t, UNI_IO_CNT> moduleOutputsData(const std::vector<uint8_t>& mtbBusData);

	static uint8_t flickPerMinToMtbUniValue(size_t flickPerMin);
	static size_t flickMtbUniToPerMin(uint8_t mtbUniFlick);

	static void alignFirmware(std::map<size_t, std::vector<uint8_t>>&, size_t pageSize);

	QJsonObject dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const override;

	float adcbg() const;

public:
	MtbUni(uint8_t addr);
	~MtbUni() override = default;
	QJsonObject moduleInfo(bool state, bool config) const override;

	void mtbBusActivate(Mtb::ModuleInfo) override;
	void mtbBusInputsChanged(const std::vector<uint8_t>&) override;
	void mtbUsbDisconnected() override;

	void jsonSetConfig(QTcpSocket*, const QJsonObject&) override;

	void loadConfig(const QJsonObject&) override;
	void saveConfig(QJsonObject&) const override;

	std::vector<QTcpSocket*> outputSetters() const override;
	void resetOutputsOfClient(QTcpSocket*) override;
	void allOutputsReset() override;
	void reactivateCheck() override;

	static uint8_t jsonOutputToByte(const QJsonObject&);

	bool fwDeprecated() const override;
};

#endif
