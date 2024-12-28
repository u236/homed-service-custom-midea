#include <QtEndian>
#include <QCryptographicHash>
#include "controller.h"
#include "logger.h"

Controller::Controller(const QString &configFile) : HOMEd(configFile), m_device(new Device(getConfig(), this))
{
    logInfo << "Starting version" << SERVICE_VERSION;
    logInfo << "Configuration file is" << getConfig()->fileName();

    m_topic = getConfig()->value("device/topic", "nobby").toString();

    connect(m_device, &Device::availabilityUpdated, this, &Controller::availabilityUpdated);
    connect(m_device, &Device::propertiesUpdated, this, &Controller::propertiesUpdated);

    m_device->init();
}

void Controller::quit(void)
{
    mqttPublish(mqttTopic("device/custom/%1").arg(m_topic), {{"status", "offline"}}, true);
}

void Controller::mqttConnected(void)
{
    mqttSubscribe(mqttTopic("td/custom/%1").arg(m_topic));
    mqttPublishStatus();
}

void Controller::mqttReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QJsonObject json = QJsonDocument::fromJson(message).object();

    if (topic.name() != mqttTopic("td/custom/%1").arg(m_topic))
        return;

    for (auto it = json.begin(); it != json.end(); it++)
        m_device->action(it.key(), it.value().toVariant());
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
