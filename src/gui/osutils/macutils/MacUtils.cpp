/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "MacUtils.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>

#include <ApplicationServices/ApplicationServices.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
#include <CoreGraphics/CGEventSource.h>
#endif

#define INVALID_KEYCODE 0xFFFF

QPointer<MacUtils> MacUtils::m_instance = nullptr;

MacUtils::MacUtils(QObject* parent)
    : OSUtilsBase(parent)
    , m_appkit(new AppKit())
{
    connect(m_appkit.data(), SIGNAL(userSwitched()), SIGNAL(userSwitched()));
    connect(m_appkit.data(), SIGNAL(interfaceThemeChanged()), SIGNAL(interfaceThemeChanged()));
    connect(m_appkit.data(), &AppKit::interfaceThemeChanged, this, [this]() {
        // Emit with delay, since isStatusBarDark() still returns the old value
        // if we call it too fast after a theme change.
        QTimer::singleShot(100, [this]() { emit statusbarThemeChanged(); });
    });
}

MacUtils::~MacUtils()
{
}

MacUtils* MacUtils::instance()
{
    if (!m_instance) {
        m_instance = new MacUtils(qApp);
    }

    return m_instance;
}

WId MacUtils::activeWindow()
{
    return m_appkit->activeProcessId();
}

bool MacUtils::raiseWindow(WId pid)
{
    return m_appkit->activateProcess(pid);
}

bool MacUtils::raiseOwnWindow()
{
    m_appkit->toggleForegroundApp(true);
    return m_appkit->activateProcess(m_appkit->ownProcessId());
}

bool MacUtils::raiseLastActiveWindow()
{
    return m_appkit->activateProcess(m_appkit->lastActiveProcessId());
}

bool MacUtils::hideOwnWindow()
{
    return m_appkit->hideProcess(m_appkit->ownProcessId());
}

bool MacUtils::isHidden()
{
    return m_appkit->isHidden(m_appkit->ownProcessId());
}

bool MacUtils::enableAccessibility()
{
    return m_appkit->enableAccessibility();
}

bool MacUtils::enableScreenRecording()
{
    return m_appkit->enableScreenRecording();
}

bool MacUtils::isDarkMode() const
{
    return m_appkit->isDarkMode();
}

bool MacUtils::isStatusBarDark() const
{
    return m_appkit->isStatusBarDark();
}

QString MacUtils::getLaunchAgentFilename() const
{
    auto launchAgentDir =
        QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/../LaunchAgents"));
    return QFile(launchAgentDir.absoluteFilePath(qApp->property("KPXC_QUALIFIED_APPNAME").toString().append(".plist")))
        .fileName();
}

bool MacUtils::isLaunchAtStartupEnabled() const
{
    return QFile::exists(getLaunchAgentFilename());
}

void MacUtils::setLaunchAtStartup(bool enable)
{
    if (enable) {
        QSettings agent(getLaunchAgentFilename(), QSettings::NativeFormat);
        agent.setValue("Label", qApp->property("KPXC_QUALIFIED_APPNAME").toString());
        agent.setValue("AssociatedBundleIdentifiers", qApp->property("KPXC_QUALIFIED_APPNAME").toString());
        agent.setValue("Program", QApplication::applicationFilePath());
        agent.setValue("ProgramArguments", QStringList() << QApplication::applicationFilePath());
        agent.setValue("RunAtLoad", true);
        agent.setValue("StandardErrorPath", "/dev/null");
        agent.setValue("StandardOutPath", "/dev/null");
    } else if (isLaunchAtStartupEnabled()) {
        QFile::remove(getLaunchAgentFilename());
    }
}

bool MacUtils::isCapslockEnabled()
{
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    return (CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState) & kCGEventFlagMaskAlphaShift) != 0;
#else
    return false;
#endif
}

void MacUtils::setUserInputProtection(bool enable)
{
    if (enable) {
        EnableSecureEventInput();
    } else {
        DisableSecureEventInput();
    }
}

/**
 * Toggle application state between foreground app and UIElement app.
 * Foreground apps have dock icons, UIElement apps do not.
 *
 * @param foreground whether app is foreground app
 */
void MacUtils::toggleForegroundApp(bool foreground)
{
    m_appkit->toggleForegroundApp(foreground);
}

bool MacUtils::canPreventScreenCapture() const
{
    return true;
}

bool MacUtils::setPreventScreenCapture(QWindow* window, bool prevent) const
{
    if (window) {
        m_appkit->setWindowSecurity(window, prevent);
    }
    return true;
}

void MacUtils::registerNativeEventFilter()
{
    EventTypeSpec eventSpec;
    eventSpec.eventClass = kEventClassKeyboard;
    eventSpec.eventKind = kEventHotKeyPressed;
    ::InstallApplicationEventHandler(MacUtils::hotkeyHandler, 1, &eventSpec, this, nullptr);
}

