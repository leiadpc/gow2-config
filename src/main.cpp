#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("GearIniEditor");
    QApplication::setApplicationName("GearIniEditor");

    const QString initialPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();

    MainWindow win(initialPath);
    win.show();

    return QApplication::exec();
}
