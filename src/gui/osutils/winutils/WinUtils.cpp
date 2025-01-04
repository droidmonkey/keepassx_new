/*
 * Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "WinUtils.h"

#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QWindow>

#include <Windows.h>
#undef MessageBox

QPointer<WinUtils> WinUtils::m_instance = nullptr;

WinUtils* WinUtils::instance()
{
    if (!m_instance) {
        m_instance = new WinUtils(qApp);
        m_instance->m_darkAppThemeActive = m_instance->isDarkMode();
        m_instance->m_darkSystemThemeActive = m_instance->isStatusBarDark();

#ifdef QT_DEBUG
        // Attach console to enable debug output
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
        }
#endif
    }

    return m_instance;
}

WinUtils::WinUtils(QObject* parent)
    : OSUtilsBase(parent)
{
}

bool WinUtils::canPreventScreenCapture() const
{
    return true;
}

bool WinUtils::setPreventScreenCapture(QWindow* window, bool prevent) const
{
    bool ret = true;
    if (window) {
        HWND handle = reinterpret_cast<HWND>(window->winId());
        ret = SetWindowDisplayAffinity(handle, prevent ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
    }
    return ret;
}

/**
 * Register event filters to handle native platform events such as global hotkeys
 */
void WinUtils::registerNativeEventFilter()
{
    qApp->installNativeEventFilter(this);
}

bool WinUtils::nativeEventFilter(const QByteArray& eventType, void* message, long*)
{
    if (eventType != "windows_generic_MSG") {
        return false;
    }

    auto* msg = static_cast<MSG*>(message);
    switch (msg->message) {
    case WM_SETTINGCHANGE:
        if (m_darkAppThemeActive != isDarkMode()) {
            m_darkAppThemeActive = !m_darkAppThemeActive;
            emit interfaceThemeChanged();
        }

        if (m_darkSystemThemeActive != isStatusBarDark()) {
            m_darkSystemThemeActive = !m_darkSystemThemeActive;
            emit statusbarThemeChanged();
        }
        break;
    case WM_HOTKEY:
        triggerGlobalShortcut(msg->wParam);
        break;
    }

    return false;
}

bool WinUtils::isDarkMode() const
{
    QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
                       QSettings::NativeFormat);
    return settings.value("AppsUseLightTheme", 1).toInt() == 0;
}

bool WinUtils::isStatusBarDark() const
{
    QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
                       QSettings::NativeFormat);
    return settings.value("SystemUsesLightTheme", 0).toInt() == 0;
}

bool WinUtils::isLaunchAtStartupEnabled() const
{
    return QSettings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)", QSettings::NativeFormat)
        .contains(qAppName());
}

void WinUtils::setLaunchAtStartup(bool enable)
{
    QSettings reg(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)", QSettings::NativeFormat);
    if (enable) {
        reg.setValue(qAppName(), QString("\"%1\"").arg(QDir::toNativeSeparators(QApplication::applicationFilePath())));
    } else {
        reg.remove(qAppName());
    }
}

bool WinUtils::isCapslockEnabled()
{
    return GetKeyState(VK_CAPITAL) == 1;
}

void WinUtils::setUserInputProtection(bool enable)
{
    // Windows does not support this feature
    Q_UNUSED(enable)
}

bool WinUtils::isHighContrastMode() const
{
    QSettings settings(R"(HKEY_CURRENT_USER\Control Panel\Accessibility\HighContrast)", QSettings::NativeFormat);
    return (settings.value("Flags").toInt() & 1u) != 0;
}

bool WinUtils::registerGlobalShortcut(const QString& name, Qt::Key key, Qt::KeyboardModifiers modifiers, QString* error)
{
    auto keycode = qtToNativeKeyCode(key);
    auto modifierscode = qtToNativeModifiers(modifiers);
    if (keycode < 1 || keycode > 254) {
        if (error) {
            *error = tr("Invalid key code");
        }
        return false;
    }

    // Check if this key combo is registered to another shortcut
    QHashIterator<QString, QSharedPointer<globalShortcut>> i(m_globalShortcuts);
    while (i.hasNext()) {
        i.next();
        if (i.value()->nativeKeyCode == keycode && i.value()->nativeModifiers == modifierscode && i.key() != name) {
            if (error) {
                *error = tr("Global shortcut already registered to %1").arg(i.key());
            }
            return false;
        }
    }

    unregisterGlobalShortcut(name);

    auto gs = QSharedPointer<globalShortcut>::create();
    gs->id = m_nextShortcutId;
    gs->nativeKeyCode = keycode;
    gs->nativeModifiers = modifierscode;
    if (!::RegisterHotKey(nullptr, gs->id, gs->nativeModifiers | MOD_NOREPEAT, gs->nativeKeyCode)) {
        if (error) {
            *error = tr("Could not register global shortcut");
        }
        return false;
    }

    m_globalShortcuts.insert(name, gs);

    if (++m_nextShortcutId > 0xBFFF) {
        // Roll over if greater than the max id per
        // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey#remarks
        m_nextShortcutId = 1;
    }
    return true;
}

bool WinUtils::unregisterGlobalShortcut(const QString& name)
{
    if (m_globalShortcuts.contains(name)) {
        auto gs = m_globalShortcuts.value(name);
        if (::UnregisterHotKey(nullptr, gs->id)) {
            m_globalShortcuts.remove(name);
            return true;
        }
    }
    return false;
}

