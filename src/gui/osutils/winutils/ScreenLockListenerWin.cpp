/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#include "ScreenLockListenerWin.h"
#include <QApplication>
#include <windows.h>
#include <wtsapi32.h>

/*
 * See https://msdn.microsoft.com/en-us/library/windows/desktop/aa373196(v=vs.85).aspx
 * See https://msdn.microsoft.com/en-us/library/aa383841(v=vs.85).aspx
 * See https://blogs.msdn.microsoft.com/oldnewthing/20060104-50/?p=32783
 */
ScreenLockListenerWin::ScreenLockListenerWin(QWidget* parent)
    : ScreenLockListenerPrivate(parent)
    , QAbstractNativeEventFilter()
{
    Q_ASSERT(parent != nullptr);
    // On windows, we need to register for platform specific messages and
    // install a message handler for them
    QCoreApplication::instance()->installNativeEventFilter(this);

    // This call requests a notification from windows when a laptop is closed
    HPOWERNOTIFY hPnotify = RegisterPowerSettingNotification(
        reinterpret_cast<HWND>(parent->winId()), &GUID_LIDSWITCH_STATE_CHANGE, DEVICE_NOTIFY_WINDOW_HANDLE);
    m_powerNotificationHandle = reinterpret_cast<void*>(hPnotify);

    // This call requests a notification for session changes
    if (!WTSRegisterSessionNotification(reinterpret_cast<HWND>(parent->winId()), NOTIFY_FOR_THIS_SESSION)) {
    }
}

ScreenLockListenerWin::~ScreenLockListenerWin()
{
    HWND h = reinterpret_cast<HWND>(static_cast<QWidget*>(parent())->winId());
    WTSUnRegisterSessionNotification(h);

    if (m_powerNotificationHandle) {
        UnregisterPowerSettingNotification(reinterpret_cast<HPOWERNOTIFY>(m_powerNotificationHandle));
    }
}

bool ScreenLockListenerWin::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(result);
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG* m = static_cast<MSG*>(message);
        if (m->message == WM_POWERBROADCAST) {
            if (m->wParam == PBT_POWERSETTINGCHANGE) {
                const POWERBROADCAST_SETTING* setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(m->lParam);
                if (setting != nullptr && setting->PowerSetting == GUID_LIDSWITCH_STATE_CHANGE) {
                    const DWORD* state = reinterpret_cast<const DWORD*>(&setting->Data);
                    if (*state == 0) {
                        emit screenLocked();
                        return true;
                    }
                }
            } else if (m->wParam == PBT_APMSUSPEND) {
                emit screenLocked();
                return true;
            }
        }
        if (m->message == WM_WTSSESSION_CHANGE) {
            if (m->wParam == WTS_CONSOLE_DISCONNECT) {
                emit screenLocked();
                return true;
            }
            if (m->wParam == WTS_SESSION_LOCK) {
                emit screenLocked();
                return true;
            }
        }
    }
    return false;
}
