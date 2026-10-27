QT += core gui qml quick quickcontrols2 serialport quickdialogs2 sql network
CONFIG += c++17
TEMPLATE = app
TARGET = SLF-EWN

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/core/Crc8.cpp \
    src/core/PacketParser.cpp \
    src/model/Database.cpp \
    src/model/SerialPortReader.cpp \
    src/model/SimulatedTelemetrySource.cpp \
    src/model/NodeListModel.cpp \
    src/model/AlertListModel.cpp \
    src/controller/NodeManager.cpp \
    src/controller/AuthController.cpp \
    src/controller/AppController.cpp

HEADERS += \
    src/core/Types.h \
    src/core/Crc8.h \
    src/core/PacketParser.h \
    src/model/Database.h \
    src/model/SerialPortReader.h \
    src/model/SimulatedTelemetrySource.h \
    src/model/NodeListModel.h \
    src/model/AlertListModel.h \
    src/controller/NodeManager.h \
    src/controller/AuthController.h \
    src/controller/AppController.h

RESOURCES += qml.qrc
