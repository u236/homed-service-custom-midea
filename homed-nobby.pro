include(../homed-common/homed-common.pri)

HEADERS += \
    controller.h \
    device.h

SOURCES += \
    controller.cpp \
    device.cpp

QT += serialport
