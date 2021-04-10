TARGET = mtb-daemon
TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle

SOURCES += \
	src/main.cpp
HEADERS +=

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
