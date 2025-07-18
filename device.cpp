#include <netinet/tcp.h>
#include "device.h"
#include "logger.h"

static uint8_t const crcTable[256] =
{
    0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83, 0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41,
    0x9D, 0xC3, 0x21, 0x7F, 0xFC, 0xA2, 0x40, 0x1E, 0x5F, 0x01, 0xE3, 0xBD, 0x3E, 0x60, 0x82, 0xDC,
    0x23, 0x7D, 0x9F, 0xC1, 0x42, 0x1C, 0xFE, 0xA0, 0xE1, 0xBF, 0x5D, 0x03, 0x80, 0xDE, 0x3C, 0x62,
    0xBE, 0xE0, 0x02, 0x5C, 0xDF, 0x81, 0x63, 0x3D, 0x7C, 0x22, 0xC0, 0x9E, 0x1D, 0x43, 0xA1, 0xFF,
    0x46, 0x18, 0xFA, 0xA4, 0x27, 0x79, 0x9B, 0xC5, 0x84, 0xDA, 0x38, 0x66, 0xE5, 0xBB, 0x59, 0x07,
    0xDB, 0x85, 0x67, 0x39, 0xBA, 0xE4, 0x06, 0x58, 0x19, 0x47, 0xA5, 0xFB, 0x78, 0x26, 0xC4, 0x9A,
    0x65, 0x3B, 0xD9, 0x87, 0x04, 0x5A, 0xB8, 0xE6, 0xA7, 0xF9, 0x1B, 0x45, 0xC6, 0x98, 0x7A, 0x24,
    0xF8, 0xA6, 0x44, 0x1A, 0x99, 0xC7, 0x25, 0x7B, 0x3A, 0x64, 0x86, 0xD8, 0x5B, 0x05, 0xE7, 0xB9,
    0x8C, 0xD2, 0x30, 0x6E, 0xED, 0xB3, 0x51, 0x0F, 0x4E, 0x10, 0xF2, 0xAC, 0x2F, 0x71, 0x93, 0xCD,
    0x11, 0x4F, 0xAD, 0xF3, 0x70, 0x2E, 0xCC, 0x92, 0xD3, 0x8D, 0x6F, 0x31, 0xB2, 0xEC, 0x0E, 0x50,
    0xAF, 0xF1, 0x13, 0x4D, 0xCE, 0x90, 0x72, 0x2C, 0x6D, 0x33, 0xD1, 0x8F, 0x0C, 0x52, 0xB0, 0xEE,
    0x32, 0x6C, 0x8E, 0xD0, 0x53, 0x0D, 0xEF, 0xB1, 0xF0, 0xAE, 0x4C, 0x12, 0x91, 0xCF, 0x2D, 0x73,
    0xCA, 0x94, 0x76, 0x28, 0xAB, 0xF5, 0x17, 0x49, 0x08, 0x56, 0xB4, 0xEA, 0x69, 0x37, 0xD5, 0x8B,
    0x57, 0x09, 0xEB, 0xB5, 0x36, 0x68, 0x8A, 0xD4, 0x95, 0xCB, 0x29, 0x77, 0xF4, 0xAA, 0x48, 0x16,
    0xE9, 0xB7, 0x55, 0x0B, 0x88, 0xD6, 0x34, 0x6A, 0x2B, 0x75, 0x97, 0xC9, 0x4A, 0x14, 0xF6, 0xA8,
    0x74, 0x2A, 0xC8, 0x96, 0x15, 0x4B, 0xA9, 0xF7, 0xB6, 0xE8, 0x0A, 0x54, 0xD7, 0x89, 0x6B, 0x35
};

DeviceObject::DeviceObject(quint8 appliance, const QString &port, const QString &name, bool debug) : QObject(nullptr), m_appliance(appliance), m_protocol(0), m_name(name), m_debug(debug), m_receiveTimer(new QTimer(this)), m_resetTimer(new QTimer(this)), m_updateTimer(new QTimer(this)), m_serial(new QSerialPort(this)), m_socket(new QTcpSocket(this)), m_serialError(false), m_connected(false), m_availability(Availability::Unknown)
{
     if (!port.startsWith("tcp://"))
     {
         m_device = m_serial;

         m_serial->setPortName(port);
         m_serial->setBaudRate(9600);
         m_serial->setDataBits(QSerialPort::Data8);
         m_serial->setParity(QSerialPort::NoParity);
         m_serial->setStopBits(QSerialPort::OneStop);

         connect(m_serial, &QSerialPort::errorOccurred, this, &DeviceObject::serialError);
     }
     else
     {
         QList <QString> list = QString(port).remove("tcp://").split(':');

         m_device = m_socket;
         m_adddress = QHostAddress(list.value(0));
         m_port = static_cast <quint16> (list.value(1).toInt());

         connect(m_socket, &QTcpSocket::errorOccurred, this, &DeviceObject::socketError);
         connect(m_socket, &QTcpSocket::connected, this, &DeviceObject::socketConnected);
     }

     connect(m_device, &QIODevice::readyRead, this, &DeviceObject::startTimer);
     connect(m_receiveTimer, &QTimer::timeout, this, &DeviceObject::readyRead);
     connect(m_resetTimer, &QTimer::timeout, this, &DeviceObject::reset);
     connect(m_updateTimer, &QTimer::timeout, this, &DeviceObject::update);

     m_receiveTimer->setSingleShot(true);
     m_resetTimer->setSingleShot(true);

     m_updateTimer->start(UPDATE_INTERVAL);
}

