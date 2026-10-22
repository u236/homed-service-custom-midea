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
    QJsonObject data = device->data().isEmpty() ? QJsonObject {{"active", true}, {"cloud", false}, {"discovery", false}} : device->data();

    data.insert("id", device->id());
    data.insert("service", QCoreApplication::applicationName());
    data.insert("exposes", device->exposes());
    data.insert("options", device->options());
    data.insert("real", true);

    mqttPublish(mqttTopic("command/%1").arg(m_topic), {{"action", "updateDevice"}, {"device", device->id()}, {"data", data}});
    device->setPublished(true);
}

void Controller::publishAvailability(DeviceObject *device)
{
    mqttPublish(mqttTopic("device/%1/%2").arg(m_topic, m_names ? device->name() : device->id()), {{"status", device->availability() == Availability::Online ? "online" : "offline"}}, true);
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
            {
                mqttUnsubscribe(mqttTopic("td/%1/%2").arg(m_topic, m_names ? m_devices.at(i)->name() : m_devices.at(i)->id()));
                m_devices.at(i)->setPublished(false);
            }

            return;
        }

        mqttSubscribe(mqttTopic("status/%1").arg(m_topic));
    }
    else if (subTopic == QString("status/%1").arg(m_topic))
    {
        QList <QString> list = {"id", "service", "exposes", "options", "real"};
        QJsonArray devices = json.value("devices").toArray();

        m_status = true;
        m_names = json.value("names").toBool();

        for (int i = 0; i < m_devices.count(); i++)
        {
            const Device &device = m_devices.at(i);

            for (auto it = devices.begin(); it != devices.end(); it++)
            {
                QJsonObject item = it->toObject();

                if (item.value("id").toString() != device->id())
                    continue;

                for (int j = 0; j < list.count(); j++)
                    item.remove(list.at(j));

                if (m_names && item.value("name").toString(device->id()) != device->name())
                {
                    mqttPublish(mqttTopic("device/%1/%2").arg(m_topic, device->name()), QJsonObject(), true);
                    mqttUnsubscribe(mqttTopic("td/%1/%2").arg(m_topic, device->name()));
                }

                device->setData(item);
                break;
            }

            mqttSubscribe(mqttTopic("td/%1/%2").arg(m_topic, m_names ? device->name() : device->id()));

            if (device->ready() && !device->published())
                publishDevice(device.data());

            publishAvailability(device.data());
        }

        for (auto it = devices.begin(); it != devices.end(); it++)
        {
            QJsonObject item = it->toObject();
            QString id = item.value("id").toString();
            bool check = true;

            if (item.value("service").toString() != QCoreApplication::applicationName())
                continue;

            for (int i = 0; i < m_devices.count(); i++)
            {
                if (m_devices.at(i)->id() != id)
                    continue;

                check = false;
                break;
            }

            if (!check)
                continue;

            mqttPublish(mqttTopic("command/%1").arg(m_topic), QJsonObject {{"action", "removeDevice"}, {"device", id}});
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
    DeviceObject *device = reinterpret_cast <DeviceObject*> (sender());

    logInfo << device << "is" << (device->availability() == Availability::Online ? "online" : "offline");

    if (!m_status)
        return;

    publishAvailability(device);
}

void Controller::propertiesUpdated(void)
{
    DeviceObject *device = reinterpret_cast <DeviceObject*> (sender());
    mqttPublish(mqttTopic("fd/%1/%2").arg(m_topic, m_names ? device->name() : device->id()), QJsonObject::fromVariantMap(device->properties()));
}
