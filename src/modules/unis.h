#ifndef MODULE_MTB_UNIS_H
#define MODULE_MTB_UNIS_H

#include "module.h"
#include "../server.h"

constexpr size_t UNIS_IO_CNT = 16;
constexpr size_t UNIS_SERVO_CNT = 6;
constexpr size_t UNIS_SERVO_OUT_CNT = (2*UNIS_SERVO_CNT);
constexpr size_t UNIS_OUT_CNT = (UNIS_IO_CNT + UNIS_SERVO_OUT_CNT);
constexpr size_t UNIS_IN_CNT = (UNIS_IO_CNT);

struct MtbUnisConfig {
  std::array<uint8_t, UNIS_OUT_CNT> outputsSafe = {0, };
  std::array<size_t, UNIS_IN_CNT> inputsDelay = {0, }; // 0 = 0s, 1 = 0.1s, 15 = 1.5s, min=0, max=15
  uint8_t servoEnabledMask = 0;
  std::array<uint8_t, UNIS_SERVO_OUT_CNT> servoPosition = {0, };
  std::array<uint8_t, UNIS_SERVO_CNT> servoSpeed = {0, };

  MtbUnisConfig(const QJsonObject& json) { this->fromJson(json); }
  MtbUnisConfig(const std::vector<uint8_t>& mtbUsbData) { this->fromMtbUsb(mtbUsbData); }

  std::vector<uint8_t> serializeForMtbUsb() const;
  void fromMtbUsb(const std::vector<uint8_t>&);

  void fromJson(const QJsonObject&);
  QJsonObject json() const;

  bool operator==(const MtbUnisConfig& other) const {
    return ((this->outputsSafe == other.outputsSafe) &&
            (this->inputsDelay == other.inputsDelay) &&
            (this->servoEnabledMask == other.servoEnabledMask) &&
            (this->servoPosition == other.servoPosition) &&
            (this->servoSpeed == other.servoSpeed));
  }

  bool operator!=(const MtbUnisConfig& other) const { return !(*this == other); }
};

class MtbUnis : public MtbModule {
protected:
  uint16_t inputs;
  std::array<uint8_t, UNIS_OUT_CNT> outputsWant;
  std::array<uint8_t, UNIS_OUT_CNT> outputsConfirmed;
  std::optional<MtbUnisConfig> config;
  std::optional<MtbUnisConfig> configToWrite;
  std::array<QTcpSocket*, UNIS_OUT_CNT> whoSetOutput;

  std::vector<ServerRequest> setOutputsWaiting;
  std::vector<ServerRequest> setOutputsSent;

  void configSet();
  bool isIrSupport() const;
  size_t pageSize() const;

  void storeInputsState(const std::vector<uint8_t>&);
  void inputsRead(const std::vector<uint8_t>&);
  void outputsReset();
  void outputsSet(uint8_t, const std::vector<uint8_t>&);
  static QJsonObject outputsToJson(const std::array<uint8_t, UNIS_OUT_CNT>&);
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
  static std::array<uint8_t, UNIS_OUT_CNT> moduleOutputsData(const std::vector<uint8_t>& mtbBusData);

  static uint8_t flickPerMinToMtbUnisValue(size_t flickPerMin);
  static size_t flickMtbUnisToPerMin(uint8_t MtbUnisFlick);

  static void alignFirmware(std::map<size_t, std::vector<uint8_t>>&, size_t pageSize);

  QJsonObject dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const override;

  float adcbg() const;

public:
  MtbUnis(uint8_t addr);
  ~MtbUnis() override = default;
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