DeviceObject::~DeviceObject(void)
{
    if (m_connected)
        m_socket->disconnectFromHost();
}

void DeviceObject::init(void)
{
    if (m_device == m_serial)
    {
        if (m_serial->isOpen())
            m_serial->close();

        if (!m_serial->open(QIODevice::ReadWrite))
            return;

        logInfo << this << "port" << m_serial->portName() << "opened successfully";
        m_serial->clear();
    }
    else
    {
        if (m_adddress.isNull() && !m_port)
        {
            logWarning << this << "has invalid connection address or port number";
            return;
        }

        if (m_connected)
            m_socket->disconnectFromHost();

        m_socket->connectToHost(m_adddress, m_port);
    }
}

quint8 DeviceObject::checksum(const QByteArray &data)
{
    quint8 checksum = 0;

    for (int i = 0; i < data.length(); i++)
        checksum -= static_cast <quint8> (data.at(i));

    return checksum;
}

quint8 DeviceObject::crc(const QByteArray &data)
{
    quint8 crc = 0;

    for (int i = 0; i < data.length(); i++)
        crc = crcTable[data.at(i) ^ crc];

    return crc;
}

void DeviceObject::updateAvailability(Availability availability)
{
    if (m_availability == availability)
        return;

    m_availability = availability;
    emit availabilityUpdated(availability);
}

void DeviceObject::sendFrame(quint8 type, const QByteArray &payload)
{
    headerStruct header;
    QByteArray data;

    memset(&header, 0, sizeof(header));

    header.startByte = START_BYTE;
    header.length = static_cast <quint8> (payload.length() + sizeof(header));
    header.appliance = m_appliance;
    header.protocol = m_protocol;
    header.type = type;

    data = QByteArray(reinterpret_cast <char*> (&header), sizeof(header)).append(payload);
    data.append(static_cast <char> (checksum(data.mid(1))));

    logDebug(m_debug) << this << "serial data sent:" << data.toHex(':');

    m_device->write(data);
    m_device->waitForBytesWritten(1000);
}

void DeviceObject::serialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::SerialPortError::NoError)
    {
        m_serialError = false;
        return;
    }

    if (!m_serialError)
        logWarning << this << "serial port error:" << error;

    m_resetTimer->start(RESET_TIMEOUT);
    m_serialError = true;
}

void DeviceObject::socketError(QTcpSocket::SocketError error)
{
    logWarning << this << "connection error:" << error;
    m_resetTimer->start(RESET_TIMEOUT);
    m_connected = false;
}

void DeviceObject::socketConnected(void)
{
    int descriptor = m_socket->socketDescriptor(), keepAlive = 1, interval = 10, count = 3;

    setsockopt(descriptor, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));
    setsockopt(descriptor, SOL_TCP, TCP_KEEPIDLE, &interval, sizeof(interval));
    setsockopt(descriptor, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(descriptor, SOL_TCP, TCP_KEEPCNT, &count, sizeof(count));

    logInfo << this << "successfully connected to" << QString("%1:%2").arg(m_adddress.toString()).arg(m_port);
    m_socket->readAll();
    m_connected = true;
}

void DeviceObject::startTimer(void)
{
    m_receiveTimer->start(RECEIVE_TIMEOUT);
}

void DeviceObject::readyRead(void)
{
    QByteArray data = m_device->readAll();

    logDebug(m_debug) << this << "serial data received:" << data.toHex(':');
    m_buffer.append(data);

    if (m_buffer.length() >= BUFFER_LENGTH_LIMIT)
    {
        m_buffer.clear();
        return;
    }

    while (!m_buffer.isEmpty())
    {
        int offset = m_buffer.indexOf(START_BYTE);
        headerStruct *header;

        if (offset < 0 || static_cast <size_t> (m_buffer.length() - offset) < sizeof(headerStruct))
            return;

        header = reinterpret_cast <headerStruct*> (m_buffer.data() + offset);

        if (m_buffer.length() - offset < header->length + 1)
            return;

        if (static_cast <quint8> (m_buffer.at(offset + header->length)) != checksum(m_buffer.mid(offset + 1, header->length - 1)))
        {
            logWarning << this << "frame" << m_buffer.mid(offset, header->length + 1).toHex(':') << "checksum mismatch";
            m_buffer.clear();
            return;
        }

        logDebug(m_debug) << this << "frame received" << m_buffer.mid(offset, header->length + 1).toHex(':');
        updateAvailability(Availability::Online);

        m_lastSeen = QDateTime::currentMSecsSinceEpoch();
        m_protocol = header->protocol;

        switch (header->type)
        {
            case FRAME_NETWORK_QUERY: // TODO: use real address?
            {
                quint8 data[20] = {0x01, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                QByteArray payload = QByteArray(reinterpret_cast <char*> (data), sizeof(data));
                sendFrame(FRAME_NETWORK_QUERY, payload.append(static_cast <char> (crc(payload))));
                break;
            }

            default:
            {
                parseFrame(header->type, m_buffer.mid(offset + sizeof(headerStruct), header->length - sizeof(headerStruct)));
                break;
            }
        }

        m_buffer.remove(0, offset + header->length + 1);
    }
}

void DeviceObject::reset(void)
{
    init();
}

void DeviceObject::update(void)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (now > m_lastSeen + UPDATE_INTERVAL)
        ping();

    if (now > m_lastSeen + UNAVAILABLE_TIMEOUT && m_availability != Availability::Offline)
        updateAvailability(Availability::Offline);
}
