QT += widgets network
CONFIG += c++20
TEMPLATE = app
TARGET = omsticker
DESTDIR = $$PWD/../../build

SOURCES = main.cpp MainWindow.cpp DeviceScanner.cpp FlashSession.cpp IsoDownloader.cpp
HEADERS = MainWindow.h DeviceScanner.h FlashSession.h IsoDownloader.h Style.h WriteCheckpoint.h ../shared/Protocol.h
INCLUDEPATH += ../shared
RESOURCES += ../../data/resources.qrc

isEmpty(PREFIX): PREFIX = /usr
target.path = $$PREFIX/bin
desktop.path = $$PREFIX/share/applications
desktop.files = $$PWD/../../data/omsticker.desktop
icon.path = $$PREFIX/share/icons/hicolor/scalable/apps
icon.files = $$PWD/../../data/icons/omsticker.svg
INSTALLS += target desktop icon
