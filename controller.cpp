#include "devices/nobby.h"
#include "controller.h"
#include "logger.h"

Controller::Controller(const QString &configFile) : HOMEd(SERVICE_VERSION, configFile), m_names(false)
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

            if (port.isEmpty())
                continue;

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
        mqttPublish(mqttTopic("device/custom/%1").arg(m_names ? m_devices.at(i)->name() : m_devices.at(i)->id()), {{"status", "offline"}}, true);

    HOMEd::quit();
}

void Controller::mqttConnected(void)
{
    mqttSubscribe(mqttTopic("service/custom"));
    mqttPublishService();
}

void Controller::mqttReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QString subTopic = topic.name().replace(0, mqttTopic().length(), QString());
    QJsonObject json = QJsonDocument::fromJson(message).object();

    if (subTopic == "service/custom")
    {
        if (json.value("status").toString() != "online")
        {
            for (int i = 0; i < m_devices.count(); i++)
                mqttUnsubscribe(mqttTopic("td/custom/%1").arg(m_names ? m_devices.at(i)->name() : m_devices.at(i)->id()));

            return;
        }

        mqttSubscribe(mqttTopic("status/custom"));
    }
    else if (subTopic == "status/custom")
    {
        QJsonArray devices = json.value("devices").toArray();

        m_names = json.value("names").toBool();

        for (int i = 0; i < m_devices.count(); i++)
        {
            const Device &device = m_devices.at(i);
            bool check = true;

            for (auto it = devices.begin(); it != devices.end(); it++)
            {
                QJsonObject item = it->toObject();
                QString name = item.value("name").toString();

                if (item.value("id").toString() != device->id())
                    continue;

                if (name.isEmpty())
                    name = device->id();

                if (m_names && name != device->name())
                {
                    mqttUnsubscribe(mqttTopic("td/custom/%1").arg(device->name()));
                    device->setName(name);
                }

                check = false;
                break;
            }

            mqttSubscribe(mqttTopic("td/custom/%1").arg(m_names ? device->name() : device->id()));

            if (!check)
                device->setPublished();

            if (!device->published())
            {
                mqttPublish(mqttTopic("command/custom"), QJsonObject {{"action", "updateDevice"}, {"data", QJsonObject {{"real", true}, {"active", true}, {"cloud", false}, {"discovery", false}, {"id", device->id()}, {"service", QCoreApplication::applicationName()}, {"exposes", device->exposes()}, {"options", device->options()}}}});
                device->setPublished();
            }
        }
    }
    else if (subTopic.startsWith("td/custom/"))
    {
        QString string = subTopic.split('/').last();

        for (int i = 0; i < m_devices.count(); i++)
        {
            const Device &device = m_devices.at(i);

            if ((m_names ? device->name() : device->id()) != string)
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
    mqttPublish(mqttTopic("device/custom/%1").arg(m_names ? device->name() : device->id()), {{"status", status}}, true);
    logInfo << device << "is" << status;
}

void Controller::propertiesUpdated(const QMap <QString, QVariant> &properties)
{
    DeviceObject *device = reinterpret_cast <DeviceObject*> (sender());
    mqttPublish(mqttTopic("fd/custom/%1").arg(m_names ? device->name() : device->id()), QJsonObject::fromVariantMap(properties));
}
