include(../homed-common/homed-common.pri)

HEADERS += \
    controller.h \
    device.h \
    devices/nobby.h

SOURCES += \
    controller.cpp \
    device.cpp \
    devices/nobby.cpp

QT += serialport
