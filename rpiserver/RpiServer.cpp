#include "RpiServer.h"

#include <QDebug>
#include <cstdio>
#include "opcodes.h"

RpiServer::RpiServer(QObject *parent) : QObject(parent)
{
	m_carSocket = NULL;

	m_controlStationListener = new QTcpServer;
	m_carListener = new QTcpServer;

	m_controlStationListener->listen(QHostAddress::Any, 9999);
	m_carListener->listen(QHostAddress::Any, 31337);

	QObject::connect(m_controlStationListener, SIGNAL(newConnection()), this, SLOT(controlStationConnected()));
	QObject::connect(m_carListener, SIGNAL(newConnection()), this, SLOT(carConnected()));

	qDebug() << "Server initialized";
}

RpiServer::~RpiServer()
{

}

void RpiServer::controlStationConnected()
{
	qDebug() << "control station connected\n";

	QTcpSocket *connectedSocket = m_controlStationListener->nextPendingConnection();
	m_controlStationSocketsList.push_back(connectedSocket);

	QObject::connect(connectedSocket, SIGNAL(readyRead()), this, SLOT(controlStationDataReceived()));
}

void RpiServer::carConnected()
{
	qDebug() << "car connected\n";

	QTcpSocket *connectedSocket = m_carListener->nextPendingConnection();
	m_carSocket = connectedSocket;

	QObject::connect(connectedSocket, SIGNAL(readyRead()), this, SLOT(carDataReceived()));
}

void RpiServer::carDataReceived()
{
	qDebug() << __FUNCTION__;

	QTcpSocket *socketLink = m_carSocket;
	if(socketLink->state() == QAbstractSocket::ConnectedState) {
		QByteArray ba = socketLink->readAll();
		qDebug() << "data: " << ba;
		// fa ceva cu datele

	} else {
		qWarning() << "Car is not connected anymore!";
		socketLink->disconnectFromHost();
	}
}

void RpiServer::controlStationDataReceived()
{
	qDebug() << __FUNCTION__;

	for (int i = 0; i < m_controlStationSocketsList.size(); ++i) {
		QTcpSocket *socketLink = m_controlStationSocketsList.at(i);
		if(socketLink->state() == QAbstractSocket::ConnectedState) {
			QByteArray ba = socketLink->readAll();
			this->controlStationDataReceivedFSM(ba);
		} else {
			qWarning() << "Socket is not connected anymore, cleaning up!";
			socketLink->disconnectFromHost();
			delete socketLink;
			m_controlStationSocketsList.removeAt(i);
			controlStationDataReceived();
			break;
		}
	}
}

void RpiServer::controlStationDataReceivedFSM(QByteArray ba)
{
	static ByteBuffer currentPacket;
	static uint8_t expectedPacketLen = 0;
	static uint8_t currentPacketLen = 0;
	static comm_Stage comm_stage = STAGE_0;

	qDebug() << QTime::currentTime() << ": " << __FUNCTION__;

	ByteBuffer m_receivedDataBuffer;
	m_receivedDataBuffer.clear();
	m_receivedDataBuffer.append((uint8_t*)ba.data(), ba.count());

	for(size_t i = 0; i < m_receivedDataBuffer.size(); i++) {
		static uint8_t data;
		m_receivedDataBuffer >> data;
		switch (comm_stage) {
		case STAGE_0: {
			currentPacket.clear();
			currentPacketLen = 0;
			if (data == SYNC_HEADER) {
				qDebug() << "Got header: " << data;
				comm_stage = STAGE_1;
			} else {
				//logStream << (char)data;
				qDebug() << "NU AM PRIMIT HEADER WTF: GOT:" << (int)data;
			}
		} break;

		case STAGE_1: {
			expectedPacketLen = data;
			comm_stage = STAGE_2;
			qDebug() << "expected len = " << expectedPacketLen;
		} break;

		case STAGE_2: {
			currentPacket.append(data);
			currentPacketLen++;
		} break;
		} // switch (comm_stage)
		if (comm_stage == STAGE_2) {
			if (currentPacket.size() == expectedPacketLen) {
				qDebug() << "HANDLE PACKET!!!!";
				comm_stage = STAGE_0;
				this->controlStationHandlePacket(currentPacket);
			}
		}
	} // while (m_receivedDataBuffer.size())
	//logStream.flush();
}

void RpiServer::controlStationHandlePacket(ByteBuffer &packet)
{
	qDebug() << __FUNCTION__;
	uint8_t opcode;
	packet >> opcode;

	switch (opcode) {
	case PMSG_ATTITUDE: {
		int roll, pitch;
		ByteBuffer buffer;
		char message[128];

		qDebug() << "GOT PMSG_ATTITUDE";
		packet.read((uint8_t*)&message, packet.size()-1);
		sscanf(message, "%d %d", &roll, &pitch);
		qDebug() << "Roll: " << roll << "; Pitch: " << pitch;

		tSetSpeedAndDirection payload;
		payload.speed = roll * 2;
		payload.directionAngle = pitch;

		buffer << (uint8_t)MMSG_SET_SPEED_AND_DIRECTION;
		buffer.append((uint8_t*)&payload, sizeof(payload));
		SendPacketToCar(buffer);
	} break;

	case MMSG_SET_SPEED_AND_DIRECTION: {
		qDebug() << "GOT MMSG_SET_SPEED_AND_DIRECTION";
		tSetSpeedAndDirection payload;
		packet.read((uint8_t*)&payload, sizeof(payload));

		ByteBuffer buffer;
		buffer << (uint8_t)MMSG_SET_SPEED_AND_DIRECTION;
		buffer.append((uint8_t*)&payload, sizeof(payload));

		SendPacketToCar(buffer);
	} break;
	default: {
		qWarning() << "WARNING: Unhandled opcode from CS (" << (int)opcode << ")";
	} break;
	}
}

bool RpiServer::SendPacketToCar(const ByteBuffer &buffer)
{
	qDebug() << __FUNCTION__;
	bool status = false;
	QTcpSocket *socketLink = m_carSocket;

	if(socketLink && socketLink->state() == QAbstractSocket::ConnectedState) {
		ByteBuffer txbuffer;
		txbuffer << (uint8_t)SYNC_HEADER;
		txbuffer << (uint8_t)(buffer.size());
		txbuffer.append(buffer.contents(), buffer.size());
		status = (socketLink->write((const char*)txbuffer.contents(), txbuffer.size()) == txbuffer.size());
	} else {
		qWarning() << "WARNING: There's NO CAR connected!";
	}

	return status;
}

bool RpiServer::SendPacketToControlStations(const ByteBuffer &buffer)
{
	qDebug() << __FUNCTION__;
	bool status = false;

	for (int i = 0; i < m_controlStationSocketsList.size(); ++i) {
		QTcpSocket *socketLink = m_controlStationSocketsList.at(i);
		if(socketLink->state() == QAbstractSocket::ConnectedState) {
			ByteBuffer txbuffer;
			txbuffer << (uint8_t)SYNC_HEADER;
			txbuffer << (uint8_t)(buffer.size());
			txbuffer.append(buffer.contents(), buffer.size());

			status = (socketLink->write((const char*)txbuffer.contents(), txbuffer.size()) == txbuffer.size());
		} else {
			qWarning() << "Socket is not connected anymore!";
		}
	}

	return status;
}

void RpiServer::HandlePacketFromControlStation(const ByteBuffer &buffer)
{
	qDebug() << __FUNCTION__;

}

