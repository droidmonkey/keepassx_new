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

#include "AutoTypeXCB.h"
#include "core/Tools.h"
#include "gui/osutils/nixutils/X11Funcs.h"

#include <QGuiApplication>
#include <private/qtx11extras_p.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

/* map of ASCII non-dead keys to equivalent dead keys that need to be repeated */
static const QPair<KeySym, KeySym> deadMap[] = {
    {XK_acute, XK_dead_acute},
    {XK_grave, XK_dead_grave},
    {XK_asciicircum, XK_dead_circumflex},
    {XK_asciitilde, XK_dead_tilde},
    {XK_asciitilde, XK_dead_perispomeni},
};

AutoTypePlatformX11::AutoTypePlatformX11()
{
    // Qt handles XCB slightly differently so we open our own connection
    if (auto* native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
        m_dpy = XOpenDisplay(XDisplayString(native->display()));
        m_rootWindow = DefaultRootWindow(native->display());
    }

    m_atomWmState = XInternAtom(m_dpy, "WM_STATE", True);
    m_atomWmName = XInternAtom(m_dpy, "WM_NAME", True);
    m_atomNetWmName = XInternAtom(m_dpy, "_NET_WM_NAME", True);
    m_atomString = XInternAtom(m_dpy, "STRING", True);
    m_atomUtf8String = XInternAtom(m_dpy, "UTF8_STRING", True);
    m_atomNetActiveWindow = XInternAtom(m_dpy, "_NET_ACTIVE_WINDOW", True);
    m_atomTransientFor = XInternAtom(m_dpy, "WM_TRANSIENT_FOR", True);
    m_atomWindow = XInternAtom(m_dpy, "WINDOW", True);

    m_classBlacklist << "desktop_window" << "gnome-panel"; // Gnome
    m_classBlacklist << "kdesktop" << "kicker"; // KDE 3
    m_classBlacklist << "Plasma"; // KDE 4
    m_classBlacklist << "plasmashell"; // KDE 5
    m_classBlacklist << "xfdesktop" << "xfce4-panel"; // Xfce 4

    m_xkb = nullptr;

    m_loaded = true;
}

bool AutoTypePlatformX11::isAvailable()
{
    int ignore;

    if (!XQueryExtension(m_dpy, "XInputExtension", &ignore, &ignore, &ignore)) {
        return false;
    }

    if (!XQueryExtension(m_dpy, "XTEST", &ignore, &ignore, &ignore)) {
        return false;
    }

    return true;
}

void AutoTypePlatformX11::unload()
{
    m_keymap.clear();

    if (m_xkb) {
        XkbFreeKeyboard(m_xkb, XkbAllComponentsMask, True);
        m_xkb = nullptr;
    }

    XCloseDisplay(m_dpy);
    m_dpy = nullptr;

    m_loaded = false;
}

QStringList AutoTypePlatformX11::windowTitles()
{
    return windowTitlesRecursive(m_rootWindow);
}

WId AutoTypePlatformX11::activeWindow()
{
    Window window;
    int revert_to_return;
    XGetInputFocus(m_dpy, &window, &revert_to_return);

    int tree;
    do {
        if (isTopLevelWindow(window)) {
            break;
        }

        Window root;
        Window parent;
        Window* children = nullptr;
        unsigned int numChildren;
        tree = XQueryTree(m_dpy, window, &root, &parent, &children, &numChildren);
        window = parent;
        if (children) {
            XFree(children);
        }
    } while (tree && window);

    return window;
}

QString AutoTypePlatformX11::activeWindowTitle()
{
    return windowTitle(activeWindow(), true);
}

AutoTypeExecutor* AutoTypePlatformX11::createExecutor()
{
    return new AutoTypeExecutorX11(this);
}

QString AutoTypePlatformX11::windowTitle(Window window, bool useBlacklist)
{
    QString title;

    Atom type;
    int format;
    unsigned long nitems;
    unsigned long after;
    unsigned char* data = nullptr;

    // the window manager spec says we should read _NET_WM_NAME first, then fall back to WM_NAME

    int retVal = XGetWindowProperty(
        m_dpy, window, m_atomNetWmName, 0, 1000, False, m_atomUtf8String, &type, &format, &nitems, &after, &data);

    if ((retVal == 0) && data) {
        title = QString::fromUtf8(reinterpret_cast<char*>(data));
    } else {
        XTextProperty textProp;
        retVal = XGetTextProperty(m_dpy, window, &textProp, m_atomWmName);
        if ((retVal != 0) && textProp.value) {
            char** textList = nullptr;
            int count;

            if (textProp.encoding == m_atomUtf8String) {
                title = QString::fromUtf8(reinterpret_cast<char*>(textProp.value));
            } else if ((XmbTextPropertyToTextList(m_dpy, &textProp, &textList, &count) == 0) && textList
                       && (count > 0)) {
                title = QString::fromLocal8Bit(textList[0]);
            } else if (textProp.encoding == m_atomString) {
                title = QString::fromLocal8Bit(reinterpret_cast<char*>(textProp.value));
            }

            if (textList) {
                XFreeStringList(textList);
            }
        }

        if (textProp.value) {
            XFree(textProp.value);
        }
    }

    if (data) {
        XFree(data);
    }

    if (useBlacklist && !title.isEmpty()) {
        if (window == m_rootWindow) {
            return {};
        }

        QString className = windowClassName(window);
        if (m_classBlacklist.contains(className)) {
            return {};
        }

        QList<Window> keepassxWindows = widgetsToX11Windows(QApplication::topLevelWidgets());
        if (keepassxWindows.contains(window)) {
            return {};
        }
    }

    return title;
}

