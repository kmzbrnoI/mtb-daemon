#include <QSerialPort>
#include <iostream>
#include "main.h"

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
			{[](Mtb::ModuleInfo, void*) { std::cout << "Ok callback" << std::endl; }},
			{[](Mtb::CmdError cmdError, void*) {
				std::cout << "Error callback: "+Mtb::cmdErrorToStr(cmdError).toStdString()+"!" << std::endl;
			}}
		)
	);

	mtbusb.send(
		Mtb::CmdMtbModuleBeacon(
			1, false,
			{[](void*) { std::cout << "Beacon set" << std::endl; }},
			{[](Mtb::CmdError cmdError, void*) {
				std::cout << "Beacon error callback: "+Mtb::cmdErrorToStr(cmdError).toStdString()+"!" << std::endl;
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
