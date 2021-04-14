#include <QSerialPort>
#include <iostream>
#include "main.h"
#include "mtbusb/mtbusb-common.h"

DaemonCoreApplication::DaemonCoreApplication(int &argc, char **argv)
     : QCoreApplication(argc, argv) {
	if (argc < 2)
		throw std::logic_error("No port specified!");

	QObject::connect(&mtbusb, SIGNAL(onLog(QString, Mtb::LogLevel)),
	                 this, SLOT(mtbUsbLog(QString, Mtb::LogLevel)));

	mtbusb.loglevel = Mtb::LogLevel::Debug;
	mtbusb.connect(argv[1], 115200, QSerialPort::FlowControl::NoFlowControl);

	mtbusb.send(
		Mtb::CmdMtbModuleInfoRequest(
			1,
			{[](uint8_t module, Mtb::ModuleInfo, void*) {
				std::cout << "Got module " << module << " info" << std::endl;
			}},
			{[](Mtb::CmdError cmdError, void*) {
				std::cout << "Error callback: "+Mtb::cmdErrorToStr(cmdError).toStdString()+"!" << std::endl;
			}}
		)
	);

	mtbusb.send(
		Mtb::CmdMtbModuleChangeSpeed(
			1, Mtb::MtbBusSpeed::br115200,
			{[](void*) { std::cout << "Speed changed" << std::endl; }},
			{[](Mtb::CmdError cmdError, void*) {
				std::cout << "Error callback: "+Mtb::cmdErrorToStr(cmdError).toStdString()+"!" << std::endl;
			}}
		)
	);
}

void DaemonCoreApplication::mtbUsbLog(QString message, Mtb::LogLevel loglevel) {
	(void)loglevel;
	std::cout << "[" << QTime::currentTime().toString("hh:mm:ss,zzz").toStdString()
	          << "] " << message.toStdString() << std::endl;
}

int main(int argc, char *argv[]) {
	DaemonCoreApplication a(argc, argv);
	return a.exec();
}
