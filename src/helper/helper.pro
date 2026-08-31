QT = core dbus
CONFIG += c++20 console link_pkgconfig
CONFIG -= app_bundle
TEMPLATE = app
TARGET = omsticker-helper
DESTDIR = $$PWD/../../build

SOURCES = main.cpp Flasher.cpp UDisks.cpp
HEADERS = Flasher.h UDisks.h ../shared/Protocol.h
INCLUDEPATH += ../shared

PKGCONFIG += fdisk

isEmpty(PREFIX): PREFIX = /usr
target.path = $$PREFIX/lib/omsticker
INSTALLS += target
