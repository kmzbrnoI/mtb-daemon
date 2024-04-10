#ifndef MODULE_MTB_RC_H
#define MODULE_MTB_RC_H

#include <set>
#include <QMap>
#include "module.h"
#include "server.h"

constexpr size_t RC_IN_CNT = 8;
using DccAddr = uint16_t;

enum DVRC {
	CutoutsStarted = 32,
	CutoutsFinished = 33,
	CutoutsTimeout = 34,
	CutoutsDataCh1 = 35,
	CutoutsDataCh2 = 36,
	CutoutsNoReadyToParse = 37,
};

const QMap<uint8_t, QString> dvsRC {
	{DVRC::CutoutsStarted, "cutouts_started"},
	{DVRC::CutoutsFinished, "cutouts_finished"},
	{DVRC::CutoutsTimeout, "cutouts_timeout"},
	{DVRC::CutoutsDataCh1, "cutouts_data_ch1"},
	{DVRC::CutoutsDataCh2, "cutouts_data_ch2"},
	{DVRC::CutoutsNoReadyToParse, "cutouts_no_ready_to_parse"},
};


class MtbRc : public MtbModule {
protected:
	std::array<std::set<DccAddr>, RC_IN_CNT> inputs;

	void storeInputsState(const std::vector<uint8_t>&);
	void inputsRead(const std::vector<uint8_t>&);
	QJsonObject inputsToJson() const;

	void jsonUpgradeFw(QTcpSocket*, const QJsonObject&) override;
	void activate();
	static void alignFirmware(std::map<size_t, std::vector<uint8_t>>&, size_t pageSize);

	QJsonObject dvRepr(uint8_t dvi, const std::vector<uint8_t> &data) const override;

public:
	MtbRc(uint8_t addr);
	~MtbRc() override = default;
	QJsonObject moduleInfo(bool state, bool config) const override;

	void mtbBusActivate(Mtb::ModuleInfo) override;
	void mtbBusInputsChanged(const std::vector<uint8_t>&) override;
	void mtbUsbDisconnected() override;

	void jsonSetConfig(QTcpSocket*, const QJsonObject&) override;
	void reactivateCheck() override;

	QString DVToStr(uint8_t dv) const override;
	std::optional<uint8_t> StrToDV(const QString&) const override;
};

#endif
