#include "conditioner.h"
#include "logger.h"

Conditioner::Conditioner(const QString &port, const QString &id, bool debug) : DeviceObject(0xAC, port, id, debug), m_autoMode(true), m_coolMode(true), m_dryMode(true), m_heatMode(true), m_fanMin(true), m_fanLow(true), m_fanMedium(true), m_fanHigh(true), m_fanMax(true), m_fanAuto(true), m_swingVertical(true), m_swingHorizontal(false), m_minTemperature(17), m_maxTemperature(30), m_attempts(0), m_sequence(0)
{
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
            qint8 mode = static_cast <qint8> (list.indexOf(data.toString()));

            if (mode < 0)
                return;

            if (!mode)
            {
                buffer[1] &= ~0x01;
                break;
            }

            buffer[1] |= 0x01;
            buffer[2] = (buffer[2] & ~0xE0) | static_cast <quint8> (mode) << 5;
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
            QList <QString> list = {"min", "low", "medium", "high", "max", "auto"};
            QList <quint8> speed = {20, 40, 60, 80, 100, 102};
            qint8 index = static_cast <qint8> (list.indexOf(data.toString()));

            if (index < 0)
                return;

            buffer[3] = speed.at(index);
            break;
        }

        case 3: // swingMode
        {
            QList <QString> list = {"off", "horizontal", "vertical", "both"};
            QList <quint8> mode = {0x00, 0x03, 0x0C, 0x0F};
            qint8 index = static_cast <qint8> (list.indexOf(data.toString()));

            if (index < 0)
                return;

            buffer[7] = 0x30 | mode.at(index);
            break;
        }

        default:
            return;
    }

    payload = QByteArray(reinterpret_cast <char*> (buffer), sizeof(buffer));
    sendFrame(FRAME_SET, payload.append(static_cast <char> (crc(payload))));
}

void Conditioner::parseFrame(quint8 type, const QByteArray &payload)
{
    if (payload.isEmpty())
        return;

    switch (type)
    {
        case FRAME_SET:
        case FRAME_GET:
        case FRAME_NOTIFY:
        {
            switch (static_cast <quint8> (payload.at(0)))
            {
                case BODY_CAPABILITIES: parseCapabilities(payload); break;
                case BODY_STATUS:       parseStatus(payload); break;
            }

            break;
        }
    }
}

void Conditioner::ping(void)
{
    quint8 buffer[22];
    QByteArray payload;

    if (!m_ready)
    {
        if (m_attempts++ < CAPABILITIES_ATTEMPTS)
        {
            capabilitiesRequest(CAPABILITIES_ITEM_FIRST);
            return;
        }

        logWarning << this << "capabilities request failed, default exposes used";

        updateExposes();
        m_ready = true;

        emit deviceUpdated();
        return;
    }

    memset(buffer, 0, sizeof(buffer));

    buffer[0] = 0x41;
    buffer[1] = 0x81;
    buffer[3] = 0xFF;
    buffer[4] = 0x03;
    buffer[5] = 0xFF;
    buffer[7] = 0x02;
    buffer[20] = 0x03;
    buffer[21] = m_sequence++;

    payload = QByteArray(reinterpret_cast <char*> (buffer), sizeof(buffer));
    sendFrame(FRAME_GET, payload.append(static_cast <char> (crc(payload))));
}

double Conditioner::temperature(quint8 integer, quint8 decimal, bool fahrenheit)
{
    qint16 value = integer - 50;

    if (!fahrenheit && decimal)
        return value / 2 + decimal * (value < 0 ? -0.1 : 0.1);

    if (decimal >= 5)
        return value / 2 + (value < 0 ? -0.5 : 0.5);

    return value * 0.5;
}

void Conditioner::capabilitiesRequest(quint8 item)
{
    quint8 buffer[3] = {BODY_CAPABILITIES, 0x01, item};
    QByteArray payload = QByteArray(reinterpret_cast <char*> (buffer), sizeof(buffer));

    if (item == CAPABILITIES_ITEM_NEXT)
        payload.append(static_cast <char> (0x00));

    sendFrame(FRAME_GET, payload.append(static_cast <char> (crc(payload))));
}

void Conditioner::updateExposes(void)
{
    QList <QString> systemMode = {"off"}, fanMode, swingMode = {"off"};

    if (m_autoMode)
        systemMode.append("auto");

    if (m_coolMode)
        systemMode.append("cool");

    if (m_dryMode)
        systemMode.append("dry");

    if (m_heatMode)
        systemMode.append("heat");

    systemMode.append("fan");

    if (m_fanMin)
        fanMode.append("min");

    if (m_fanLow)
        fanMode.append("low");

    if (m_fanMedium)
        fanMode.append("medium");

    if (m_fanHigh)
        fanMode.append("high");

    if (m_fanMax)
        fanMode.append("max");

    if (m_fanAuto)
        fanMode.append("auto");

    if (m_swingVertical)
        swingMode.append("vertical");

    if (m_swingHorizontal)
        swingMode.append("horizontal");

    if (m_swingVertical && m_swingHorizontal)
        swingMode.append("both");

    m_options.insert("systemMode",          QJsonObject {{"enum", QJsonArray::fromStringList(systemMode)}});
    m_options.insert("targetTemperature",   QJsonObject {{"min", m_minTemperature}, {"max", m_maxTemperature}, {"step", 0.5}});
    m_options.insert("outdoorTemperature",  QJsonObject {{"type", "sensor"}, {"unit", "°C"}, {"icon", "mdi:home-thermometer-outline"}});

    m_exposes = {"thermostat", "outdoorTemperature"};

    if (!fanMode.isEmpty())
        m_options.insert("fanMode", QJsonObject {{"enum", QJsonArray::fromStringList(fanMode)}});

    if (swingMode.count() > 1)
        m_options.insert("swingMode", QJsonObject {{"enum", QJsonArray::fromStringList(swingMode)}});

    logInfo << this << "exposes" << m_exposes << "detected";
}

