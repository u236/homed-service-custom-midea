#include "devices/nobby.h"
#include "controller.h"
#include "logger.h"

Controller::Controller(const QString &configFile) : HOMEd(configFile)
{
    logInfo << "Starting version" << SERVICE_VERSION;
    logInfo << "Configuration file is" << getConfig()->fileName();

    m_device = new NobbyBalance(getConfig()->value("device/port", "/dev/ttyUSB0").toString(), getConfig()->value("device/name", "nobby").toString(), getConfig()->value("device/debug", false).toBool(), this);

    connect(m_device, &Device::availabilityUpdated, this, &Controller::availabilityUpdated);
    connect(m_device, &Device::propertiesUpdated, this, &Controller::propertiesUpdated);

    m_device->init();
}

void Controller::quit(void)
{
    mqttPublish(mqttTopic("device/custom/%1").arg(m_device->name()), {{"status", "offline"}}, true);
}

void Controller::mqttConnected(void)
{
    mqttSubscribe(mqttTopic("service/custom"));
    mqttSubscribe(mqttTopic("td/custom/%1").arg(m_device->name()));
    mqttPublishStatus();
}

void Controller::mqttReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QString subTopic = topic.name().replace(mqttTopic(), QString());
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
        QJsonObject options, data;

        mqttUnsubscribe(mqttTopic("service/custom"));
        mqttUnsubscribe(mqttTopic("status/custom"));

        for (auto it = devices.begin(); it != devices.end(); it++)
            if (it->toObject().value(json.value("names").toBool() ? "name" : "id") == m_device->name())
                return;

        data.insert("name", m_device->name());
        data.insert("id", m_device->name());
        data.insert("real", true);
        data.insert("active", true);
        data.insert("cloud", false);
        data.insert("discovery", false);
        data.insert("exposes", QJsonArray::fromStringList(m_device->exposes()));
        data.insert("options", QJsonObject::fromVariantMap(m_device->options()));

        mqttPublish(mqttTopic("command/custom"), QJsonObject {{"action", "updateDevice"}, {"data", data}});
    }
    else if (topic.name() == mqttTopic("td/custom/%1").arg(m_device->name()))
    {
        for (auto it = json.begin(); it != json.end(); it++)
            m_device->action(it.key(), it.value().toVariant());
    }
}

void Controller::availabilityUpdated(Availability availability)
{
    QString status = availability == Availability::Online ? "online" : "offline";
    mqttPublish(mqttTopic("device/custom/%1").arg(m_device->name()), {{"status", status}}, true);
    logInfo << "Device is" << status;
}

void Controller::propertiesUpdated(const QMap<QString, QVariant> &properties)
{
    mqttPublish(mqttTopic("fd/custom/%1").arg(m_device->name()), QJsonObject::fromVariantMap(properties));
}
