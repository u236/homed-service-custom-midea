#include <QtEndian>
#include <QCryptographicHash>
#include "controller.h"
#include "logger.h"

Controller::Controller(const QString &configFile) : HOMEd(configFile, false, false), m_device(new Device(getConfig(), this))
{
    logInfo << "Starting version" << SERVICE_VERSION;
    logInfo << "Configuration file is" << getConfig()->fileName();

    m_topic = getConfig()->value("device/topic", "nobby").toString();

    connect(m_device, &Device::availabilityUpdated, this, &Controller::availabilityUpdated);
    connect(m_device, &Device::propertiesUpdated, this, &Controller::propertiesUpdated);

    mqttSetWillTopic(mqttTopic("device/custom/%1").arg(m_topic));
    mqttConnect();

    m_device->init();
}

void Controller::quit(void)
{
    mqttPublish(mqttTopic("device/custom/%1").arg(m_topic), {{"status", "offline"}}, true);
}

void Controller::mqttConnected(void)
{
    mqttSubscribe(mqttTopic("service/custom"));
    mqttSubscribe(mqttTopic("td/custom/%1").arg(m_topic));
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
            if (it->toObject().value(json.value("names").toBool() ? "name" : "id") == m_topic)
                return;

        options.insert("heater", QJsonObject {{"type", "toggle"}});
        options.insert("waterTemperature", QJsonObject {{"type", "sensor"}, {"unit", "°C"}});
        options.insert("waterTargetTemperature", QJsonObject {{"type", "number"}, {"min", 35}, {"max", 60}, {"unit", "°C"}});
        options.insert("heaterTemperature", QJsonObject {{"type", "sensor"}, {"unit", "°C"}});
        options.insert("heaterTargetTemperature", QJsonObject {{"type", "number"}, {"min", 30}, {"max", 80}, {"unit", "°C"}});
        options.insert("pressure", QJsonObject {{"type", "sensor"}, {"unit", "bar"}});

        data.insert("name", m_topic);
        data.insert("id", m_topic);
        data.insert("real", true);
        data.insert("active", true);
        data.insert("cloud", false);
        data.insert("discovery", false);
        data.insert("exposes", QJsonArray {"switch", "heater", "flame", "mode", "waterTemperature", "waterTargetTemperature", "heaterTemperature", "heaterTargetTemperature", "pressure", "errorCode"});
        data.insert("options", options);

        mqttPublish(mqttTopic("command/custom"), QJsonObject {{"action", "updateDevice"}, {"data", data}});
    }
    else if (topic.name() == mqttTopic("td/custom/%1").arg(m_topic))
    {
        for (auto it = json.begin(); it != json.end(); it++)
            m_device->action(it.key(), it.value().toVariant());
    }
}

void Controller::availabilityUpdated(Availability availability)
{
    QString status = availability == Availability::Online ? "online" : "offline";
    mqttPublish(mqttTopic("device/custom/%1").arg(m_topic), {{"status", status}}, true);
    logInfo << "Device is" << status;
}

void Controller::propertiesUpdated(const QMap<QString, QVariant> &properties)
{
    mqttPublish(mqttTopic("fd/custom/%1").arg(m_topic), QJsonObject::fromVariantMap(properties));
}
