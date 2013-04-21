#include "RpiClient.h"

#include <QDebug>
#include <cstdio>
#include <QTextStream>
#include "opcodes.h"

#define TIMEOUT_MS 500
#define SERVER_ADDR "10.8.0.1"
#define SERVER_PORT 31337

RpiClient::RpiClient(QObject *parent) : QObject(parent)
{
	qDebug() << __FUNCTION__;

	m_watchdogTimer = new QTimer(this);
	connect(m_watchdogTimer, SIGNAL(timeout()), this, SLOT(watchdog()));
	m_lastPWMUpdate = QTime::currentTime();

	m_socketLink = new QTcpSocket(this);
	m_socketLink->connectToHost(SERVER_ADDR, SERVER_PORT);

	QObject::connect(m_socketLink, SIGNAL(connected()), this, SLOT(linkConnected()));
	QObject::connect(m_socketLink, SIGNAL(disconnected()), this, SLOT(linkDisconnected()));
	QObject::connect(m_socketLink, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(linkStateChanged(QAbstractSocket::SocketState)));
	QObject::connect(m_socketLink, SIGNAL(readyRead()), this, SLOT(linkDataReceived()));
	m_watchdogTimer->start(500);

	QFile file("/sys/devices/platform/rpi-pwm/pwm0/mode");
	if (file.open(QIODevice::WriteOnly) == false) {
		qWarning() << "Couldn't open mode";
		return;
	}
	QTextStream writeStream(&file);
	writeStream << "servo";
	file.close();
	this->setDirection(0);
}

RpiClient::~RpiClient()
{
	//qDebug() << __FUNCTION__;
}

void RpiClient::watchdog()
{
	//qDebug() << __FUNCTION__;
	if (m_lastPWMUpdate.msecsTo(QTime::currentTime()) > TIMEOUT_MS) {
		qDebug() << "WARNING: cutting power!";
		this->disablePWMs();
	}

	if (m_socketLink->state() == QAbstractSocket::UnconnectedState) {
		m_socketLink->connectToHost(SERVER_ADDR, SERVER_PORT);
	}

    m_watchdogTimer->start(500);
}

void RpiClient::linkConnected()
{
	qDebug() << __FUNCTION__;
}

void RpiClient::linkDisconnected()
{
	qDebug() << __FUNCTION__;
}

void RpiClient::linkStateChanged(QAbstractSocket::SocketState socketState)
{
	qDebug() << __FUNCTION__;
	qDebug() << socketState;
}

void RpiClient::linkDataReceived()
{
	//qDebug() << __FUNCTION__;

	QByteArray ba = m_socketLink->readAll();
	//qDebug() << ba;
	this->dataReceivedFSM(ba);
}


void RpiClient::dataReceivedFSM(QByteArray ba)
{
	static ByteBuffer currentPacket;
	static uint8_t expectedPacketLen = 0;
	static uint8_t currentPacketLen = 0;
	static comm_Stage comm_stage = STAGE_0;

	//qDebug() << __FUNCTION__;

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
				//qDebug() << "Got header: " << data;
				comm_stage = STAGE_1;
			} else {
				//logStream << (char)data;
				//qDebug() << "Invalid header, got: " << (int)data;
			}
		} break;

		case STAGE_1: {
			expectedPacketLen = data;
			comm_stage = STAGE_2;
			//qDebug() << "expected len = " << expectedPacketLen;
		} break;

		case STAGE_2: {
			currentPacket.append(data);
			currentPacketLen++;
		} break;
		} // switch (comm_stage)
		if (comm_stage == STAGE_2) {
			if (currentPacket.size() == expectedPacketLen) {
				//qDebug() << "HANDLE PACKET!!!!";
				comm_stage = STAGE_0;
				this->handlePacket(currentPacket);
			}
		}
	} // while (m_receivedDataBuffer.size())
	//logStream.flush();
}

void RpiClient::handlePacket(ByteBuffer &packet)
{
	//qDebug() << __FUNCTION__;
	uint8_t opcode;
	packet >> opcode;

	switch (opcode) {
	case MMSG_SET_SPEED_AND_DIRECTION: {
		//qDebug() << "GOT MMSG_SET_SPEED_AND_DIRECTION";

		tSetSpeedAndDirection payload;
		if (packet.size() < sizeof(payload)) {
			qWarning("%s: Invalid packet size!", __FUNCTION__);
			return;
		}

		packet.read((uint8_t*)&payload, sizeof(payload));
		this->setMotorPWM(payload.speed);
		this->setDirection(payload.directionAngle);

		qDebug() << "got angle: " << payload.directionAngle << " and speed: " << payload.speed;
	} break;

	default: {
		qWarning() << "WARNING: Unhandled opcode from Master Server (" << (int)opcode << ")";
	} break;
	}
}

void RpiClient::handleRobotSpeedSet(ByteBuffer &packet)
{

}

void RpiClient::setMotorPWM(int32_t duty)
{
	if (!(duty >= -100 && duty <= 100)) {
		qWarning() << "BAD DUTY";
		return;
	}
	this->m_lastPWMUpdate = QTime::currentTime();

	{
		QFile file("/sys/pwmdev/direction");
		if (file.open(QIODevice::WriteOnly) == false) {
			qWarning() << "Couldn't open direction file";
			return;
		}
		QTextStream writeStream(&file);
		if (duty < 0) {
			writeStream << "1";
		} else {
			writeStream << "0";
		}
		file.close();
	}

	duty = abs(duty);

	QFile file("/sys/pwmdev/motor_pwm_duty");
	if (file.open(QIODevice::WriteOnly) == false) {
		qWarning() << "Couldn't open motorPWMFile";
		return;
	}
	QTextStream writeStream(&file);
	uint32_t pwmDuty = duty * 500000/100;
	writeStream << QString("%1").arg(pwmDuty);

	file.close();

	this->enablePWMs();
}

void RpiClient::setDirection(int32_t angle)
{
	if (!(angle >= -90 && angle <= 90)) {
		qWarning() << "BAD DUTY";
		return;
	}

	this->m_lastPWMUpdate = QTime::currentTime();

	QFile file("/sys/devices/platform/rpi-pwm/pwm0/servo");
	if (file.open(QIODevice::WriteOnly) == false) {
		qWarning() << "Couldn't open motorPWMFile";
		return;
	}
	QTextStream writeStream(&file);
	uint32_t pwmDuty = 21 - angle/9;
	if (pwmDuty > 26)
		pwmDuty = 26;
	if (pwmDuty < 16)
		pwmDuty = 16;
	writeStream << QString("%1").arg(pwmDuty);

	file.close();

	this->enablePWMs();
}

void RpiClient::enablePWMs()
{
	QFile file("/sys/pwmdev/enabled");
	if (file.open(QIODevice::WriteOnly) == false) {
		qWarning() << "Couldn't open enabled";
		return;
	}
	QTextStream writeStream(&file);
	writeStream << "1";

	file.close();
}

void RpiClient::disablePWMs()
{
	QFile file("/sys/pwmdev/enabled");
	if (file.open(QIODevice::WriteOnly) == false) {
		qWarning() << "Couldn't open enabled";
		return;
	}
	QTextStream writeStream(&file);
	writeStream << "0";

	file.close();
}
