TARGET = mtb-daemon
TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle

SOURCES += \
	src/main.cpp \
	src/mtbusb.cpp \
	src/mtbusb-send.cpp \
	src/mtbusb-receive.cpp \
	src/mtbusb-hist.cpp \
	src/mtbusb-common.cpp

HEADERS += \
	src/mtbusb.h \
	src/mtbusb-commands.h
	src/mtbusb-common.h

CONFIG += c++14
QMAKE_CXXFLAGS += -Wall -Wextra -pedantic

win32 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
}
win64 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
}

QT -= gui
QT += core serialport

VERSION_MAJOR = 0
VERSION_MINOR = 1

DEFINES += "VERSION_MAJOR=$$VERSION_MAJOR" "VERSION_MINOR=$$VERSION_MINOR"

#Target version
VERSION = $${VERSION_MAJOR}.$${VERSION_MINOR}
DEFINES += "VERSION=\\\"$${VERSION}\\\""

QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -O0