bool MacUtils::registerGlobalShortcut(const QString& name, Qt::Key key, Qt::KeyboardModifiers modifiers, QString* error)
{
    auto keycode = qtToNativeKeyCode(key);
    if (keycode == INVALID_KEYCODE) {
        if (error) {
            *error = tr("Invalid key code");
        }
        return false;
    }

    // Qt inverts CMD and CTRL on macOS under the hood, undo this
    if (modifiers & Qt::MetaModifier && !(modifiers & Qt::ControlModifier)) {
        modifiers &= ~Qt::MetaModifier;
        modifiers |= Qt::ControlModifier;
    } else if (modifiers & Qt::ControlModifier && !(modifiers & Qt::MetaModifier)) {
        modifiers &= ~Qt::ControlModifier;
        modifiers |= Qt::MetaModifier;
    }
    auto modifierscode = qtToNativeModifiers(modifiers, false);

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

    // Remove existing registration for this name
    unregisterGlobalShortcut(name);

    auto gs = QSharedPointer<globalShortcut>::create();
    gs->hotkeyId.signature = 'kpxc';
    gs->hotkeyId.id = m_nextShortcutId;
    gs->nativeKeyCode = keycode;
    gs->nativeModifiers = modifierscode;
    if (::RegisterEventHotKey(
            gs->nativeKeyCode, gs->nativeModifiers, gs->hotkeyId, GetApplicationEventTarget(), 0, &gs->hotkeyRef)
        != noErr) {
        if (error) {
            *error = tr("Could not register global shortcut");
        }
        return false;
    }

    m_globalShortcuts.insert(name, gs);
    ++m_nextShortcutId;

    return true;
}

bool MacUtils::unregisterGlobalShortcut(const QString& name)
{
    if (m_globalShortcuts.contains(name)) {
        auto gs = m_globalShortcuts.value(name);
        ::UnregisterEventHotKey(gs->hotkeyRef);
        m_globalShortcuts.remove(name);
        return true;
    }
    return false;
}

OSStatus MacUtils::hotkeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void* userData)
{
    Q_UNUSED(nextHandler);

    auto self = static_cast<MacUtils*>(userData);
    EventHotKeyID hotkeyId;
    if (::GetEventParameter(
            theEvent, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(hotkeyId), nullptr, &hotkeyId)
        == noErr) {
        QHashIterator<QString, QSharedPointer<globalShortcut>> i(self->m_globalShortcuts);
        while (i.hasNext()) {
            i.next();
            if (i.value()->hotkeyId.id == hotkeyId.id) {
                emit self->globalShortcutTriggered(i.key());
                return noErr;
            }
        }
    }

    return eventNotHandledErr;
}

