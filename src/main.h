#ifndef MAIN_H
#define MAIN_H

#include <QCoreApplication>
#include "mtbusb/mtbusb.h"

extern Mtb::MtbUsb mtbusb;

class DaemonCoreApplication : public QCoreApplication {
	Q_OBJECT
public:
	DaemonCoreApplication(int &argc, char **argv);
	virtual ~DaemonCoreApplication() {};

private:

private slots:
	void mtbUsbLog(QString message, Mtb::LogLevel loglevel);
};

#endif
