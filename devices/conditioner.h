#ifndef CONDITIONER_H
#define CONDITIONER_H

#define CAPABILITIES_ATTEMPTS       3

#define CAPABILITIES_ITEM_FIRST     0x11
#define CAPABILITIES_ITEM_NEXT      0x01

#define CAPABILITIES_FAN_SPEED      0x0210
#define CAPABILITIES_MODES          0x0214
#define CAPABILITIES_SWING_MODES    0x0215
#define CAPABILITIES_PRESET_TURBO   0x021A
#define CAPABILITIES_TEMPERATURES   0x0225

#define BODY_CAPABILITIES           0xB5
#define BODY_STATUS                 0xC0

#define TEMPERATURE_UNKNOWN         0xFF

#include "device.h"

class Conditioner : public DeviceObject
{

public:

    Conditioner(const QString &port, const QString &id, bool debug);

    void action(const QString &name, const QVariant &data) override;
    void parseFrame(quint8 type, const QByteArray &payload) override;
    void ping(void) override;

private:

    bool m_autoMode, m_coolMode, m_dryMode, m_heatMode;
    bool m_fanMin, m_fanLow, m_fanMedium, m_fanHigh, m_fanMax, m_fanAuto;
    bool m_swingVertical, m_swingHorizontal;

    double m_minTemperature, m_maxTemperature;
    quint8 m_attempts, m_sequence;

    QByteArray m_status;

    double temperature(quint8 integer, quint8 decimal, bool fahrenheit);

    void capabilitiesRequest(quint8 item);
    void updateExposes(void);

    void parseCapabilities(const QByteArray &payload);
    void parseStatus(const QByteArray &payload);

};

#endif
