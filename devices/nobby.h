#ifndef NOBBY_H
#define NOBBY_H

#include "device.h"

class NobbyBalance : public Device
{

public:

    NobbyBalance(QSettings *config, QObject *parent);
    void action(const QString &name, const QVariant &data) override;

private:

    void parseFrame(quint8 type, const QByteArray &payload) override;
    void ping(void) override;

};

#endif
