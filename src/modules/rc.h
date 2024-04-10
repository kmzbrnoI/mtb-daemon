#ifndef MODULE_MTB_RC_H
#define MODULE_MTB_RC_H

#include <set>
#include <QMap>
#include "module.h"
#include "server.h"

constexpr size_t RC_IN_CNT = 8;
using DccAddr = uint16_t;

const QMap<uint8_t, QString> dvsRC {
	{32, "cutouts_started"},
	{33, "cutouts_finished"},
	{34, "cutouts_timeout"},
	{35, "cutouts_data_ch1"},
	{36, "cutouts_data_ch2"},
	{37, "cutouts_no_ready_to_parse"},

	{40, "ch1_invalid_checksum"},
	{41, "ch2_invalid_checksum"},
	{42, "addr1_received_count_resets"},
	{43, "addr2_received_count_resets"},
	{44, "appid_adr_low_received"},
	{45, "appid_adr_high_received"},

	{50, "t0_ch1_invalid_checksum"},
	{51, "t0_ch2_invalid_checksum"},
	{52, "t0_addr1_received_count_resets"},
	{53, "t0_addr2_received_count_resets"},
	{54, "t0_appid_adr_low_received"},
	{55, "t0_appid_adr_high_received"},

	{60, "t1_ch1_invalid_checksum"},
	{61, "t1_ch2_invalid_checksum"},
	{62, "t1_addr1_received_count_resets"},
	{63, "t1_addr2_received_count_resets"},
	{64, "t1_appid_adr_low_received"},
	{65, "t1_appid_adr_high_received"},

	{70, "t2_ch1_invalid_checksum"},
	{71, "t2_ch2_invalid_checksum"},
	{72, "t2_addr1_received_count_resets"},
	{73, "t2_addr2_received_count_resets"},
	{74, "t2_appid_adr_low_received"},
	{75, "t2_appid_adr_high_received"},

	{80, "t3_ch1_invalid_checksum"},
	{81, "t3_ch2_invalid_checksum"},
	{82, "t3_addr1_received_count_resets"},
	{83, "t3_addr2_received_count_resets"},
	{84, "t3_appid_adr_low_received"},
	{85, "t3_appid_adr_high_received"},

	{90, "t4_ch1_invalid_checksum"},
	{91, "t4_ch2_invalid_checksum"},
	{92, "t4_addr1_received_count_resets"},
	{93, "t4_addr2_received_count_resets"},
	{94, "t4_appid_adr_low_received"},
	{95, "t4_appid_adr_high_received"},

	{100, "t5_ch1_invalid_checksum"},
	{101, "t5_ch2_invalid_checksum"},
	{102, "t5_addr1_received_count_resets"},
	{103, "t5_addr2_received_count_resets"},
	{104, "t5_appid_adr_low_received"},
	{105, "t5_appid_adr_high_received"},

	{110, "t6_ch1_invalid_checksum"},
	{111, "t6_ch2_invalid_checksum"},
	{112, "t6_addr1_received_count_resets"},
	{113, "t6_addr2_received_count_resets"},
	{114, "t6_appid_adr_low_received"},
	{115, "t6_appid_adr_high_received"},

	{120, "t7_ch1_invalid_checksum"},
	{121, "t7_ch2_invalid_checksum"},
	{122, "t7_addr1_received_count_resets"},
	{123, "t7_addr2_received_count_resets"},
	{124, "t7_appid_adr_low_received"},
	{125, "t7_appid_adr_high_received"},

	{130, "dcc_received_packets"},
	{131, "dcc_received_bad_xor"},
	{132, "logical_0_preamble_soon"},
	{133, "mobile_reads_count"},
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
