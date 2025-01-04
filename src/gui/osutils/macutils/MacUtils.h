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

#ifndef KEEPASSXC_MACUTILS_H
#define KEEPASSXC_MACUTILS_H

#include "AppKit.h"
#include "gui/osutils/OSUtilsBase.h"
#include <Carbon/Carbon.h>

#include <QColor>
#include <QPointer>
#include <QScopedPointer>
#include <qwindowdefs.h>

class MacUtils : public OSUtilsBase
{
    Q_OBJECT

public:
    static MacUtils* instance();

    bool isDarkMode() const override;
    bool isStatusBarDark() const override;
    bool isLaunchAtStartupEnabled() const override;
    void setLaunchAtStartup(bool enable) override;
    bool isCapslockEnabled() override;
    void setUserInputProtection(bool enable) override;

    WId activeWindow();
    bool raiseWindow(WId pid);
    bool raiseLastActiveWindow();
    bool raiseOwnWindow();
    bool hideOwnWindow();
    bool isHidden();
    bool enableAccessibility();
    bool enableScreenRecording();
    void toggleForegroundApp(bool foreground);

    void registerNativeEventFilter() override;

    bool registerGlobalShortcut(const QString& name,
                                Qt::Key key,
                                Qt::KeyboardModifiers modifiers,
                                QString* error = nullptr) override;
    bool unregisterGlobalShortcut(const QString& name) override;

    uint16 qtToNativeKeyCode(Qt::Key key);
    CGEventFlags qtToNativeModifiers(Qt::KeyboardModifiers modifiers, bool native);

    bool canPreventScreenCapture() const override;
    bool setPreventScreenCapture(QWindow* window, bool prevent) const override;

signals:
    void userSwitched();

protected:
    explicit MacUtils(QObject* parent = nullptr);
    ~MacUtils() override;

private:
    QString getLaunchAgentFilename() const;
    static OSStatus hotkeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void* userData);

    QScopedPointer<AppKit> m_appkit;
    static QPointer<MacUtils> m_instance;

    struct globalShortcut
    {
        EventHotKeyRef hotkeyRef;
        EventHotKeyID hotkeyId;
        uint16 nativeKeyCode;
        CGEventFlags nativeModifiers;
    };

    int m_nextShortcutId = 1;
    QHash<QString, QSharedPointer<globalShortcut>> m_globalShortcuts;

    Q_DISABLE_COPY(MacUtils)
};

inline MacUtils* macUtils()
{
    return MacUtils::instance();
}

#endif // KEEPASSXC_MACUTILS_H
