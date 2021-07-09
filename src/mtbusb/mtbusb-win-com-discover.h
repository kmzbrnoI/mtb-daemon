#ifndef MTBUSBWINCOMDISCOVER_H
#define MTBUSBWINCOMDISCOVER_H

#include <QtGlobal>

#ifdef Q_OS_WIN

#include <QSerialPortInfo>
#include <vector>
#include <windows.h>

std::vector<QSerialPortInfo> winMtbUsbPorts();

#endif

#endif // MTBUSBWINCOMDISCOVER_H
