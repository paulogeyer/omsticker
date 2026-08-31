QT += widgets network
CONFIG += c++20
TEMPLATE = app
TARGET = omsticker
DESTDIR = $$PWD/../../build

SOURCES = main.cpp MainWindow.cpp DeviceScanner.cpp FlashSession.cpp IsoDownloader.cpp
HEADERS = MainWindow.h DeviceScanner.h FlashSession.h IsoDownloader.h Style.h ../shared/Protocol.h
INCLUDEPATH += ../shared
RESOURCES += ../../data/resources.qrc

isEmpty(PREFIX): PREFIX = /usr
target.path = $$PREFIX/bin
desktop.path = $$PREFIX/share/applications
desktop.files = $$PWD/../../data/omsticker.desktop
policy.path = $$PREFIX/share/polkit-1/actions
policy.files = $$PWD/../../data/org.omarchy.omsticker.policy
icon.path = $$PREFIX/share/icons/hicolor/scalable/apps
icon.files = $$PWD/../../data/icons/omsticker.svg
INSTALLS += target desktop policy icon
