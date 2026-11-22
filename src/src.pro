TARGET = harbour-shaketorch

QT += sensors dbus
QT -= gui

CONFIG += qt link_pkgconfig console

PKGCONFIG += mlite5 \
            systemsettings

SOURCES += ShakeTorch.cpp \
           accwatcher.cpp

HEADERS += \
    accwatcher.h

TARGETPATH = /usr/bin
target.path = $$TARGETPATH

INSTALLS += target
