//
#include <QtCore>
#include <QDebug>

#include "RpiClient.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
	RpiClient *client = new RpiClient();

    return a.exec();
}
