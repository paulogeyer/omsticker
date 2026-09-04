#include "MainWindow.h"
#include "Style.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("OmSticker"));
    app.setApplicationVersion(QStringLiteral("0.9.0"));
    app.setOrganizationName(QStringLiteral("Omarchy"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/omsticker.svg")));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(omstickerStyleSheet());

    MainWindow window;
    window.show();
    return app.exec();
}
