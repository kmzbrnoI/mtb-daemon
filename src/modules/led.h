#ifndef _MODULE_MTB_LED_H_
#define _MODULE_MTB_LED_H_

#include "module.h"
#include "server.h"

constexpr size_t LED_IO_CNT = 32;

struct MtbLedConfig {
	std::array<bool, LED_IO_CNT> outputsSafe = {false, };

	MtbLedConfig() {} // default config
	MtbLedConfig(const QJsonObject& json) { this->fromJson(json); }
	MtbLedConfig(const std::vector<uint8_t>& mtbUsbData) { this->fromMtbUsb(mtbUsbData); }

	std::vector<uint8_t> serializeForMtbUsb() const;
	void fromMtbUsb(const std::vector<uint8_t>&);

	void fromJson(const QJsonObject&);
	QJsonObject json(bool file) const;

	bool operator==(const MtbUniConfig& other) const {
		return (this->outputsSafe == other.outputsSafe);
	}

	bool operator!=(const MtbUniConfig& other) const { return !(*this == other); }
};

class MtbLed : public MtbModule {
	friend MtbLedConfig;

protected:
	std::array<bool, LED_IO_CNT> inputs;
	std::array<bool, LED_IO_CNT> outputsWant;
	std::array<bool, LED_IO_CNT> outputsConfirmed;

	std::optional<MtbUniConfig> config;
	std::optional<MtbUniConfig> configToWrite;
	std::array<QTcpSocket*, LED_IO_CNT> whoSetOutput;

	std::vector<ServerRequest> setOutputsWaiting;
	std::vector<ServerRequest> setOutputsSent;

	void configSet();
	size_t pageSize() const;

	void storeInputsState(const std::vector<uint8_t>&);
	void inputsRead(const std::vector<uint8_t>&);
	void outputsReset();
	void outputsSet(uint8_t, const std::vector<uint8_t>&);
	static QJsonObject outputsToJson(const std::array<bool, UNI_LED_CNT>&);
	static QJsonObject inputsToJson(const std::array<bool, UNI_LED_CNT>&);

	void jsonSetOutput(QTcpSocket*, const QJsonObject&) override;
	void jsonUpgradeFw(QTcpSocket*, const QJsonObject&) override;

	void setOutputs();
	void mtbBusOutputsSet(const std::vector<bool> &data);
	void mtbBusOutputsNotSet(Mtb::CmdError);
	void mtbBusConfigWritten();
	void mtbBusConfigNotWritten(Mtb::CmdError);

	void activate();

	std::vector<uint8_t> mtbBusOutputsData() const;
	static std::array<bool, LED_IO_CNT> moduleOutputsData(const std::vector<uint8_t>& mtbBusData);

	static void alignFirmware(std::map<size_t, std::vector<uint8_t>>&, size_t pageSize);

	QJsonObject dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const override;

	float adcbg() const;

public:
	MtbLed(uint8_t addr);
	~MtbLed() override = default;
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
};

#endif
