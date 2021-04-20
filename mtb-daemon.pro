TARGET = mtb-daemon
TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle

SOURCES += \
	src/main.cpp \
	src/mtbusb/mtbusb.cpp \
	src/mtbusb/mtbusb-send.cpp \
	src/mtbusb/mtbusb-receive.cpp \
	src/mtbusb/mtbusb-hist.cpp \
	src/mtbusb/mtbusb-common.cpp \
	src/server.cpp \
	src/modules/module.cpp \
	src/modules/uni.cpp

HEADERS += \
	src/main.h \
	src/mtbusb/mtbusb.h \
	src/mtbusb/mtbusb-commands.h \
	src/mtbusb/mtbusb-common.h \
	src/server.h \
	src/modules/module.h \
	src/modules/uni.h \
	src/errors.h

CONFIG += c++17
QMAKE_CXXFLAGS += -Wall -Wextra -pedantic -std=c++17

win32 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
}
win64 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
}

QT -= gui
QT += core serialport network

VERSION_MAJOR = 0
VERSION_MINOR = 1

DEFINES += "VERSION_MAJOR=$$VERSION_MAJOR" "VERSION_MINOR=$$VERSION_MINOR"

#Target version
VERSION = $${VERSION_MAJOR}.$${VERSION_MINOR}
DEFINES += "VERSION=\\\"$${VERSION}\\\""
