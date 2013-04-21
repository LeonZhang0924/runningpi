//
#include <QtCore>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>

#include "RpiServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
	RpiServer *server = new RpiServer();


    return a.exec();
}