QString AutoTypePlatformX11::windowClassName(Window window)
{
    QString className;

    XClassHint wmClass;
    wmClass.res_name = nullptr;
    wmClass.res_class = nullptr;

    if (XGetClassHint(m_dpy, window, &wmClass) && wmClass.res_name) {
        className = QString::fromLocal8Bit(wmClass.res_name);
    }
    if (wmClass.res_name) {
        XFree(wmClass.res_name);
    }
    if (wmClass.res_class) {
        XFree(wmClass.res_class);
    }

    return className;
}

QList<Window> AutoTypePlatformX11::widgetsToX11Windows(const QWidgetList& widgetList)
{
    QList<Window> windows;

    for (const QWidget* widget : widgetList) {
        windows.append(widget->effectiveWinId());
    }

    return windows;
}

QStringList AutoTypePlatformX11::windowTitlesRecursive(Window window)
{
    QStringList titles;

    if (isTopLevelWindow(window)) {
        QString title = windowTitle(window, true);
        if (!title.isEmpty()) {
            titles.append(title);
        }
    }

    Window root;
    Window parent;
    Window* children = nullptr;
    unsigned int numChildren;
    if (XQueryTree(m_dpy, window, &root, &parent, &children, &numChildren) && children) {
        for (uint i = 0; i < numChildren; i++) {
            titles.append(windowTitlesRecursive(children[i]));
        }
    }
    if (children) {
        XFree(children);
    }

    return titles;
}

bool AutoTypePlatformX11::isTopLevelWindow(Window window)
{
    bool result = false;

    Atom type = None;
    int format;
    unsigned long nitems;
    unsigned long after;
    unsigned char* data = nullptr;

    // Check if the window has WM_STATE atom and it is not Withdrawn
    int retVal = XGetWindowProperty(
        m_dpy, window, m_atomWmState, 0, 2, False, m_atomWmState, &type, &format, &nitems, &after, &data);

    if (retVal == 0 && data) {
        if (type == m_atomWmState && format == 32 && nitems > 0) {
            result = (static_cast<quint32>(*data) != WithdrawnState);
        }
        XFree(data);
    } else {
        // See if this is a transient window without WM_STATE
        retVal = XGetWindowProperty(
            m_dpy, window, m_atomTransientFor, 0, 1, False, m_atomWindow, &type, &format, &nitems, &after, &data);
        if (retVal == 0 && data) {
            result = true;
            XFree(data);
        }
    }

    return result;
}

/*
 * Update the keyboard and modifier mapping.
 * We need the KeyboardMapping for AddKeysym.
 * Modifier mapping is required for clearing the modifiers.
 */
void AutoTypePlatformX11::updateKeymap()
{
    if (m_xkb) {
        XkbFreeKeyboard(m_xkb, XkbAllComponentsMask, True);
    }
    m_xkb = XkbGetMap(m_dpy, XkbAllClientInfoMask, XkbUseCoreKbd);

    /* workaround X11 bug https://gitlab.freedesktop.org/xorg/xserver/-/issues/1155 */
    XkbSetMap(m_dpy, XkbAllClientInfoMask, m_xkb);
    XSync(m_dpy, False);

    /* Build updated keymap */
    m_keymap.clear();
    m_remapKeycode = 0;

    for (int ckeycode = m_xkb->min_key_code; ckeycode < m_xkb->max_key_code; ckeycode++) {
        int groups = XkbKeyNumGroups(m_xkb, ckeycode);

        /* track highest remappable keycode, don't add to keymap */
        if (groups == 0) {
            m_remapKeycode = ckeycode;
            continue;
        }

        for (int cgroup = 0; cgroup < groups; cgroup++) {
            XkbKeyTypePtr type = XkbKeyKeyType(m_xkb, ckeycode, cgroup);

            for (int clevel = 0; clevel < type->num_levels; clevel++) {
                KeySym sym = XkbKeycodeToKeysym(m_dpy, ckeycode, cgroup, clevel);

                int mask = 0;
                for (int nmap = 0; nmap < type->map_count; nmap++) {
                    XkbKTMapEntryRec map = type->map[nmap];
                    if (map.active && map.level == clevel) {
                        mask = map.mods.mask;
                        break;
                    }
                }

                /* explicitly disallow requiring lock modifiers (Caps Lock and Num Lock) */
                if (mask & (LockMask | Mod2Mask)) {
                    continue;
                }

                m_keymap.append(AutoTypePlatformX11::KeyDesc{sym, ckeycode, cgroup, mask});
            }
        }
    }

    /* determine the keycode to use for modifiers */
    XModifierKeymap* modifiers = XGetModifierMapping(m_dpy);
    for (int mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
        m_modifier_keycode[mod_index] = 0;
        for (int mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++) {
            int keycode = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
            if (keycode) {
                m_modifier_keycode[mod_index] = keycode;
                break;
            }
        }
    }
    XFreeModifiermap(modifiers);
}

