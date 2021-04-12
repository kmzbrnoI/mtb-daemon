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

	mtbusb.send(Mtb::CmdMtbUsbInfoRequest());
}

void DaemonCoreApplication::mtbUsbLog(QString message, Mtb::LogLevel loglevel) {
	(void)loglevel;
	std::cout << message.toStdString() << std::endl;
}

int main(int argc, char *argv[]) {
	DaemonCoreApplication a(argc, argv);
	return a.exec();
}
