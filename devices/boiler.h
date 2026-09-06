#ifndef BOILER_H
#define BOILER_H

#include "device.h"

class Boiler : public DeviceObject
{

public:

    Boiler(const QString &port, const QString &id, bool debug);
    void action(const QString &name, const QVariant &data) override;

private:

    void parseFrame(const QByteArray &payload) override;
    void ping(void) override;

};

#endif