// --------------------------------------------------------------------------
// The following code is taken from xvkbd 3.0 and has been slightly modified.
// --------------------------------------------------------------------------

/*
 * Send event to the focused window.
 * If input focus is specified explicitly, select the window
 * before send event to the window.
 */
void AutoTypePlatformX11::SendKeyEvent(unsigned keycode, bool press)
{
    XSync(m_dpy, False);
    int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(MyErrorHandler);

    XTestFakeKeyEvent(m_dpy, keycode, press, 0);
    XFlush(m_dpy);
    XSync(m_dpy, False);

    XSetErrorHandler(oldHandler);
}

/*
 * Send a modifier press/release event for all modifiers
 * which are set in the mask variable.
 */
void AutoTypePlatformX11::SendModifiers(unsigned int mask, bool press)
{
    int mod_index;
    for (mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) {
        if (mask & (1 << mod_index)) {
            SendKeyEvent(m_modifier_keycode[mod_index], press);
        }
    }
}

/*
 * Determines the keycode and modifier mask for the given
 * keysym.
 */
bool AutoTypePlatformX11::GetKeycode(KeySym keysym, int* keycode, int* group, unsigned int* mask, bool* repeat)
{
    const KeyDesc* desc = nullptr;
    bool isDead = false;

    for (const auto& key : m_keymap) {
        if (key.sym == keysym) {
            // pick this description if we don't have any for this sym or this matches the current group
            if (desc == nullptr || key.group == *group) {
                desc = &key;
            }
        }
    }

    // try to find the best dead key mapping if we're unlucky to have one
    if (!desc) {
        for (const auto& map : deadMap) {
            if (map.first == keysym) {
                for (const auto& key : m_keymap) {
                    if (key.sym == map.second) {
                        // same as above, we try to match the group so no breaking out
                        if (desc == nullptr || key.group == *group) {
                            desc = &key;
                            isDead = true;
                        }
                    }
                }
            }
        }
    }

    if (desc) {
        *keycode = desc->code;
        *group = desc->group;
        *mask = desc->mask;
        *repeat = isDead;
        return true;
    }

    /* if we can't find an existing key for this keysym, try remapping */
    if (RemapKeycode(keysym)) {
        *keycode = m_remapKeycode;
        *group = 0;
        *mask = 0;
        *repeat = false;
        return true;
    }

    return false;
}

/*
 * Get remapped keycode for any keysym.
 */
bool AutoTypePlatformX11::RemapKeycode(KeySym keysym)
{
    if (!m_remapKeycode) {
        return false;
    }

    if (keysym != NoSymbol) {
        int type = XkbOneLevelIndex;
        if (XkbChangeTypesOfKey(m_xkb, m_remapKeycode, 1, XkbGroup1Mask, &type, NULL) != Success) {
            return false;
        }

        XkbKeySymEntry(m_xkb, m_remapKeycode, 0, 0) = keysym;
    } else {
        XkbChangeTypesOfKey(m_xkb, m_remapKeycode, 0, XkbGroup1Mask, NULL, NULL);
    }

    XkbSetMap(m_dpy, XkbAllClientInfoMask, m_xkb);
    XFlush(m_dpy);
    XSync(m_dpy, False);
    return true;
}

/*
 * Send sequence of KeyPressed/KeyReleased events to the focused
 * window to simulate keyboard.  If modifiers (shift, control, etc)
 * are set ON, many events will be sent.
 */
