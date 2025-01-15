#include "nobby.h"

NobbyBalance::NobbyBalance(const QString &port, const QString &name, bool debug, QObject *parent) : Device(port, name, debug, parent)
{
    m_actions = {"status", "heater", "heaterTargetTemperature", "waterTargetTemperature"};
    m_exposes = {"switch", "heater", "flame", "mode", "waterTemperature", "waterTargetTemperature", "heaterTemperature", "heaterTargetTemperature", "pressure", "errorCode"};

    m_options.insert("heater", QJsonObject {{"type", "toggle"}});
    m_options.insert("waterTemperature", QJsonObject {{"type", "sensor"}, {"unit", "°C"}});
    m_options.insert("waterTargetTemperature", QJsonObject {{"type", "number"}, {"min", 35}, {"max", 60}, {"unit", "°C"}});
    m_options.insert("heaterTemperature", QJsonObject {{"type", "sensor"}, {"unit", "°C"}});
    m_options.insert("heaterTargetTemperature", QJsonObject {{"type", "number"}, {"min", 30}, {"max", 80}, {"unit", "°C"}});
    m_options.insert("pressure", QJsonObject {{"type", "sensor"}, {"unit", "bar"}});
}

void NobbyBalance::action(const QString &name, const QVariant &value)
{
    quint8 data[30];
    QByteArray payload;

    memset(data, 0, sizeof(data));

    switch (m_actions.indexOf(name))
    {
        case 0: // status
        {
            QList <QString> list = {"toggle", "on", "off"};
            qint8 command = list.indexOf(value.toString());

            if (command < 0)
                return;

            data[0] = command ? command : m_properties.value("status").toString() != "on" ? 0x01 : 0x02;
            data[1] = 0x01;
            break;
        }

        case 1: // heater
            data[0] = 0x04;
            data[1] = 0x01;
            data[2] = value.toBool() ? 0x01 : 0x02;
            break;

        case 2: // heaterTargetTemperature
            data[0] = 0x04;
            data[1] = 0x13;
            data[2] = static_cast <quint8> (value.toInt());
            break;

        case 3: // waterTargetTemperature
            data[0] = 0x04;
            data[1] = 0x12;
            data[2] = static_cast <quint8> (value.toInt());
            break;

        default:
            return;
    }

    payload = QByteArray(reinterpret_cast <char*> (data), sizeof(data));
    sendFrame(FRAME_SET, payload.append(static_cast <char> (crc(payload))));
}

void NobbyBalance::parseFrame(quint8 type, const QByteArray &payload)
{
    switch (type)
    {
        case FRAME_SET:
        case FRAME_GET:
        case FRAME_NOTIFY:
        {
            QMap <QString, QVariant> properties;

            if (payload.length() != STATUS_PAYLOAD_LENGTH)
                return;

            properties.insert("status", payload.at(2) & 0x04 ? "on" : "off");
            properties.insert("heater", payload.at(4) & 0x01 ? true : false);
            properties.insert("flame", payload.at(2) & 0x08 ? true : false);

            switch (payload.at(2) >> 4 & 0x03)
            {
                case 0x00: properties.insert("mode", "idle"); break;
                case 0x01: properties.insert("mode", "heater"); break;
                case 0x02: properties.insert("mode", "water"); break;
            }

            properties.insert("waterTemperature", static_cast <quint8> (payload.at(8)));
            properties.insert("waterTargetTemperature", static_cast <quint8> (payload.at(12)));

            properties.insert("heaterTemperature", static_cast <quint8> (payload.at(14)));
            properties.insert("heaterTargetTemperature", static_cast <quint8> (payload.at(17)));

            properties.insert("pressure", static_cast <quint8> (payload.at(27)) / 10.0);
            properties.insert("errorCode", static_cast <quint8> (payload.at(6))); // not equals error codes on display

            if (m_properties != properties)
            {
                m_properties = properties;
                emit propertiesUpdated(properties);
            }

            break;
        }
    }
}

void NobbyBalance::ping(void)
{
    quint8 data[30];
    QByteArray payload;

    memset(data, 0, sizeof(data));
    data[0] = 0x01;
    data[1] = 0x01;

    payload = QByteArray(reinterpret_cast <char*> (data), sizeof(data));
    sendFrame(FRAME_GET, payload.append(static_cast <char> (crc(payload))));
}
