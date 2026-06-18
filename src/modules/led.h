#ifndef _MODULE_MTB_LED_H_
#define _MODULE_MTB_LED_H_

#include "module.h"
#include "server.h"

constexpr size_t LED_IO_CNT = 32;

struct MtbLedConfig {
	std::array<bool, LED_IO_CNT> outputsSafe = {false, };
	std::array<uint8_t, LED_IO_CNT> brightness = {100, };

	MtbLedConfig() {} // default config
	MtbLedConfig(const QJsonObject& json) { this->fromJson(json); }
	MtbLedConfig(const std::vector<uint8_t>& mtbUsbData) { this->fromMtbUsb(mtbUsbData); }

	std::vector<uint8_t> serializeForMtbUsb() const;
	void fromMtbUsb(const std::vector<uint8_t>&);

	void fromJson(const QJsonObject&);
	QJsonObject json() const;

	bool operator==(const MtbLedConfig& other) const {
		return (this->outputsSafe == other.outputsSafe) && (this->brightness == other.brightness);
	}

	bool operator!=(const MtbLedConfig& other) const { return !(*this == other); }
};

class MtbLed : public MtbModule {
	friend MtbLedConfig;
	static constexpr float ADCBG = 1.1;
	static constexpr size_t PAGE_SIZE = 128;

protected:
	std::array<bool, LED_IO_CNT> inputs;
	std::array<bool, LED_IO_CNT> outputsWant;
	std::array<bool, LED_IO_CNT> outputsConfirmed;

	std::optional<MtbLedConfig> config;
	std::optional<MtbLedConfig> configToWrite;
	std::array<QTcpSocket*, LED_IO_CNT> whoSetOutput;

	std::vector<ServerRequest> setOutputsWaiting;
	std::vector<ServerRequest> setOutputsSent;

	void configSet();

	void inputsRead(const std::vector<uint8_t>&);
	void outputsReset();
	void outputsSet(uint8_t, const std::vector<uint8_t>&);
	static QJsonObject ioStateToJson(const std::array<bool, LED_IO_CNT>&);

	void jsonSetOutput(QTcpSocket*, const QJsonObject&) override;
	void jsonUpgradeFw(QTcpSocket*, const QJsonObject&) override;

	void setOutputs();
	void mtbBusOutputsSet(const std::vector<uint8_t> &data);
	void mtbBusOutputsNotSet(Mtb::CmdError);
	void mtbBusConfigWritten();
	void mtbBusConfigNotWritten(Mtb::CmdError);

	void activate();

	static std::vector<uint8_t> ioToMtb(const std::array<bool, LED_IO_CNT>&);
	static std::array<bool, LED_IO_CNT> mtbDataToIo(const std::vector<uint8_t>& mtbBusData);

	QJsonObject dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const override;

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
};

#endif
