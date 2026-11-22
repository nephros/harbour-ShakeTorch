/*
    Copyright (C) 2023 Andrea Scarpino <andrea@scarpino.dev>
    All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QDebug>
#include <QDBusReply>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QSensorGestureManager>

#include <systemsettings/displaysettings.h>

#include "accwatcher.h"

#ifdef DBUS_IFACE_LEGACY
static const QString FLASHLIGHT_SERVICE("com.jolla.settings.system.flashlight");
static const QString FLASHLIGHT_PATH("/com/jolla/settings/system/flashlight");
#else
static const QString FLASHLIGHT_SERVICE("org.sailfish.flashlight.provider");
static const QString FLASHLIGHT_PATH("/org/sailfish/flashlight/provider");
#endif
static const QString FLASHLIGHT_INTERFACE(FLASHLIGHT_SERVICE);

static const QString SETTING_TURN_ON("turnOnOnShake");
static const QString SETTING_TURN_OFF("turnOffOnShake");
static const QString SETTING_BRIGHTNESS("changeBrightness");

AccWatcher::AccWatcher(QObject *parent) :
    QObject(parent),
    m_shake(0)
{
    m_ds = new DisplaySettings();
    QSensorGestureManager manager;
    if (manager.gestureIds().contains("QtSensors.shake")) {
        m_shake = new QSensorGesture(QStringList("QtSensors.shake"), this);
        m_shake->startDetection();
        connect(m_shake, SIGNAL(shake()), this, SLOT(accChanged()));
        qInfo() << "Connected to shake gesture";
    } else
        qCritical() << "QtSensors.shake gesture is missing";

    if (QDBusConnection::sessionBus().connect(
                    FLASHLIGHT_SERVICE,
                    FLASHLIGHT_PATH,
                    FLASHLIGHT_INTERFACE,
                    QStringLiteral("flashlightOnChanged"), "b",
                    this, SLOT(flashStateChanged(bool)))) {
        qInfo() << "Connected to flashlight change signal";
    } else
        qCritical() << "Could not connect flashlight change signal";

    m_config = new MDConfGroup(QStringLiteral("/apps/%1/%2")
                    .arg(parent->property("organizationName").toString())
                    .arg(parent->property("applicationName").toString())
                    );

    connect(m_config, SIGNAL(valuesChanged()), this, SLOT(updateSettings()));
    connect(m_config, SIGNAL(valueChanged(const QString&)), this, SLOT(updateSetting(const QString&)));
    updateSettings();
}

AccWatcher::~AccWatcher()
{
    delete m_shake;
    delete m_config;
}

void AccWatcher::flashStateChanged(bool on)
{
    qInfo() << "flashlight changed to" << (on ? "on" : "off");
    if (m_isOn != on)
        m_isOn = on;
}

void AccWatcher::accChanged()
{
    qDebug() << "shaken!";

    // Let's prevent the slot to be invoked twice until we've done
    disconnect(m_shake, SIGNAL(shake()), this, SLOT(accChanged()));

    // Check wheather the service is already active or not
    QDBusInterface dbus("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", QDBusConnection::sessionBus(), this);
    if (!dbus.isValid()) {
        qCritical() << "Unable to get dbus iface";
    }

    QDBusReply<bool> reply = dbus.call("NameHasOwner", FLASHLIGHT_SERVICE);
    if (!reply.isValid()) {
        qDebug() << "DBus NameHasOwner call failed with" << reply.error().name() << ":" << reply.error().message();
    }

    // Start the dbus service
    if (!reply.value()) {
        QDBusPendingCall call = dbus.asyncCall("StartServiceByName", FLASHLIGHT_SERVICE, uint(0));
        call.waitForFinished();
    }

    QDBusInterface iface(FLASHLIGHT_SERVICE, FLASHLIGHT_PATH, FLASHLIGHT_INTERFACE);
    if (iface.isValid()) {
        if ((m_isOn && m_turnOff) || (!m_isOn && m_turnOn)) {
            QDBusPendingCall call =  iface.asyncCall("toggleFlashlight");
        }
        if (m_changeBrightness) {
             handleBrightness();
        }
    }

    connect(m_shake, SIGNAL(shake()), this, SLOT(accChanged()));
}

void AccWatcher::handleBrightness()
{
    if (m_ds == nullptr) return;
    static const int max = m_ds->maximumBrightness();
    qDebug() << "Handling Brightness:"
            << m_brightness << ":"
            << m_ds->brightness()
            << "/" << max;
    if (m_ds->brightness() != max) {
        qDebug() << "Handling Brightness: settings to max";
        m_brightness = m_ds->brightness();
        m_ds->setBrightness(max);
    } else {
        if (m_brightness != -1) {
            qDebug() << "Handling Brightness: settings to saved";
            m_ds->setBrightness(m_brightness);
        }
    }
}

void AccWatcher::updateSetting(const QString& key)
{
    Q_UNUSED(key);
    updateSettings();
}

void AccWatcher::updateSettings()
{
    if (m_config == nullptr) return;
    m_turnOn  = m_config->value(SETTING_TURN_ON,  true).toBool();
    m_turnOff = m_config->value(SETTING_TURN_OFF, true).toBool();
    m_changeBrightness = m_config->value(SETTING_BRIGHTNESS, false).toBool();
    qInfo() << "Read Settings from:" << m_config->path()
            << "\nTurn on flash on shake:" << m_turnOn
            << "\nTurn off flash on shake:" << m_turnOff
            << "\nChange brightness on shake:" << m_changeBrightness << "\n";

}
