#include <QtEndian>
#include "conditioner.h"
#include "logger.h"

Conditioner::Conditioner(const QString &port, const QString &id, bool debug) : DeviceObject(0xAC, port, id, debug), m_min(17), m_max(30), m_retry(0), m_sequence(0)
{
    m_exposes = {"thermostat", "outdoorTemperature"};
    m_actions = {"systemMode", "targetTemperature", "fanMode", "swingMode"};
}

void Conditioner::action(const QString &name, const QVariant &data)
{
    quint8 buffer[24];
    QByteArray payload;

    if (m_status.isEmpty())
        return;

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, m_status.constData(), 11);

    buffer[0] = 0x40;

    switch (m_actions.indexOf(name))
    {
        case 0: // systemMode
        {
            QList <QString> list = {"off", "auto", "cool", "dry", "heat", "fan"};
            qint8 value = static_cast <qint8> (list.indexOf(data.toString()));

            if (value < 0)
                return;

            if (!value)
            {
                buffer[1] &= ~0x01;
                break;
            }

            buffer[1] |= 0x01;
            buffer[2] = (buffer[2] & ~0xE0) | value << 5;
            break;
        }

        case 1: // targetTemperature
        {
            quint8 value = static_cast <quint8> (data.toDouble() * 4) + 1, integer = value / 4;

            buffer[18] = (buffer[18] & ~0x1F) | ((integer - 12) & 0x1F);
            integer -= 16;

            if (integer < 1 || integer > 14)
                integer = 1;

            buffer[2] = (buffer[2] & ~0x1F) | (value & 0x02) << 3 | integer;
            break;
        }

        case 2: // fanMode
        {
            QMap <QString, quint8> map = {{"min", 20}, {"low", 40}, {"medium", 60}, {"high", 80}, {"max", 100}, {"auto", 102}};

            if (!map.contains(data.toString()))
                return;

            buffer[3] = map.value(data.toString());
            break;
        }

        case 3: // swingMode
        {
            QMap <QString, quint8> map = {{"off", 0x00}, {"horizontal", 0x03}, {"vertical", 0x0C}, {"both", 0x0F}};

            if (!map.contains(data.toString()))
                return;

            buffer[7] = 0x30 | map.value(data.toString());
            break;
        }

        default:
            return;
    }

    payload = QByteArray(reinterpret_cast <char*> (buffer), sizeof(buffer));
    sendFrame(FRAME_SET, payload.append(static_cast <char> (crc(payload))));
}

double Conditioner::temperature(quint8 integer, quint8 decimal, bool check)
{
    qint16 value = integer - 50;

    if (decimal && !check)
        return value / 2 + decimal * (value < 0 ? -0.1 : 0.1);

    if (decimal >= 5)
        return value / 2 + (value < 0 ? -0.5 : 0.5);

    return value * 0.5;
}

void Conditioner::requestCapabilities(bool next)
{
    quint8 data[3] = {0xB5, 0x01, static_cast <quint8> (next ? 0x01 : 0x11)};
    QByteArray payload = QByteArray(reinterpret_cast <char*> (data), sizeof(data));

    if (next)
        payload.append(1, 0x00);

    sendFrame(FRAME_GET, payload.append(static_cast <char> (crc(payload))));
}

void Conditioner::updateExposes(void)
{
    QList <QString> systemMode = {"off"}, swingMode = {"off"}, fanMode;

    for (int i = 0; i < m_features.count(); i++)
    {
        switch (m_features.at(i))
        {
            case Feature::modeAuto:   systemMode.append("auto"); break;
            case Feature::modeCool:   systemMode.append("cool"); break;
            case Feature::modeDry:    systemMode.append("dry"); break;
            case Feature::modeHeat:   systemMode.append("heat"); break;
            case Feature::fanMin:     fanMode.append("min"); break;
            case Feature::fanLow:     fanMode.append("low"); break;
            case Feature::fanMedium:  fanMode.append("medium"); break;
            case Feature::fanHigh:    fanMode.append("high"); break;
            case Feature::fanMax:     fanMode.append("max"); break;
            case Feature::fanAuto:    fanMode.append("auto"); break;
            case Feature::vertical:   swingMode.append("vertical"); break;
            case Feature::horizontal: swingMode.append("horizontal"); break;
        }
    }

    systemMode.append("fan");

    if (swingMode.count() > 2)
        swingMode.append("both");

    if (swingMode.count() > 1)
        m_options.insert("swingMode", QJsonObject {{"enum", QJsonArray::fromStringList(swingMode)}});

    if (fanMode.count() > 1)
        m_options.insert("fanMode", QJsonObject {{"enum", QJsonArray::fromStringList(fanMode)}});

    m_options.insert("systemMode",         QJsonObject {{"enum", QJsonArray::fromStringList(systemMode)}});
    m_options.insert("targetTemperature",  QJsonObject {{"min", m_min}, {"max", m_max}, {"step", 0.5}});
    m_options.insert("outdoorTemperature", QJsonObject {{"type", "sensor"}, {"unit", "°C"}, {"icon", "mdi:home-thermometer-outline"}});
}

