#ifndef RPI_CLIENT_H__
#define RPI_CLIENT_H__

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTime>
#include <QFile>
#include "ByteBuffer.h"
#include <stdint.h>

class RpiClient : public QObject
{
	Q_OBJECT

public:
	RpiClient(QObject *parent = 0);
	~RpiClient();

	void setMotorPWM(int32_t duty); // 0-100%
	void setDirection(int32_t direction);

	void enablePWMs();
	void disablePWMs();

public slots:
	void linkConnected();
	void linkDisconnected();
	void linkStateChanged(QAbstractSocket::SocketState socketState);
	void linkDataReceived();

	void dataReceivedFSM(QByteArray ba);
	void handlePacket(ByteBuffer &buffer);

	void handleRobotSpeedSet(ByteBuffer &packet);
	void watchdog();

private:
	QTime m_lastPWMUpdate;
	QTcpSocket *m_socketLink;
	QTimer *m_watchdogTimer;
};

#endif
