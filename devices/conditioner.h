#ifndef CONDITIONER_H
#define CONDITIONER_H

#define CAPABILITY_REQUEST_RETRIES      3
#define UNKNOWN_TEMPERATURE             0xFF

#include "device.h"

class Conditioner : public DeviceObject
{

public:

    Conditioner(const QString &port, const QString &id, bool debug);
    void action(const QString &name, const QVariant &data) override;

private:

    enum class Capability
    {
        fanMode     = 0x0210,
        systemMode  = 0x0214,
        swingMode   = 0x0215,
        turboMode   = 0x021A,
        temperature = 0x0225
    };

    enum class Feature
    {
        modeAuto,
        modeCool,
        modeDry,
        modeHeat,
        fanMin,
        fanLow,
        fanMedium,
        fanHigh,
        fanMax,
        fanAuto,
        vertical,
        horizontal
    };

    QByteArray m_status;
    double m_min, m_max;
    quint8 m_retry, m_sequence;
    QList <Feature> m_features;

    double temperature(quint8 integer, quint8 decimal, bool check);

    void requestCapabilities(bool next = false);
    void updateExposes(void);

    void parseCapabilities(const QByteArray &payload);
    void parseStatus(void);

    void parseFrame(const QByteArray &payload) override;
    void ping(void) override;

};

#endif