void Conditioner::parseCapabilities(const QByteArray &payload)
{
    int offset = 2;

    for (quint8 i = 0; i < static_cast <quint8> (payload.at(1)) && payload.length() - offset >= 3; i++)
    {
        quint8 size = payload.at(offset + 2), value;

        if (!size || payload.length() - offset - 3 < size)
            break;

        value = payload.at(offset + 3);

        switch (static_cast <Capability> (qFromLittleEndian <quint16> (payload.constData() + offset)))
        {
            case Capability::fanMode:

                switch (value)
                {
                    case 0x01: m_features.append({Feature::fanMin, Feature::fanLow, Feature::fanMedium, Feature::fanHigh, Feature::fanAuto}); break;
                    case 0x03: m_features.append({Feature::fanLow, Feature::fanHigh}); break;
                    case 0x04: m_features.append({Feature::fanLow, Feature::fanHigh, Feature::fanAuto}); break;
                    case 0x05: m_features.append({Feature::fanLow, Feature::fanMedium, Feature::fanHigh, Feature::fanAuto}); break;
                    case 0x06: m_features.append({Feature::fanMin, Feature::fanLow, Feature::fanMedium, Feature::fanHigh, Feature::fanAuto}); break;
                    case 0x07: m_features.append({Feature::fanLow, Feature::fanMedium, Feature::fanHigh}); break;
                    case 0x09: m_features.append({Feature::fanMin, Feature::fanLow, Feature::fanHigh, Feature::fanAuto}); break;
                }

                break;

            case Capability::systemMode:

                switch (value)
                {
                    case 0x00: m_features.append({Feature::modeAuto, Feature::modeCool, Feature::modeDry}); break;
                    case 0x01: m_features.append({Feature::modeAuto, Feature::modeCool, Feature::modeDry, Feature::modeHeat}); break;
                    case 0x02: m_features.append({Feature::modeAuto, Feature::modeHeat}); break;
                    case 0x03: m_features.append({Feature::modeCool}); break;
                }

                break;

            case Capability::swingMode:

                switch (value)
                {
                    case 0x00: m_features.append({Feature::vertical}); break;
                    case 0x01: m_features.append({Feature::vertical, Feature::horizontal}); break;
                    case 0x03: m_features.append({Feature::horizontal}); break;
                }

                break;

            case Capability::turboMode:

                if (value != 2)
                    m_features.append(Feature::fanMax);

                break;

            case Capability::temperature:

                if (size < 6)
                    break;

                m_min = qMin(payload.at(offset + 3), qMin(payload.at(offset + 5), payload.at(offset + 7))) * 0.5;
                m_max = qMax(payload.at(offset + 4), qMax(payload.at(offset + 6), payload.at(offset + 8))) * 0.5;
                break;
        }

        offset += size + 3;
    }

    if (payload.length() - offset == 3 && payload.at(offset))
    {
        requestCapabilities(true);
        return;
    }

    std::sort(m_features.begin(), m_features.end());

    updateExposes();
    m_ready = true;

    emit deviceUpdated();
}

void Conditioner::parseStatus(void)
{
    QMap <QString, QVariant> properties;
    QList <QString> systemMode = {"off", "auto", "cool", "dry", "heat", "fan"};
    double value = (m_status.at(2) & 0x0F) + 16;
    bool check = m_status.at(10) & 0x04;

    if (m_status.at(13) & 0x1F)
        value = (m_status.at(13) & 0x1F) + 12;

    if (m_status.at(2) & 0x10)
        value += 0.5;

    properties.insert("systemMode", m_status.at(1) & 0x01 ? systemMode.value(m_status.at(2) >> 5 & 0x07) : "off");
    properties.insert("targetTemperature", value);

    if (static_cast <quint8> (m_status.at(11)) != UNKNOWN_TEMPERATURE)
        properties.insert("temperature", temperature(m_status.at(11), m_status.at(15) & 0x0F, check));

    if (static_cast <quint8> (m_status.at(12)) != UNKNOWN_TEMPERATURE)
        properties.insert("outdoorTemperature", temperature(m_status.at(12), m_status.at(15) >> 4 & 0x0F, check));

    switch (static_cast <quint8> (m_status.at(3)))
    {
        case 0 ... 20:
            properties.insert("fanMode", "min");
            break;

        case 21 ... 40:
            properties.insert("fanMode", "low");
            break;

        case 41 ... 60:
            properties.insert("fanMode", "medium");
            break;

        case 61 ... 80:
            properties.insert("fanMode", "high");
            break;

        case 81 ... 100:
            properties.insert("fanMode", "max");
            break;

        default:
            properties.insert("fanMode", "auto");
            break;
    }

    switch (m_status.at(7) & 0x0F)
    {
        case 0x00: properties.insert("swingMode", "off"); break;
        case 0x03: properties.insert("swingMode", "horizontal"); break;
        case 0x0C: properties.insert("swingMode", "vertical"); break;
        case 0x0F: properties.insert("swingMode", "both"); break;
    }

    if (m_properties == properties)
        return;

    m_properties = properties;
    emit propertiesUpdated();
}

void Conditioner::parseFrame(const QByteArray &payload)
{
    switch (static_cast <quint8> (payload.at(0)))
    {
        case 0xB5:

            if (payload.length() > 1)
                parseCapabilities(payload);

            break;

        case 0xC0:

            if (payload.length() > 20)
            {
                m_status = payload;
                parseStatus();
            }

            break;
    }
}

void Conditioner::ping(void)
{
    quint8 buffer[22] = {0x41, 0x81, 0x00, 0xFF, 0x03, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, m_sequence++};
    QByteArray payload;

    if (!m_ready)
    {
        if (m_retry < CAPABILITY_REQUEST_RETRIES)
        {
            m_retry++;
            m_features.clear();
            requestCapabilities();
            return;
        }

        logWarning << this << "capabilities request failed, using defaults";
        m_features = {Feature::modeAuto, Feature::modeCool, Feature::modeHeat, Feature::fanLow, Feature::fanMedium, Feature::fanHigh, Feature::fanAuto, Feature::vertical};

        updateExposes();
        m_ready = true;

        emit deviceUpdated();
        return;
    }

    payload = QByteArray(reinterpret_cast <char*> (buffer), sizeof(buffer));
    sendFrame(FRAME_GET, payload.append(static_cast <char> (crc(payload))));
}
