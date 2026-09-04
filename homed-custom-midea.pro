include(../homed-common/homed-common.pri)

HEADERS += \
    controller.h \
    device.h \
    devices/boiler.h

SOURCES += \
    controller.cpp \
    device.cpp \
    devices/boiler.cpp

QT += serialport
