/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2000-2008 Tom Sato <VEF00200@nifty.ne.jp>
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

#ifndef KEEPASSXC_AUTOTYPEXCB_H
#define KEEPASSXC_AUTOTYPEXCB_H

#include <QApplication>
#include <QSet>
#include <QWidget>
#include <QtPlugin>

#include "autotype/AutoTypePlatformPlugin.h"

#include <X11/XKBlib.h>

#define N_MOD_INDICES (Mod5MapIndex + 1)

class AutoTypePlatformX11 : public QObject, public AutoTypePlatformInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.keepassx.AutoTypePlatformX11")
    Q_INTERFACES(AutoTypePlatformInterface)

public:
    AutoTypePlatformX11();
    bool isAvailable() override;
    void unload() override;
    QStringList windowTitles() override;
    WId activeWindow() override;
    QString activeWindowTitle() override;
    bool raiseWindow(WId window) override;
    AutoTypeExecutor* createExecutor() override;
    void updateKeymap();

    AutoTypeAction::Result sendKey(KeySym keysym, unsigned int modifiers = Qt::NoModifier);

private:
    QString windowTitle(Window window, bool useBlacklist);
    QStringList windowTitlesRecursive(Window window);
    QString windowClassName(Window window);
    QList<Window> widgetsToX11Windows(const QWidgetList& widgetList);
    bool isTopLevelWindow(Window window);

    XkbDescPtr getKeyboard();
    bool RemapKeycode(KeySym keysym);
    void SendKeyEvent(unsigned keycode, bool press);
    void SendModifiers(unsigned int mask, bool press);
    bool GetKeycode(KeySym keysym, int* keycode, int* group, unsigned int* mask, bool* repeat);

    static int MyErrorHandler(Display* my_dpy, XErrorEvent* event);

    Display* m_dpy;
    Window m_rootWindow;
    Atom m_atomWmState;
    Atom m_atomWmName;
    Atom m_atomNetWmName;
    Atom m_atomString;
    Atom m_atomUtf8String;
    Atom m_atomNetActiveWindow;
    Atom m_atomTransientFor;
    Atom m_atomWindow;
    QSet<QString> m_classBlacklist;

    typedef struct
    {
        KeySym sym;
        int code;
        int group;
        int mask;
    } KeyDesc;

    XkbDescPtr m_xkb;
    QList<KeyDesc> m_keymap;
    KeyCode m_modifier_keycode[N_MOD_INDICES];
    KeyCode m_remapKeycode;
    bool m_loaded;
};

class AutoTypeExecutorX11 : public AutoTypeExecutor
{
public:
    explicit AutoTypeExecutorX11(AutoTypePlatformX11* platform);

    AutoTypeAction::Result execBegin(const AutoTypeBegin* action) override;
    AutoTypeAction::Result execType(const AutoTypeKey* action) override;
    AutoTypeAction::Result execClearField(const AutoTypeClearField* action) override;

private:
    AutoTypePlatformX11* const m_platform;
};

#endif // KEEPASSXC_AUTOTYPEXCB_H
