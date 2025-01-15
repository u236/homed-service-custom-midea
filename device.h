#ifndef DEVICE_H
#define DEVICE_H

#define RECEIVE_TIMEOUT             20
#define RESET_TIMEOUT               10000
#define UPDATE_INTERVAL             2000
#define UNAVAILABLE_TIMEOUT         10000

#define START_BYTE                  0xAA
#define BUFFER_LENGTH_LIMIT         1024

#define FRAME_SET                   0x02
#define FRAME_GET                   0x03
#define FRAME_NOTIFY                0x04
#define FRAME_NETWORK_QUERY         0x63

#include <QHostAddress>
#include <QJsonObject>
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

class DeviceObject;
typedef QSharedPointer <DeviceObject> Device;

class DeviceObject : public QObject
{
    Q_OBJECT

public:

    DeviceObject(const QString &port, const QString &name, bool debug);
    ~DeviceObject(void);

    virtual void action(const QString &name, const QVariant &data) = 0;

    inline QString name(void) { return m_name; }
    inline QList <QString> exposes(void) { return m_exposes; }
    inline QMap <QString, QVariant> options(void) { return m_options; }

    void init(void);

protected:

    QString m_name;
    bool m_debug;

    QTimer *m_receiveTimer, *m_resetTimer, *m_updateTimer;

    QSerialPort *m_serial;
    QTcpSocket *m_socket;
    QIODevice *m_device;

    bool m_serialError;

    QHostAddress m_adddress;
    quint16 m_port;
    bool m_connected;

    QByteArray m_buffer;

    Availability m_availability;
    qint64 m_lastSeen;
    quint8 m_protocol;

    QList <QString> m_actions, m_exposes;
    QMap <QString, QVariant> m_options, m_properties;

    virtual void parseFrame(quint8 type, const QByteArray &payload) = 0;
    virtual void ping(void) = 0;

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
