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
	src/mtbusb/mtbusb-win-com-discover.cpp \
	src/server.cpp \
	src/logging.cpp \
	src/qjsonsafe.cpp \
	src/modules/module.cpp \
	src/modules/uni.cpp \
	src/modules/unis.cpp \
	src/modules/rc.cpp

HEADERS += \
	src/main.h \
	src/mtbusb/mtbusb-win-com-discover.h \
	src/mtbusb/mtbusb.h \
	src/mtbusb/mtbusb-commands.h \
	src/mtbusb/mtbusb-common.h \
	src/server.h \
	src/logging.h \
	src/qjsonsafe.h \
	src/modules/module.h \
	src/modules/uni.h \
	src/modules/unis.h \
	src/modules/rc.h \
	src/errors.h \
	src/utils.h \
	lib/termcolor.h

INCLUDEPATH += \
	src \
	lib \
	src/mtbusb \
	src/modules

CONFIG += c++17
QMAKE_CXXFLAGS += -Wall -Wextra -pedantic -std=c++17

win32 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
	LIBS += -lsetupapi
}
win64 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
	LIBS += -lsetupapi
}

QT -= gui
QT += core serialport network

VERSION_MAJOR = 1
VERSION_MINOR = 8

DEFINES += "VERSION_MAJOR=$$VERSION_MAJOR" "VERSION_MINOR=$$VERSION_MINOR"

#Target version
VERSION = $${VERSION_MAJOR}.$${VERSION_MINOR}
DEFINES += "VERSION=\\\"$${VERSION}\\\""
