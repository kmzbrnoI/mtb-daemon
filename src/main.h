#ifndef MAIN_H
#define MAIN_H

#include <QCoreApplication>
#include "mtbusb/mtbusb.h"

class DaemonCoreApplication : public QCoreApplication {
	Q_OBJECT
public:
	Mtb::MtbUsb mtbusb;

	DaemonCoreApplication(int &argc, char **argv);
	virtual ~DaemonCoreApplication() {};

private:

private slots:
	void mtbUsbLog(QString message, Mtb::LogLevel loglevel);
};

#endif