void Conditioner::parseCapabilities(const QByteArray &payload)
{
    quint8 count;
    int offset = 2;

    if (payload.length() < offset)
        return;

    count = static_cast <quint8> (payload.at(1));

    for (quint8 i = 0; i < count; i++)
    {
        quint16 item;
        quint8 size, value;

        if (payload.length() - offset < 3)
            break;

        item = static_cast <quint8> (payload.at(offset)) | static_cast <quint8> (payload.at(offset + 1)) << 8;
        size = static_cast <quint8> (payload.at(offset + 2));

        if (!size || payload.length() - offset - 3 < size)
            break;

        value = static_cast <quint8> (payload.at(offset + 3));

        switch (item)
        {
            case CAPABILITIES_FAN_SPEED:
                m_fanMin    = value == 1 || value == 6 || value == 9;
                m_fanLow    = value == 1 || (value >= 3 && value <= 7) || value == 9;
                m_fanMedium = value == 1 || (value >= 5 && value <= 7);
                m_fanHigh   = value == 1 || (value >= 3 && value <= 7) || value == 9;
                m_fanAuto   = value == 1 || (value >= 4 && value <= 6) || value == 9;
                break;

            case CAPABILITIES_PRESET_TURBO:
                m_fanMax = value != 2;
                break;

            case CAPABILITIES_MODES:

                switch (value)
                {
                    case 0x00: m_autoMode = true;  m_coolMode = true;  m_dryMode = true;  m_heatMode = false; break;
                    case 0x01: m_autoMode = true;  m_coolMode = true;  m_dryMode = true;  m_heatMode = true;  break;
                    case 0x02: m_autoMode = true;  m_coolMode = false; m_dryMode = false; m_heatMode = true;  break;
                    case 0x03: m_autoMode = false; m_coolMode = true;  m_dryMode = false; m_heatMode = false; break;
                }

                break;

            case CAPABILITIES_SWING_MODES:

                switch (value)
                {
                    case 0x00: m_swingVertical = true;  m_swingHorizontal = false; break;
                    case 0x01: m_swingVertical = true;  m_swingHorizontal = true;  break;
                    case 0x02: m_swingVertical = false; m_swingHorizontal = false; break;
                    case 0x03: m_swingVertical = false; m_swingHorizontal = true;  break;
                }

                break;

            case CAPABILITIES_TEMPERATURES:
            {
                if (size < 6)
                    break;

                m_minTemperature = qMin(value, qMin(static_cast <quint8> (payload.at(offset + 5)), static_cast <quint8> (payload.at(offset + 7)))) * 0.5;
                m_maxTemperature = qMax(static_cast <quint8> (payload.at(offset + 4)), qMax(static_cast <quint8> (payload.at(offset + 6)), static_cast <quint8> (payload.at(offset + 8)))) * 0.5;
                break;
            }
        }

        offset += size + 3;
    }

    if (payload.length() - offset == 3 && payload.at(offset))
    {
        capabilitiesRequest(CAPABILITIES_ITEM_NEXT);
        return;
    }

    updateExposes();
    m_ready = true;

    emit deviceUpdated();
}

void Conditioner::parseStatus(const QByteArray &payload)
{
    QList <QString> systemMode = {"off", "auto", "cool", "dry", "heat", "fan"};
    QMap <QString, QVariant> properties;
    bool fahrenheit;
    double value;

    if (payload.length() < 21)
        return;

    m_status = payload;
    fahrenheit = payload.at(10) & 0x04;
    value = (payload.at(2) & 0x0F) + 16;

    if (payload.at(13) & 0x1F)
        value = (payload.at(13) & 0x1F) + 12;

    if (payload.at(2) & 0x10)
        value += 0.5;

    properties.insert("systemMode", payload.at(1) & 0x01 ? systemMode.value(payload.at(2) >> 5 & 0x07) : "off");
    properties.insert("targetTemperature", value);

    if (static_cast <quint8> (payload.at(11)) != TEMPERATURE_UNKNOWN)
        properties.insert("temperature", temperature(payload.at(11), payload.at(15) & 0x0F, fahrenheit));

    if (static_cast <quint8> (payload.at(12)) != TEMPERATURE_UNKNOWN)
        properties.insert("outdoorTemperature", temperature(payload.at(12), payload.at(15) >> 4 & 0x0F, fahrenheit));

    switch (static_cast <quint8> (payload.at(3)))
    {
        case 20:         properties.insert("fanMode", "min"); break;
        case 30 ... 40:  properties.insert("fanMode", "low"); break;
        case 50 ... 60:  properties.insert("fanMode", "medium"); break;
        case 80:         properties.insert("fanMode", "high"); break;
        case 100:        properties.insert("fanMode", "max"); break;
        case 102:        properties.insert("fanMode", "auto"); break;
    }

    switch (payload.at(7) & 0x0F)
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
