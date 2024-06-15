/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "AutoTypeWayland.h"

#include "autotype/AutoTypeAction.h"
#include "core/Tools.h"
#include "gui/osutils/nixutils/X11Funcs.h"

#include <QDBusMessage>
#include <QDebug>
#include <QRandomGenerator>

QString generateToken()
{
    static uint next = 0;
    return QString("keepassxc_%1_%2").arg(next++).arg(QRandomGenerator::system()->generate());
}

AutoTypePlatformWayland::AutoTypePlatformWayland()
    : m_bus(QDBusConnection::sessionBus())
    , m_remote_desktop("org.freedesktop.portal.Desktop",
                       "/org/freedesktop/portal/desktop",
                       "org.freedesktop.portal.RemoteDesktop",
                       m_bus,
                       this)
{
    m_bus.connect("org.freedesktop.portal.Desktop",
                  "",
                  "org.freedesktop.portal.Request",
                  "Response",
                  this,
                  SLOT(portalResponse(uint, QVariantMap, QDBusMessage)));

    createSession();
}

void AutoTypePlatformWayland::createSession()
{
    QString requestHandle = generateToken();

    m_handlers.insert(requestHandle,
                      [this](uint _response, QVariantMap _result) { handleCreateSession(_response, _result); });

    m_remote_desktop.call("CreateSession",
                          QVariantMap{{"handle_token", requestHandle}, {"session_handle_token", generateToken()}});
}

void AutoTypePlatformWayland::handleCreateSession(uint response, QVariantMap result)
{
    qDebug() << "Got response and result" << response << result;
    if (response == 0) {
        m_session_handle = QDBusObjectPath(result["session_handle"].toString());

        QString selectDevicesRequestHandle = generateToken();
        m_handlers.insert(selectDevicesRequestHandle,
                          [this](uint _response, QVariantMap _result) { handleSelectDevices(_response, _result); });

        QVariantMap selectDevicesOptions{
            {"handle_token", selectDevicesRequestHandle},
            {"types", uint(1)},
            {"persist_mode", uint(2)},
        };

        // TODO: Store restore token in database/some other persistent data so the dialog doesn't appear every launch
        if (!m_restore_token.isEmpty()) {
            selectDevicesOptions.insert("restore_token", m_restore_token);
        }

        m_remote_desktop.call("SelectDevices", m_session_handle, selectDevicesOptions);

        QString startRequestHandle = generateToken();
        m_handlers.insert(startRequestHandle,
                          [this](uint _response, QVariantMap _result) { handleStart(_response, _result); });

        QVariantMap startOptions{
            {"handle_token", startRequestHandle},
        };

        // TODO: Pass window identifier here instead of empty string if we want the dialog to appear on top of the
        // application window, need to be able to get active window and handle from Wayland
        m_remote_desktop.call("Start", m_session_handle, "", startOptions);
    }
}

void AutoTypePlatformWayland::handleSelectDevices(uint response, QVariantMap result)
{
    Q_UNUSED(result)
    qDebug() << "Select Devices: " << response << result;
}

void AutoTypePlatformWayland::handleStart(uint response, QVariantMap result)
{
    qDebug() << "Start: " << response << result;
    if (response == 0) {
        m_session_started = true;
        m_restore_token = result["restore_token"].toString();
    }
}

void AutoTypePlatformWayland::portalResponse(uint response, QVariantMap results, QDBusMessage message)
{
    Q_UNUSED(response)
    Q_UNUSED(results)
    qDebug() << "Received message: " << message;
    auto index = message.path().lastIndexOf("/");
    auto handle = message.path().right(message.path().length() - index - 1);
    if (m_handlers.contains(handle)) {
        m_handlers.take(handle)(response, results);
    }
}

AutoTypeAction::Result AutoTypePlatformWayland::sendKey(xkb_keysym_t keysym, QVector<xkb_keysym_t> modifiers)
{
    for (auto modifier : modifiers) {
        m_remote_desktop.call("NotifyKeyboardKeysym", m_session_handle, QVariantMap(), int(modifier), uint(1));
    }

    m_remote_desktop.call("NotifyKeyboardKeysym", m_session_handle, QVariantMap(), int(keysym), uint(1));

    m_remote_desktop.call("NotifyKeyboardKeysym", m_session_handle, QVariantMap(), int(keysym), uint(0));

    for (auto modifier : modifiers) {
        m_remote_desktop.call("NotifyKeyboardKeysym", m_session_handle, QVariantMap(), int(modifier), uint(0));
    }
    return AutoTypeAction::Result::Ok();
}

bool AutoTypePlatformWayland::isAvailable()
{
    return true;
}

void AutoTypePlatformWayland::unload()
{
}

QString AutoTypePlatformWayland::activeWindowTitle()
{
    return {};
}

WId AutoTypePlatformWayland::activeWindow()
{
    return 0;
}

AutoTypeExecutor* AutoTypePlatformWayland::createExecutor()
{
    return new AutoTypeExecutorWayland(this);
}

bool AutoTypePlatformWayland::raiseWindow(WId window)
{
    Q_UNUSED(window)
    return false;
}

QStringList AutoTypePlatformWayland::windowTitles()
{
    return {};
}

AutoTypeExecutorWayland::AutoTypeExecutorWayland(AutoTypePlatformWayland* platform)
    : m_platform(platform)
{
}

AutoTypeAction::Result AutoTypeExecutorWayland::execBegin(const AutoTypeBegin* action)
{
    Q_UNUSED(action)
    return AutoTypeAction::Result::Ok();
}

AutoTypeAction::Result AutoTypeExecutorWayland::execType(const AutoTypeKey* action)
{
    Q_UNUSED(action)

    QVector<xkb_keysym_t> modifiers{};

    if (action->modifiers.testFlag(Qt::ShiftModifier)) {
        modifiers.append(XKB_KEY_Shift_L);
    }
    if (action->modifiers.testFlag(Qt::ControlModifier)) {
        modifiers.append(XKB_KEY_Control_L);
    }
    if (action->modifiers.testFlag(Qt::AltModifier)) {
        modifiers.append(XKB_KEY_Alt_L);
    }
    if (action->modifiers.testFlag(Qt::MetaModifier)) {
        modifiers.append(XKB_KEY_Meta_L);
    }

    // TODO: Replace these with proper lookups to xkbcommon keysyms instead of just reusing the X11 ones
    // They're mostly the same for most things, but strictly speaking differ slightly
    if (action->key != Qt::Key_unknown) {
        m_platform->sendKey(qtToNativeKeyCode(action->key), modifiers);
    } else {
        m_platform->sendKey(qcharToNativeKeyCode(action->character), modifiers);
    }

    Tools::sleep(execDelayMs);

    return AutoTypeAction::Result::Ok();
}

AutoTypeAction::Result AutoTypeExecutorWayland::execClearField(const AutoTypeClearField* action)
{
    Q_UNUSED(action)
    execType(new AutoTypeKey(Qt::Key_Home));
    execType(new AutoTypeKey(Qt::Key_End, Qt::ShiftModifier));
    execType(new AutoTypeKey(Qt::Key_Backspace));

    return AutoTypeAction::Result::Ok();
}