AutoTypeAction::Result AutoTypePlatformX11::sendKey(KeySym keysym, unsigned int modifiers)
{
    if (keysym == NoSymbol) {
        return AutoTypeAction::Result::Failed(tr("Trying to send invalid keyboard symbol."));
    }

    int keycode;
    int group;
    int group_active;
    unsigned int wanted_mask;
    bool repeat;

    /* pull current active layout group */
    XkbStateRec state;
    XkbGetState(m_dpy, XkbUseCoreKbd, &state);
    group_active = state.group;

    /* tell GetKeycode we would prefer a key from active group */
    group = group_active;

    Window root, child;
    int root_x, root_y, x, y;
    unsigned int original_mask;

    XSync(m_dpy, False);
    XQueryPointer(m_dpy, m_rootWindow, &root, &child, &root_x, &root_y, &x, &y, &original_mask);

    /* fail permanently if Caps Lock is on */
    if (original_mask & LockMask) {
        return AutoTypeAction::Result::Failed(tr("Sequence aborted: Caps Lock is on"));
    }

    /* retry if keysym affecting modifier is held except Num Lock (Mod2Mask) */
    if (original_mask & (ShiftMask | ControlMask | Mod1Mask | Mod3Mask | Mod4Mask | Mod5Mask)) {
        return AutoTypeAction::Result::Retry(tr("Sequence aborted: Modifier keys held by user"));
    }

    /* determine keycode, group and mask for the given keysym */
    if (!GetKeycode(keysym, &keycode, &group, &wanted_mask, &repeat)) {
        return AutoTypeAction::Result::Failed(tr("Unable to get valid keycode for key: ")
                                              + QString(XKeysymToString(keysym)));
    }

    wanted_mask |= modifiers;

    /* modifiers that need to be held but aren't */
    unsigned int press_mask = wanted_mask & ~original_mask;

    /* change layout group if necessary */
    if (group_active != group) {
        XkbLockGroup(m_dpy, XkbUseCoreKbd, group);
        XFlush(m_dpy);
    }

    /* hold modifiers */
    SendModifiers(press_mask, true);

    /* press and release key, with repeat if necessary */
    for (int i = 0; i < (repeat ? 2 : 1); i++) {
        SendKeyEvent(keycode, true);
        SendKeyEvent(keycode, false);
    }

    /* release modifiers */
    SendModifiers(press_mask, false);

    /* reset layout group if necessary */
    if (group_active != group) {
        XkbLockGroup(m_dpy, XkbUseCoreKbd, group_active);
        XFlush(m_dpy);
    }

    /* reset remap to prevent leaking remap keysyms longer than necessary */
    if (keycode == m_remapKeycode) {
        RemapKeycode(NoSymbol);
    }

    return AutoTypeAction::Result::Ok();
}

int AutoTypePlatformX11::MyErrorHandler(Display* my_dpy, XErrorEvent* event)
{
    char msg[200];

    if (event->error_code == BadWindow) {
        return 0;
    }
    XGetErrorText(my_dpy, event->error_code, msg, sizeof(msg) - 1);
    qWarning("X error trapped: %s, request-code=%d\n", msg, event->request_code);
    return 0;
}

AutoTypeExecutorX11::AutoTypeExecutorX11(AutoTypePlatformX11* platform)
    : m_platform(platform)
{
}

AutoTypeAction::Result AutoTypeExecutorX11::execBegin(const AutoTypeBegin* action)
{
    Q_UNUSED(action);
    m_platform->updateKeymap();
    return AutoTypeAction::Result::Ok();
}

AutoTypeAction::Result AutoTypeExecutorX11::execType(const AutoTypeKey* action)
{
    AutoTypeAction::Result result;

    if (action->key != Qt::Key_unknown) {
        result = m_platform->sendKey(qtToNativeKeyCode(action->key), qtToNativeModifiers(action->modifiers));
    } else {
        result = m_platform->sendKey(qcharToNativeKeyCode(action->character), qtToNativeModifiers(action->modifiers));
    }

    if (result.isOk()) {
        Tools::sleep(execDelayMs);
    }

    return result;
}

AutoTypeAction::Result AutoTypeExecutorX11::execClearField(const AutoTypeClearField* action)
{
    Q_UNUSED(action);
    execType(new AutoTypeKey(Qt::Key_Home));
    execType(new AutoTypeKey(Qt::Key_End, Qt::ShiftModifier));
    execType(new AutoTypeKey(Qt::Key_Backspace));
    return AutoTypeAction::Result::Ok();
}

bool AutoTypePlatformX11::raiseWindow(WId window)
{
    if (m_atomNetActiveWindow == None) {
        return false;
    }

    XRaiseWindow(m_dpy, window);

    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.window = window;
    event.xclient.message_type = m_atomNetActiveWindow;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; // FromApplication
    event.xclient.data.l[1] = QX11Info::appUserTime();
    QWidget* activeWindow = QApplication::activeWindow();
    if (activeWindow) {
        event.xclient.data.l[2] = activeWindow->internalWinId();
    } else {
        event.xclient.data.l[2] = 0;
    }
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = 0;
    XSendEvent(m_dpy, m_rootWindow, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(m_dpy);

    return true;
}
