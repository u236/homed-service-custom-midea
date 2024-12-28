#ifndef DEVICE_H
#define DEVICE_H

#define RECEIVE_TIMEOUT             20
#define RESET_TIMEOUT               10000
#define UPDATE_INTERVAL             2000
#define UNAVAILABLE_TIMEOUT         10000

#define START_BYTE                  0xAA
#define BUFFER_LENGTH_LIMIT         1024
#define STATUS_PAYLOAD_LENGTH       37

#define FRAME_SET                   0x02
#define FRAME_GET                   0x03
#define FRAME_NOTIFY                0x04
#define FRAME_NETWORK_QUERY         0x63

#include <QHostAddress>
#include <QSerialPort>
#include <QSettings>
#include <QTcpSocket>
#include <QTimer>

enum class Availability
{
    Unknown,
    Online,
    Offline,
    Inactive
};

struct headerStruct
{
    quint8 startByte;
    quint8 length;
    quint8 appliance;
    quint8 sync[5];
    quint8 protocol;
    quint8 type;
};

class Device : public QObject
{
    Q_OBJECT

public:

    Device(QSettings *config, QObject *parent);
    ~Device(void);

    void init(void);
    void action(const QString &name, const QVariant &data);

private:

    QTimer *m_receiveTimer, *m_resetTimer, *m_updateTimer;

    QSerialPort *m_serial;
    QTcpSocket *m_socket;
    QIODevice *m_device;

    bool m_serialError;

    QHostAddress m_adddress;
    quint16 m_port;
    bool m_connected;

    QByteArray m_buffer;
    bool m_debug;

    Availability m_availability;
    qint64 m_lastSeen;
    quint8 m_protocol;

    QList <QString> m_actions;
    QMap <QString, QVariant> m_properties;

    quint8 checksum(const QByteArray &data);
    quint8 crc(const QByteArray &data);

    void updateAvailability(Availability available);
    void sendFrame(quint8 type, const QByteArray &data);

private slots:

    void serialError(QSerialPort::SerialPortError error);

    void socketError(QTcpSocket::SocketError error);
    void socketConnected(void);

    void startTimer(void);
    void readyRead(void);

    void reset(void);
    void update(void);

signals:

    void availabilityUpdated(Availability availability);
    void propertiesUpdated(const QMap <QString, QVariant> &properties);

};

#endif
