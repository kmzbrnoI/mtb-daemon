#include "mtbusb.h"
#include "mtbusb-common.h"

namespace Mtb {

void MtbUsb::spHandleReadyRead() {
	// check timeout
	if ((m_receiveTimeout < QDateTime::currentDateTime()) && (m_readData.size() > 0)) {
		// clear input buffer when data not received for a long time
		log("Cleared BUF due to timeout", LogLevel::Debug);
		m_readData.clear();
	}

	m_readData.append(m_serialPort.readAll());
	m_receiveTimeout = QDateTime::currentDateTime().addMSecs(_BUF_IN_TIMEOUT);

	log("BUF: " + dataToStr<QByteArray, uint8_t>(m_readData, m_readData.size()), LogLevel::Debug);

	// remove message till 0x2A 0x42
	int pos = m_readData.indexOf(QByteArray("\x2A\x42"));
	if (pos != 0)
		log("Removing incoming message leading data!", LogLevel::Warning);
	if (pos == -1)
		pos = m_readData.size();
	m_readData.remove(0, pos);

	while (m_readData.size() > 2 && m_readData.size() >= (m_readData[2]) + 3) {
		unsigned int length = m_readData[2];

		log("GET: " + dataToStr<QByteArray, uint8_t>(m_readData, length+3), LogLevel::RawData);

		auto begin = m_readData.begin();
		++begin;
		++begin;
		++begin;
		++begin;

		std::vector<uint8_t> data(begin, begin + length - 1);
		try {
			parseMtbUsbMessage(m_readData[3], data); // without 0x2A 0x42 length; just command code & data
		} catch (const std::logic_error& err) {
			log("MTB received data Exception: "+QString(err.what()), LogLevel::Error);
		} catch (...) {
			log("MTB received data Exception: unknown", LogLevel::Error);
		}
		m_readData.remove(0, static_cast<int>(length + 3));
	}

	// Set timeout again to avoid buf clear because of long processing time (long message)
	m_receiveTimeout = QDateTime::currentDateTime().addMSecs(_BUF_IN_TIMEOUT);
}

void MtbUsb::parseMtbUsbMessage(uint8_t command_code, const std::vector<uint8_t> &data) {
	switch (static_cast<MtbUsbRecvCommand>(command_code)) {
	case MtbUsbRecvCommand::Ack:
		log("GET: ACK", LogLevel::Commands);
		break;

	case MtbUsbRecvCommand::Error:
		if (data.size() >= 3)
			handleMtbUsbError(data[0], data[1], data[2]);
		return; // error is fully processed only here

	case MtbUsbRecvCommand::MtbBusForward:
		if (data.size() >= 2) {
			std::vector<uint8_t> mtbBusData(data.begin()+3, data.end());
			parseMtbBusMessage(data[1], data[0], data[2], mtbBusData);
		}
		return; // fully processes in parseMtbBusMessage

	case MtbUsbRecvCommand::MtbUsbInfo:
		if (data.size() >= 6) {
			MtbUsbInfo info;
			info.type = data[0];
			info.speed = static_cast<MtbBusSpeed>(data[1] & 0x0F);
			info.fw_major = data[2];
			info.fw_minor = data[3];
			info.proto_major = data[4];
			info.proto_minor = data[5];
			m_mtbUsbInfo = info;
			log("GET: MTB-USB info: type 0x"+QString::number(info.type, 16)+", fw: "+info.fw_version()+
			    ", speed: "+QString::number(mtbBusSpeedToInt(info.speed))+", protocol: "+info.proto_version(),
			    LogLevel::Commands);
			if (info.fw_deprecated())
				log("MTB-USB firmware is deprecated! Upgrade to the newer firmware!", LogLevel::Warning);
		}
		break;

	case MtbUsbRecvCommand::ActiveModules:
		if (data.size() >= 32) {
			std::array<bool, _MAX_MODULES> activeModules;
			for (size_t i = 0; i < _MAX_MODULES; i++)
				activeModules[i] = (data[i/8] >> (i%8)) & 0x1;
			m_activeModules = activeModules;
			log("GET: active modules list", LogLevel::Commands);
		}
		break;

	case MtbUsbRecvCommand::NewModule:
		if (data.size() >= 1) {
			log("GET: new module "+QString::number(data[0]), LogLevel::Commands);
			if (m_activeModules.has_value()) {
				m_activeModules.value()[data[0]] = true;
				emit onNewModule(data[0]);
			}
		}
		return; // event cannot be response to command

	case MtbUsbRecvCommand::ModuleFailed:
		if (data.size() >= 2) {
			log("GET: module "+QString::number(data[0])+" no response for inquiry, remaining attempts: "+
			    QString::number(data[1]), LogLevel::Commands);
			if (data[1] == 0) {
				log("GET: module "+QString::number(data[0])+" failed", LogLevel::Commands);
				if (m_activeModules.has_value()) {
					m_activeModules.value()[data[0]] = false;
					emit onModuleFail(data[0]);
				}
			}
		}
		return; // event cannot be response to command

	default:
		break;
	}

	// Find appropriate history item & call its ok callback
	for (size_t i = 0; i < m_hist.size(); i++) {
		const Cmd* cmd = m_hist[i].cmd.get();
		if (cmd->processUsbResponse(static_cast<MtbUsbRecvCommand>(command_code), data)) {
			// Find this item in history again, because error callback may sent new command and thus invalidated m_hist iterators
			for (auto it = m_hist.begin(); it != m_hist.end(); ++it) {
				if (it->cmd.get() == cmd) {
					m_hist.erase(it);
					if (!m_out.empty())
						this->sendNextOut();
					return;
				}
			}
			return;
		}
	}

	log("GET: unknown/unmatched MTB-USB command "+QString::number(command_code), LogLevel::Warning);
}

void MtbUsb::parseMtbBusMessage(uint8_t module, uint8_t attempts, uint8_t command_code,
                                const std::vector<uint8_t> &data) {
	MtbBusRecvCommand command = static_cast<MtbBusRecvCommand>(command_code);

	if (command == MtbBusRecvCommand::DiagValue) {
		if (attempts > 1)
			log("Got attempts="+QString::number(attempts)+" for DiagValue command!", LogLevel::Warning);
	} else if (isBusEvent(command)) {
		if (attempts != 0)
			log("Got attempts="+QString::number(attempts)+" for event!", LogLevel::Warning);
	} else {
		if (attempts != 1)
			log("Got attempts="+QString::number(attempts)+" for non-event!", LogLevel::Warning);
	}

	switch (command) {
	case MtbBusRecvCommand::Error:
		handleMtbBusError(data[0], module);
		return;

	case MtbBusRecvCommand::Acknowledgement:
		log("GET: module "+QString::number(module)+" acknowledgement", LogLevel::Commands);
		break;

	case MtbBusRecvCommand::ModuleInfo:
		log("GET: module "+QString::number(module)+" information", LogLevel::Commands);
		break;

	case MtbBusRecvCommand::ModuleConfig:
		log("GET: module "+QString::number(module)+" configuration", LogLevel::Commands);
		break;

	case MtbBusRecvCommand::InputChanged:
		log("GET: module "+QString::number(module)+" inputs changed", LogLevel::Commands);
		emit onModuleInputsChange(module, data);
		return; // event = return

	case MtbBusRecvCommand::InputState:
		log("GET: module "+QString::number(module)+" inputs state", LogLevel::Commands);
		break;

	case MtbBusRecvCommand::OutputSet:
		log("GET: module "+QString::number(module)+" outputs set", LogLevel::Commands);
		break;

	case MtbBusRecvCommand::DiagValue:
		if (data.size() > 0)
			log("GET: module "+QString::number(module)+" DV "+QString::number(data[0]), LogLevel::Commands);
		break;

	case MtbBusRecvCommand::FWWriteFlashStatus:
		log("GET: module "+QString::number(module)+" firmware write flash status", LogLevel::Commands);
		break;

	case MtbBusRecvCommand::ModuleSpecific:
		log("GET: module "+QString::number(module)+" specific command", LogLevel::Commands);
		break;
	}

	// Find appropriate history item & call its ok callback
	for (size_t i = 0; i < m_hist.size(); i++) {
		if (is<CmdMtbUsbForward>(*m_hist[i].cmd)) {
			const CmdMtbUsbForward &forward = dynamic_cast<const CmdMtbUsbForward&>(*m_hist[i].cmd);
			if ((forward.module == module) &&
			    (forward.processBusResponse(command, data))) {
				for (auto it = m_hist.begin(); it != m_hist.end(); ++it) {
					if (it->cmd.get() == m_hist[i].cmd.get()) {
						m_hist.erase(it);
						if (!m_out.empty())
							this->sendNextOut();
						return;
					}
				}
				return;
			}
		}
	}

	if ((command == MtbBusRecvCommand::DiagValue) && (data.size() > 1) && (data[0] == DVCommon::State)) {
		// asynchronous diagnostic information change
		const std::vector<uint8_t> dvdata = {data.begin()+1, data.end()};
		emit onModuleDiagStateChange(module, dvdata);
		return;
	}

	log("GET: unknown/unmatched MTBbus command 0x"+QString::number(command_code, 16), LogLevel::Warning);
}

void MtbUsb::handleMtbUsbError(uint8_t code, uint8_t out_command_code, uint8_t addr) {
	MtbUsbRecvError error = static_cast<MtbUsbRecvError>(code);
	if (error == MtbUsbRecvError::NoResponse) {
		for (size_t i = 0; i < m_hist.size(); i++) {
			if (is<CmdMtbUsbForward>(*m_hist[i].cmd)) {
				const CmdMtbUsbForward &forward = dynamic_cast<const CmdMtbUsbForward&>(*m_hist[i].cmd);
				if ((out_command_code == forward.busCommandCode) && (addr == forward.module)) {
					log("GET: error: no response from module "+QString::number(addr)+" to command "+forward.msg(),
					    LogLevel::Error);
					histTimeoutError(CmdError::BusNoResponse, i);
					return;
				}
			}
		}

		log("GET: error not paired with outgoing command (code "+QString::number(code)+", out command code: 0x"+
		    QString::number(out_command_code, 16)+", addr "+QString::number(addr)+")", LogLevel::Error);

	} else if (error == MtbUsbRecvError::FullBuffer) {
		log("GET: error: full buffer (code "+QString::number(code)+", out command code: 0x"+
		    QString::number(out_command_code, 16)+", addr "+QString::number(addr)+")", LogLevel::Error);
		// TODO: resend? report as error?
		// currently: error event will be called on timeout

	} else {
		log("GET: unknown error (code "+QString::number(code)+", out command code: 0x"+
		    QString::number(out_command_code, 16)+", addr "+QString::number(addr)+")", LogLevel::Error);
	}
}

void MtbUsb::handleMtbBusError(uint8_t errorCode, uint8_t addr) {
	MtbBusRecvError error = static_cast<MtbBusRecvError>(errorCode);

	for (size_t i = 0; i < m_hist.size(); i++) {
		if (is<CmdMtbUsbForward>(*m_hist[i].cmd)) {
			const CmdMtbUsbForward &forward = dynamic_cast<const CmdMtbUsbForward&>(*m_hist[i].cmd);
			if (addr == forward.module) {
				log("GET: error: "+mtbBusRecvErrorToStr(error)+", module: "+QString::number(addr)+
				    ", command: "+forward.msg(), LogLevel::Error);
				histTimeoutError(static_cast<CmdError>(errorCode), i);
				return;
			}
		}
	}

	log("GET: error: "+mtbBusRecvErrorToStr(error)+", module: "+QString::number(addr)+
		", unable to pair with outgoing command", LogLevel::Error);
}

void MtbUsb::histTimeoutError(CmdError cmdError, size_t i) {
	auto it = m_hist.begin();
	for (size_t j = 0; j < i; j++) {
		++it;
		if (it == m_hist.end())
			return;
	}

	assert(m_hist[i].cmd != nullptr);
	std::unique_ptr<const Cmd> cmd = std::move(m_hist[i].cmd);
	m_hist.erase(it);
	cmd->callError(cmdError);

	if (!m_out.empty())
		this->sendNextOut();
}

} // namespace Mtb
