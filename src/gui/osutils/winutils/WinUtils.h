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

#ifndef KEEPASSXC_WINUTILS_H
#define KEEPASSXC_WINUTILS_H

#include "gui/osutils/OSUtilsBase.h"

#include <QAbstractNativeEventFilter>
#include <QPointer>
#include <QScopedPointer>
#include <QSharedPointer>

#undef NOMINMAX
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#undef MessageBox

class WinUtils : public OSUtilsBase, QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    static WinUtils* instance();

    bool isDarkMode() const override;
    bool isStatusBarDark() const override;
    bool isLaunchAtStartupEnabled() const override;
    void setLaunchAtStartup(bool enable) override;
    bool isCapslockEnabled() override;
    void setUserInputProtection(bool enable) override;
    bool isHighContrastMode() const;

    void registerNativeEventFilter() override;

    bool registerGlobalShortcut(const QString& name,
                                Qt::Key key,
                                Qt::KeyboardModifiers modifiers,
                                QString* error = nullptr) override;
    bool unregisterGlobalShortcut(const QString& name) override;

    WORD qtToNativeKeyCode(Qt::Key key);
    DWORD qtToNativeModifiers(Qt::KeyboardModifiers modifiers);

    bool canPreventScreenCapture() const override;
    bool setPreventScreenCapture(QWindow* window, bool prevent) const override;

protected:
    explicit WinUtils(QObject* parent = nullptr);
    ~WinUtils() override = default;

    bool nativeEventFilter(const QByteArray& eventType, void* message, long*) override;
    void triggerGlobalShortcut(int id);

private:
    static QPointer<WinUtils> m_instance;

    struct globalShortcut
    {
        int id;
        DWORD nativeKeyCode;
        DWORD nativeModifiers;
    };

    int m_nextShortcutId = 1;
    QHash<QString, QSharedPointer<globalShortcut>> m_globalShortcuts;

    bool m_darkAppThemeActive;
    bool m_darkSystemThemeActive;

    Q_DISABLE_COPY(WinUtils)
};

inline WinUtils* winUtils()
{
    return WinUtils::instance();
}

#endif // KEEPASSXC_WINUTILS_H
