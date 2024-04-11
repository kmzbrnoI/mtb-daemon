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

	{40, "ch1_invalid_data"},
	{41, "ch2_invalid_data"},
	{42, "addr1_received_count_resets"},
	{43, "addr2_received_count_resets"},
	{44, "appid_adr_low_received"},
	{45, "appid_adr_high_received"},
	{46, "ch1_addr_added"},
	{47, "ch2_addr_added"},

	{50, "t0_ch1_invalid_data"},
	{51, "t0_ch2_invalid_data"},
	{52, "t0_addr1_received_count_resets"},
	{53, "t0_addr2_received_count_resets"},
	{54, "t0_appid_adr_low_received"},
	{55, "t0_appid_adr_high_received"},
	{56, "t0_ch1_addr_added"},
	{57, "t0_ch2_addr_added"},

	{60, "t1_ch1_invalid_data"},
	{61, "t1_ch2_invalid_data"},
	{62, "t1_addr1_received_count_resets"},
	{63, "t1_addr2_received_count_resets"},
	{64, "t1_appid_adr_low_received"},
	{65, "t1_appid_adr_high_received"},
	{66, "t1_ch1_addr_added"},
	{67, "t1_ch2_addr_added"},

	{70, "t2_ch1_invalid_data"},
	{71, "t2_ch2_invalid_data"},
	{72, "t2_addr1_received_count_resets"},
	{73, "t2_addr2_received_count_resets"},
	{74, "t2_appid_adr_low_received"},
	{75, "t2_appid_adr_high_received"},
	{76, "t2_ch1_addr_added"},
	{77, "t2_ch2_addr_added"},

	{80, "t3_ch1_invalid_data"},
	{81, "t3_ch2_invalid_data"},
	{82, "t3_addr1_received_count_resets"},
	{83, "t3_addr2_received_count_resets"},
	{84, "t3_appid_adr_low_received"},
	{85, "t3_appid_adr_high_received"},
	{86, "t3_ch1_addr_added"},
	{87, "t3_ch2_addr_added"},

	{90, "t4_ch1_invalid_data"},
	{91, "t4_ch2_invalid_data"},
	{92, "t4_addr1_received_count_resets"},
	{93, "t4_addr2_received_count_resets"},
	{94, "t4_appid_adr_low_received"},
	{95, "t4_appid_adr_high_received"},
	{96, "t4_ch1_addr_added"},
	{97, "t4_ch2_addr_added"},

	{100, "t5_ch1_invalid_data"},
	{101, "t5_ch2_invalid_data"},
	{102, "t5_addr1_received_count_resets"},
	{103, "t5_addr2_received_count_resets"},
	{104, "t5_appid_adr_low_received"},
	{105, "t5_appid_adr_high_received"},
	{106, "t5_ch1_addr_added"},
	{107, "t5_ch2_addr_added"},

	{110, "t6_ch1_invalid_data"},
	{111, "t6_ch2_invalid_data"},
	{112, "t6_addr1_received_count_resets"},
	{113, "t6_addr2_received_count_resets"},
	{114, "t6_appid_adr_low_received"},
	{115, "t6_appid_adr_high_received"},
	{116, "t6_ch1_addr_added"},
	{117, "t6_ch2_addr_added"},

	{120, "t7_ch1_invalid_data"},
	{121, "t7_ch2_invalid_data"},
	{122, "t7_addr1_received_count_resets"},
	{123, "t7_addr2_received_count_resets"},
	{124, "t7_appid_adr_low_received"},
	{125, "t7_appid_adr_high_received"},
	{126, "t7_ch1_addr_added"},
	{127, "t7_ch2_addr_added"},

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
