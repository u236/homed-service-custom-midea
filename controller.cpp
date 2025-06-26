#include "devices/nobby.h"
#include "controller.h"
#include "logger.h"

Controller::Controller(const QString &configFile) : HOMEd(SERVICE_VERSION, configFile)
{
    QList <QString> names = getConfig()->childGroups(), types = {"nobbyBalance"};

    for (int i = 0; i < names.count(); i++)
    {
        const QString &name = names.at(i);

        if (name != "log" && name != "mqtt")
        {
            QString port = getConfig()->value(QString("%1/port").arg(name), "/dev/ttyUSB0").toString();
            bool debug = getConfig()->value(QString("%1/debug").arg(name), false).toBool();
            Device device;

            switch (types.indexOf(getConfig()->value(QString("%1/type").arg(name)).toString()))
            {
                case 0:  device = Device(new NobbyBalance(port, name, debug)); break;
                default: continue;
            }

            connect(device.data(), &DeviceObject::availabilityUpdated, this, &Controller::availabilityUpdated);
            connect(device.data(), &DeviceObject::propertiesUpdated, this, &Controller::propertiesUpdated);

            m_devices.append(device);
            device->init();
        }
    }
}

void Controller::quit(void)
{
    for (int i = 0; i < m_devices.count(); i++)
        mqttPublish(mqttTopic("device/custom/%1").arg(m_devices.at(i)->name()), {{"status", "offline"}}, true);
}

void Controller::mqttConnected(void)
{
    mqttSubscribe(mqttTopic("service/custom"));

    for (int i = 0; i < m_devices.count(); i++)
        mqttSubscribe(mqttTopic("td/custom/%1").arg(m_devices.at(i)->name()));

    mqttPublishStatus();
}

void Controller::mqttReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QString subTopic = topic.name().replace(0, mqttTopic().length(), QString());
    QJsonObject json = QJsonDocument::fromJson(message).object();

    if (subTopic == "service/custom")
    {
        if (json.value("status").toString() != "online")
            return;

        mqttSubscribe(mqttTopic("status/custom"));
    }
    else if (subTopic == "status/custom")
    {
        QJsonArray devices = json.value("devices").toArray();

        for (int i = 0; i < m_devices.count(); i++)
        {
            const Device &device = m_devices.at(i);

            bool check = true;

            for (auto it = devices.begin(); it != devices.end(); it++)
            {
                if (it->toObject().value(json.value("names").toBool() ? "name" : "id") == device->name())
                {
                    check = false;
                    break;
                }
            }

            if (!check)
                continue;

            mqttPublish(mqttTopic("command/custom"), QJsonObject {{"action", "updateDevice"}, {"data", QJsonObject {{"real", true}, {"active", true}, {"cloud", false}, {"discovery", false}, {"name", device->name()}, {"id", device->name()}, {"exposes", device->exposes()}, {"options", device->options()}}}});
        }

        mqttUnsubscribe(mqttTopic("service/custom"));
        mqttUnsubscribe(mqttTopic("status/custom"));
    }
    else if (subTopic.startsWith("td/custom/"))
    {
        QString name = subTopic.split('/').last();

        for (int i = 0; i < m_devices.count(); i++)
        {
            const Device &device = m_devices.at(i);

            if (device->name() != name)
                continue;

            for (auto it = json.begin(); it != json.end(); it++)
                device->action(it.key(), it.value().toVariant());

            break;
        }
    }
}

void Controller::availabilityUpdated(Availability availability)
{
    DeviceObject *device = reinterpret_cast <DeviceObject*> (sender());
    QString status = availability == Availability::Online ? "online" : "offline";
    mqttPublish(mqttTopic("device/custom/%1").arg(device->name()), {{"status", status}}, true);
    logInfo << device << "is" << status;
}

void Controller::propertiesUpdated(const QMap <QString, QVariant> &properties)
{
    mqttPublish(mqttTopic("fd/custom/%1").arg(reinterpret_cast <DeviceObject*> (sender())->name()), QJsonObject::fromVariantMap(properties));
}