void WinUtils::triggerGlobalShortcut(int id)
{
    QHashIterator<QString, QSharedPointer<globalShortcut>> i(m_globalShortcuts);
    while (i.hasNext()) {
        i.next();
        if (i.value()->id == id) {
            emit globalShortcutTriggered(i.key());
            break;
        }
    }
}

// clang-format off
//
// Translate qt key code to windows virtual key code
// see: https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx
//
WORD WinUtils::qtToNativeKeyCode(Qt::Key key)
{
    switch (key) {
    case Qt::Key_Backspace:
        return VK_BACK;     // 0x08
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return VK_TAB;      // 0x09
    case Qt::Key_Clear:
        return VK_CLEAR;    // 0x0C
    case Qt::Key_Enter:
    case Qt::Key_Return:
        return VK_RETURN;   // 0x0D
    case Qt::Key_Shift:
        return VK_SHIFT;    // 0x10
    case Qt::Key_Control:
        return VK_CONTROL;  // 0x11
    case Qt::Key_Alt:
        return VK_MENU;     // 0x12
    case Qt::Key_Pause:
        return VK_PAUSE;    // 0x13
    case Qt::Key_CapsLock:
        return VK_CAPITAL;  // 0x14
    case Qt::Key_Escape:
        return VK_ESCAPE;   // 0x1B
    case Qt::Key_Space:
        return VK_SPACE;    // 0x20
    case Qt::Key_PageUp:
        return VK_PRIOR;    // 0x21
    case Qt::Key_PageDown:
        return VK_NEXT;     // 0x22
    case Qt::Key_End:
        return VK_END;      // 0x23
    case Qt::Key_Home:
        return VK_HOME;     // 0x24
    case Qt::Key_Left:
        return VK_LEFT;     // 0x25
    case Qt::Key_Up:
        return VK_UP;       // 0x26
    case Qt::Key_Right:
        return VK_RIGHT;    // 0x27
    case Qt::Key_Down:
        return VK_DOWN;     // 0x28
    case Qt::Key_Print:
        return VK_SNAPSHOT; // 0x2C
    case Qt::Key_Insert:
        return VK_INSERT;   // 0x2D
    case Qt::Key_Delete:
        return VK_DELETE;   // 0x2E
    case Qt::Key_Help:
        return VK_HELP;     // 0x2F
    case Qt::Key_Meta:
        return VK_LWIN;     // 0x5B
    case Qt::Key_AltGr:
        return VK_RMENU;    // 0xA5

    case Qt::Key_F1:
        return VK_F1;       // 0x70
    case Qt::Key_F2:
        return VK_F2;       // 0x71
    case Qt::Key_F3:
        return VK_F3;       // 0x72
    case Qt::Key_F4:
        return VK_F4;       // 0x73
    case Qt::Key_F5:
        return VK_F5;       // 0x74
    case Qt::Key_F6:
        return VK_F6;       // 0x75
    case Qt::Key_F7:
        return VK_F7;       // 0x76
    case Qt::Key_F8:
        return VK_F8;       // 0x77
    case Qt::Key_F9:
        return VK_F9;       // 0x78
    case Qt::Key_F10:
        return VK_F10;      // 0x79
    case Qt::Key_F11:
        return VK_F11;      // 0x7A
    case Qt::Key_F12:
        return VK_F12;      // 0x7B
    case Qt::Key_F13:
        return VK_F13;      // 0x7C
    case Qt::Key_F14:
        return VK_F14;      // 0x7D
    case Qt::Key_F15:
        return VK_F15;      // 0x7E
    case Qt::Key_F16:
        return VK_F16;      // 0x7F
    case Qt::Key_F17:
        return VK_F17;      // 0x80
    case Qt::Key_F18:
        return VK_F18;      // 0x81
    case Qt::Key_F19:
        return VK_F19;      // 0x82
    case Qt::Key_F20:
        return VK_F20;      // 0x83
    case Qt::Key_F21:
        return VK_F21;      // 0x84
    case Qt::Key_F22:
        return VK_F22;      // 0x85
    case Qt::Key_F23:
        return VK_F23;      // 0x86
    case Qt::Key_F24:
        return VK_F24;      // 0x87

    case Qt::Key_NumLock:
        return VK_NUMLOCK;  // 0x90
    case Qt::Key_ScrollLock:
        return VK_SCROLL;   // 0x91

    default:
        return LOBYTE(::VkKeyScanExW(key, ::GetKeyboardLayout(0)));
    }
}
// clang-format on

//
// Translate qt key modifiers to windows modifiers
//
DWORD WinUtils::qtToNativeModifiers(Qt::KeyboardModifiers modifiers)
{
    DWORD nativeModifiers = 0;

    if (modifiers & Qt::ShiftModifier) {
        nativeModifiers |= MOD_SHIFT;
    }
    if (modifiers & Qt::ControlModifier) {
        nativeModifiers |= MOD_CONTROL;
    }
    if (modifiers & Qt::AltModifier) {
        nativeModifiers |= MOD_ALT;
    }
    if (modifiers & Qt::MetaModifier) {
        nativeModifiers |= MOD_WIN;
    }

    return nativeModifiers;
}
