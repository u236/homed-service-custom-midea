#include "devices/boiler.h"
#include "devices/conditioner.h"
#include "controller.h"
#include "logger.h"

Controller::Controller(const QString &configFile) : HOMEd(SERVICE_VERSION, configFile, true), m_status(false), m_names(false)
{
    QList <QString> names = getConfig()->childGroups(), types = {"ac", "conditioner", "boiler", "nobbyBalance"};
    QString instance = serviceTopic().split('/').value(1);

    m_topic = instance.isEmpty() ? "custom" : QString("custom/%1").arg(instance);

    for (int i = 0; i < names.count(); i++)
    {
        const QString &name = names.at(i);

        if (name != "log" && name != "mqtt")
        {
            QString port = getConfig()->value(QString("%1/port").arg(name), "/dev/ttyUSB0").toString(), type = getConfig()->value(QString("%1/type").arg(name)).toString();
            bool debug = getConfig()->value(QString("%1/debug").arg(name), false).toBool();
            Device device;

            if (port.isEmpty())
                continue;

            switch (types.indexOf(type))
            {
                case 0: // ac
                case 1: // conditioner
                    device = Device(new Conditioner(port, QString("midea-%1").arg(name), debug));
                    break;

                case 2: // boiler
                case 3: // nobbyBalance
                    device = Device(new Boiler(port, QString("midea-%1").arg(name), debug));
                    break;

                default:
                    logWarning << "Device type" << type << "unrecognized";
                    continue;
            }

            connect(device.data(), &DeviceObject::deviceUpdated, this, &Controller::deviceUpdated);
            connect(device.data(), &DeviceObject::availabilityUpdated, this, &Controller::availabilityUpdated);
            connect(device.data(), &DeviceObject::propertiesUpdated, this, &Controller::propertiesUpdated);

            m_devices.append(device);
            device->init();
        }
    }
}

void Controller::publishDevice(DeviceObject *device)
{
    mqttPublish(mqttTopic("command/%1").arg(m_topic), QJsonObject {{"action", "updateDevice"}, {"data", QJsonObject {{"real", true}, {"active", true}, {"cloud", false}, {"discovery", false}, {"id", device->id()}, {"service", QCoreApplication::applicationName()}, {"exposes", device->exposes()}, {"options", device->options()}}}});
    device->setPublished();
}

void Controller::publishAvailability(DeviceObject *device)
{
    QString status = device->availability() == Availability::Online ? "online" : "offline";
    mqttPublish(mqttTopic("device/%1/%2").arg(m_topic, m_names ? device->name() : device->id()), {{"status", status}}, true);
    logInfo << device << "is" << status;
}

void Controller::quit(void)
{
    for (int i = 0; i < m_devices.count(); i++)
        mqttPublish(mqttTopic("device/%1/%2").arg(m_topic, m_names ? m_devices.at(i)->name() : m_devices.at(i)->id()), {{"status", "offline"}}, true);

    HOMEd::quit();
}

void Controller::mqttConnected(void)
{
    mqttSubscribe(mqttTopic("service/%1").arg(m_topic));
    mqttPublishService();
}

void Controller::mqttReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QString subTopic = topic.name().replace(0, mqttTopic().length(), QString());
    QJsonObject json = QJsonDocument::fromJson(message).object();

    if (subTopic == QString("service/%1").arg(m_topic))
    {
        if (json.value("status").toString() != "online")
        {
            m_status = false;

            for (int i = 0; i < m_devices.count(); i++)
                mqttUnsubscribe(mqttTopic("td/%1/%2").arg(m_topic, m_names ? m_devices.at(i)->name() : m_devices.at(i)->id()));

            return;
        }

        mqttSubscribe(mqttTopic("status/%1").arg(m_topic));
    }
    else if (subTopic == QString("status/%1").arg(m_topic))
    {
        QJsonArray devices = json.value("devices").toArray();

        m_status = true;
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
                    mqttPublish(mqttTopic("device/%1/%2").arg(m_topic, device->name()), QJsonObject(), true);
                    mqttUnsubscribe(mqttTopic("td/%1/%2").arg(m_topic, device->name()));
                    device->setName(name);
                }

                check = false;
                break;
            }

            mqttSubscribe(mqttTopic("td/%1/%2").arg(m_topic, m_names ? device->name() : device->id()));

            if (!check)
                device->setPublished();

            if (device->ready() && !device->published())
                publishDevice(device.data());

            publishAvailability(device.data());
        }
    }
    else if (subTopic.startsWith(QString("td/%1/").arg(m_topic)))
    {
        QString string = subTopic.split('/').last();

        for (int i = 0; i < m_devices.count(); i++)
        {
            const Device &device = m_devices.at(i);

            if (!device->ready() || (m_names ? device->name() : device->id()) != string)
                continue;

            for (auto it = json.begin(); it != json.end(); it++)
                device->action(it.key(), it.value().toVariant());

            break;
        }
    }
}

void Controller::deviceUpdated(void)
{
    DeviceObject *device = reinterpret_cast <DeviceObject*> (sender());

    if (!m_status || device->published())
        return;

    publishDevice(device);
}

void Controller::availabilityUpdated(void)
{
    if (!m_status)
        return;

    publishAvailability(reinterpret_cast <DeviceObject*> (sender()));
}

void Controller::propertiesUpdated(void)
{
    DeviceObject *device = reinterpret_cast <DeviceObject*> (sender());
    mqttPublish(mqttTopic("fd/%1/%2").arg(m_topic, m_names ? device->name() : device->id()), QJsonObject::fromVariantMap(device->properties()));
}
