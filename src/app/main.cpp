#include "MainWindow.h"
#include "Style.h"
#include "Version.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("OmSticker"));
    app.setApplicationVersion(QString::fromUtf8(kVersion));
    app.setDesktopFileName(QStringLiteral("omsticker"));
    app.setOrganizationName(QStringLiteral("Omarchy"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/omsticker.svg")));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(omstickerStyleSheet());

    MainWindow window;
    window.show();
    return app.exec();
}
