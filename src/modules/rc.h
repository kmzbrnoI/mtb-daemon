#ifndef MODULE_MTB_RC_H
#define MODULE_MTB_RC_H

#include <set>
#include "module.h"
#include "server.h"

constexpr size_t RC_IN_CNT = 8;
using DccAddr = uint16_t;

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
};

#endif
