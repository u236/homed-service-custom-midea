include(../homed-common/homed-common.pri)

HEADERS += \
    controller.h \
    device.h \
    devices/boiler.h \
    devices/conditioner.h

SOURCES += \
    controller.cpp \
    device.cpp \
    devices/boiler.cpp \
    devices/conditioner.cpp

QT += serialport
