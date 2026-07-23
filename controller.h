#ifndef CONTROLLER_H
#define CONTROLLER_H

#define SERVICE_VERSION     "1.0.6"

#include "device.h"
#include "homed.h"

class Controller : public HOMEd
{
    Q_OBJECT

public:

    Controller(const QString &configFile);

private:

    bool m_status, m_names;
    QList <Device> m_devices;

    void publishAvailability(DeviceObject *device);

public slots:

    void quit(void) override;

private slots:

    void mqttConnected(void) override;
    void mqttReceived(const QByteArray &message, const QMqttTopicName &topic) override;

    void availabilityUpdated(void);
    void propertiesUpdated(void);

};

#endif
