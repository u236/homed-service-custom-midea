#ifndef NOBBY_H
#define NOBBY_H

#include "device.h"

class NobbyBalance : public DeviceObject
{

public:

    NobbyBalance(const QString &port, const QString &name, bool debug);
    void action(const QString &name, const QVariant &data) override;

private:

    void parseFrame(quint8 type, const QByteArray &payload) override;
    void ping(void) override;

};

#endif