//
// Translate qt key code to mac os key code
// see: HIToolbox/Events.h
//
uint16 MacUtils::qtToNativeKeyCode(Qt::Key key)
{
    switch (key) {
    case Qt::Key_A:
        return kVK_ANSI_A;
    case Qt::Key_B:
        return kVK_ANSI_B;
    case Qt::Key_C:
        return kVK_ANSI_C;
    case Qt::Key_D:
        return kVK_ANSI_D;
    case Qt::Key_E:
        return kVK_ANSI_E;
    case Qt::Key_F:
        return kVK_ANSI_F;
    case Qt::Key_G:
        return kVK_ANSI_G;
    case Qt::Key_H:
        return kVK_ANSI_H;
    case Qt::Key_I:
        return kVK_ANSI_I;
    case Qt::Key_J:
        return kVK_ANSI_J;
    case Qt::Key_K:
        return kVK_ANSI_K;
    case Qt::Key_L:
        return kVK_ANSI_L;
    case Qt::Key_M:
        return kVK_ANSI_M;
    case Qt::Key_N:
        return kVK_ANSI_N;
    case Qt::Key_O:
        return kVK_ANSI_O;
    case Qt::Key_P:
        return kVK_ANSI_P;
    case Qt::Key_Q:
        return kVK_ANSI_Q;
    case Qt::Key_R:
        return kVK_ANSI_R;
    case Qt::Key_S:
        return kVK_ANSI_S;
    case Qt::Key_T:
        return kVK_ANSI_T;
    case Qt::Key_U:
        return kVK_ANSI_U;
    case Qt::Key_V:
        return kVK_ANSI_V;
    case Qt::Key_W:
        return kVK_ANSI_W;
    case Qt::Key_X:
        return kVK_ANSI_X;
    case Qt::Key_Y:
        return kVK_ANSI_Y;
    case Qt::Key_Z:
        return kVK_ANSI_Z;

    case Qt::Key_0:
        return kVK_ANSI_0;
    case Qt::Key_1:
        return kVK_ANSI_1;
    case Qt::Key_2:
        return kVK_ANSI_2;
    case Qt::Key_3:
        return kVK_ANSI_3;
    case Qt::Key_4:
        return kVK_ANSI_4;
    case Qt::Key_5:
        return kVK_ANSI_5;
    case Qt::Key_6:
        return kVK_ANSI_6;
    case Qt::Key_7:
        return kVK_ANSI_7;
    case Qt::Key_8:
        return kVK_ANSI_8;
    case Qt::Key_9:
        return kVK_ANSI_9;

    case Qt::Key_Equal:
        return kVK_ANSI_Equal;
    case Qt::Key_Minus:
        return kVK_ANSI_Minus;
    case Qt::Key_BracketRight:
        return kVK_ANSI_RightBracket;
    case Qt::Key_BracketLeft:
        return kVK_ANSI_LeftBracket;
    case Qt::Key_QuoteDbl:
        return kVK_ANSI_Quote;
    case Qt::Key_Semicolon:
        return kVK_ANSI_Semicolon;
    case Qt::Key_Backslash:
        return kVK_ANSI_Backslash;
    case Qt::Key_Comma:
        return kVK_ANSI_Comma;
    case Qt::Key_Slash:
        return kVK_ANSI_Slash;
    case Qt::Key_Period:
        return kVK_ANSI_Period;

    case Qt::Key_Shift:
        return kVK_Shift;
    case Qt::Key_Control:
        return kVK_Control;
    case Qt::Key_Alt:
        return kVK_Option;
    case Qt::Key_Meta:
        return kVK_Command;
    case Qt::Key_Backspace:
        return kVK_Delete;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return kVK_Tab;
    case Qt::Key_Enter:
    case Qt::Key_Return:
        return kVK_Return;
    case Qt::Key_CapsLock:
        return kVK_CapsLock;
    case Qt::Key_Escape:
        return kVK_Escape;
    case Qt::Key_Space:
        return kVK_Space;
    case Qt::Key_PageUp:
        return kVK_PageUp;
    case Qt::Key_PageDown:
        return kVK_PageDown;
    case Qt::Key_End:
        return kVK_End;
    case Qt::Key_Home:
        return kVK_Home;
    case Qt::Key_Left:
        return kVK_LeftArrow;
    case Qt::Key_Up:
        return kVK_UpArrow;
    case Qt::Key_Right:
        return kVK_RightArrow;
    case Qt::Key_Down:
        return kVK_DownArrow;
    case Qt::Key_Delete:
        return kVK_ForwardDelete;
    case Qt::Key_Help:
        return kVK_Help;

    case Qt::Key_F1:
        return kVK_F1;
    case Qt::Key_F2:
        return kVK_F2;
    case Qt::Key_F3:
        return kVK_F3;
    case Qt::Key_F4:
        return kVK_F4;
    case Qt::Key_F5:
        return kVK_F5;
    case Qt::Key_F6:
        return kVK_F6;
    case Qt::Key_F7:
        return kVK_F7;
    case Qt::Key_F8:
        return kVK_F8;
    case Qt::Key_F9:
        return kVK_F9;
    case Qt::Key_F10:
        return kVK_F10;
    case Qt::Key_F11:
        return kVK_F11;
    case Qt::Key_F12:
        return kVK_F12;
    case Qt::Key_F13:
        return kVK_F13;
    case Qt::Key_F14:
        return kVK_F14;
    case Qt::Key_F15:
        return kVK_F15;
    case Qt::Key_F16:
        return kVK_F16;

    default:
        return INVALID_KEYCODE;
    }
}

//
// Translate qt key modifiers to mac os modifiers
// see: https://doc.qt.io/qt-5/osx-issues.html#special-keys
//
CGEventFlags MacUtils::qtToNativeModifiers(Qt::KeyboardModifiers modifiers, bool native)
{
    CGEventFlags nativeModifiers = CGEventFlags(0);

    CGEventFlags shiftMod = CGEventFlags(shiftKey);
    CGEventFlags cmdMod = CGEventFlags(cmdKey);
    CGEventFlags optionMod = CGEventFlags(optionKey);
    CGEventFlags controlMod = CGEventFlags(controlKey);

    if (native) {
        shiftMod = kCGEventFlagMaskShift;
        cmdMod = kCGEventFlagMaskCommand;
        optionMod = kCGEventFlagMaskAlternate;
        controlMod = kCGEventFlagMaskControl;
    }

    if (modifiers & Qt::ShiftModifier) {
        nativeModifiers = CGEventFlags(nativeModifiers | shiftMod);
    }
    if (modifiers & Qt::ControlModifier) {
        nativeModifiers = CGEventFlags(nativeModifiers | controlMod);
    }
    if (modifiers & Qt::AltModifier) {
        nativeModifiers = CGEventFlags(nativeModifiers | optionMod);
    }
    if (modifiers & Qt::MetaModifier) {
        nativeModifiers = CGEventFlags(nativeModifiers | cmdMod);
    }

    return nativeModifiers;
}
