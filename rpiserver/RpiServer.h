#ifndef RPI_SERVER_H__
#define RPI_SERVER_H__

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include "ByteBuffer.h"

class RpiServer : public QObject
{
	Q_OBJECT

public:
	RpiServer(QObject *parent = 0);
	~RpiServer();

public slots:
	void controlStationConnected();
	void carConnected();

	void controlStationDataReceived();
	void carDataReceived();

	bool SendPacketToCar(const ByteBuffer &buffer);
	bool SendPacketToControlStations(const ByteBuffer &buffer);

	void HandlePacketFromControlStation(const ByteBuffer &buffer);

	void controlStationDataReceivedFSM(QByteArray ba);
	void controlStationHandlePacket(ByteBuffer &buffer);

private:

	QTcpServer *m_controlStationListener;
	QTcpServer *m_carListener;

	QTcpSocket *m_carSocket;
	QList<QTcpSocket*> m_controlStationSocketsList;
};

#endif
