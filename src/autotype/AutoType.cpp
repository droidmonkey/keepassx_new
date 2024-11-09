/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include "AutoType.h"

#include <QApplication>
#include <QDebug>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QUrl>

#include "config-keepassx.h"

#include "autotype/AutoTypePlatformPlugin.h"
#include "autotype/AutoTypeSelectDialog.h"
#include "autotype/PickcharsDialog.h"
#include "core/Global.h"
#include "core/Resources.h"
#include "core/Tools.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "gui/osutils/OSUtils.h"

namespace
{
    // Basic Auto-Type placeholder associations
    const QHash<QString, Qt::Key> g_placeholderToKey = {{"tab", Qt::Key_Tab},
                                                        {"enter", Qt::Key_Enter},
                                                        {"space", Qt::Key_Space},
                                                        {"up", Qt::Key_Up},
                                                        {"down", Qt::Key_Down},
                                                        {"left", Qt::Key_Left},
                                                        {"right", Qt::Key_Right},
                                                        {"insert", Qt::Key_Insert},
                                                        {"ins", Qt::Key_Insert},
                                                        {"delete", Qt::Key_Delete},
                                                        {"del", Qt::Key_Delete},
                                                        {"home", Qt::Key_Home},
                                                        {"end", Qt::Key_End},
                                                        {"pgup", Qt::Key_PageUp},
                                                        {"pgdown", Qt::Key_PageDown},
                                                        {"backspace", Qt::Key_Backspace},
                                                        {"bs", Qt::Key_Backspace},
                                                        {"bksp", Qt::Key_Backspace},
                                                        {"break", Qt::Key_Pause},
                                                        {"capslock", Qt::Key_CapsLock},
                                                        {"esc", Qt::Key_Escape},
                                                        {"help", Qt::Key_Help},
                                                        {"numlock", Qt::Key_NumLock},
                                                        {"ptrsc", Qt::Key_Print},
                                                        {"scrolllock", Qt::Key_ScrollLock},
                                                        {"win", Qt::Key_Meta},
                                                        {"lwin", Qt::Key_Meta},
                                                        {"rwin", Qt::Key_Meta},
                                                        {"apps", Qt::Key_Menu},
                                                        {"help", Qt::Key_Help},
                                                        {"add", Qt::Key_Plus},
                                                        {"subtract", Qt::Key_Minus},
                                                        {"multiply", Qt::Key_Asterisk},
                                                        {"divide", Qt::Key_Slash},
                                                        {"leftbrace", Qt::Key_BraceLeft},
                                                        {"{", Qt::Key_unknown},
                                                        {"rightbrace", Qt::Key_BraceRight},
                                                        {"}", Qt::Key_unknown},
                                                        {"leftparen", Qt::Key_ParenLeft},
                                                        {"(", Qt::Key_unknown},
                                                        {"rightparen", Qt::Key_ParenRight},
                                                        {")", Qt::Key_unknown},
                                                        {"[", Qt::Key_unknown},
                                                        {"]", Qt::Key_unknown},
                                                        {"+", Qt::Key_unknown},
                                                        {"%", Qt::Key_unknown},
                                                        {"^", Qt::Key_unknown},
                                                        {"~", Qt::Key_unknown},
                                                        {"#", Qt::Key_unknown},
                                                        {"numpad0", Qt::Key_0},
                                                        {"numpad1", Qt::Key_1},
                                                        {"numpad2", Qt::Key_2},
                                                        {"numpad3", Qt::Key_3},
                                                        {"numpad4", Qt::Key_4},
                                                        {"numpad5", Qt::Key_5},
                                                        {"numpad6", Qt::Key_6},
                                                        {"numpad7", Qt::Key_7},
                                                        {"numpad8", Qt::Key_8},
                                                        {"numpad9", Qt::Key_9},
                                                        {"f1", Qt::Key_F1},
                                                        {"f2", Qt::Key_F2},
                                                        {"f3", Qt::Key_F3},
                                                        {"f4", Qt::Key_F4},
                                                        {"f5", Qt::Key_F5},
                                                        {"f6", Qt::Key_F6},
                                                        {"f7", Qt::Key_F7},
                                                        {"f8", Qt::Key_F8},
                                                        {"f9", Qt::Key_F9},
                                                        {"f10", Qt::Key_F10},
                                                        {"f11", Qt::Key_F11},
                                                        {"f12", Qt::Key_F12},
                                                        {"f13", Qt::Key_F13},
                                                        {"f14", Qt::Key_F14},
                                                        {"f15", Qt::Key_F15},
                                                        {"f16", Qt::Key_F16}};
} // namespace

AutoType* AutoType::m_instance = nullptr;

AutoType::AutoType(QObject* parent, bool test)
    : QObject(parent)
    , m_pluginLoader(new QPluginLoader(this))
    , m_plugin(nullptr)
    , m_executor(nullptr)
    , m_windowState(WindowState::Normal)
    , m_windowForGlobal(0)
    , m_lastMatch(nullptr, QString())
    , m_lastMatchRetypeTimer(nullptr)
{
    // configure timer to reset last match
    m_lastMatchRetypeTimer.setSingleShot(true);
    connect(&m_lastMatchRetypeTimer, &QTimer::timeout, this, [this] {
        m_lastMatch = {nullptr, QString()};
        emit autotypeRetypeTimeout();
    });

    // prevent crash when the plugin has unresolved symbols
    m_pluginLoader->setLoadHints(QLibrary::ResolveAllSymbolsHint);

    QString pluginName = "keepassxc-autotype-";
    if (!test) {
        pluginName += QApplication::platformName();
    } else {
        pluginName += "test";
    }

    QString pluginPath = resources()->pluginPath(pluginName);

    if (!pluginPath.isEmpty()) {
#ifdef WITH_XC_AUTOTYPE
        loadPlugin(pluginPath);
#endif
    }

    connect(qApp, SIGNAL(aboutToQuit()), SLOT(unloadPlugin()));
}

AutoType::~AutoType()
{
    if (m_executor) {
        delete m_executor;
        m_executor = nullptr;
    }
}

void AutoType::loadPlugin(const QString& pluginPath)
{
    m_pluginLoader->setFileName(pluginPath);

    QObject* pluginInstance = m_pluginLoader->instance();
    if (pluginInstance) {
        m_plugin = qobject_cast<AutoTypePlatformInterface*>(pluginInstance);
        m_executor = nullptr;

        if (m_plugin) {
            if (m_plugin->isAvailable()) {
                m_executor = m_plugin->createExecutor();
                connect(osUtils,
                        &OSUtilsBase::globalShortcutTriggered,
                        this,
                        [this](const QString& name, const QString& initialSearch) {
                            if (name == "autotype") {
                                startGlobalAutoType(initialSearch);
                            }
                        });
            } else {
                unloadPlugin();
            }
        }
    }

    if (!m_plugin) {
        qWarning("Unable to load auto-type plugin:\n%s", qPrintable(m_pluginLoader->errorString()));
    }
}

void AutoType::unloadPlugin()
{
    if (m_executor) {
        delete m_executor;
        m_executor = nullptr;
    }

    if (m_plugin) {
        m_plugin->unload();
        m_plugin = nullptr;
    }
}

AutoType* AutoType::instance()
{
    if (!m_instance) {
        m_instance = new AutoType(qApp);
    }

    return m_instance;
}

void AutoType::createTestInstance()
{
    Q_ASSERT(!m_instance);

    m_instance = new AutoType(qApp, true);
}

QStringList AutoType::windowTitles()
{
    if (!m_plugin) {
        return {};
    }

    return m_plugin->windowTitles();
}

void AutoType::raiseWindow()
{
#if defined(Q_OS_MACOS)
    m_plugin->raiseOwnWindow();
#endif
}

bool AutoType::registerGlobalShortcut(Qt::Key key, Qt::KeyboardModifiers modifiers, QString* error)
{
    if (!m_plugin) {
        return false;
    }

    return osUtils->registerGlobalShortcut("autotype", key, modifiers, error);
}

void AutoType::unregisterGlobalShortcut()
{
    osUtils->unregisterGlobalShortcut("autotype");
}

/**
 * Core Autotype function that will execute actions
 */
void AutoType::executeAutoTypeActions(const Entry* entry,
                                      const QString& sequence,
                                      WId window,
                                      AutoTypeExecutor::Mode mode)
{
    QString error;
    auto actions = parseSequence(sequence, entry, error);

    if (!error.isEmpty()) {
        auto errorMsg = tr("The requested Auto-Type sequence cannot be used due to an error:");
        errorMsg.append(QString("\n%1\n%2").arg(sequence, error));
        if (getMainWindow()) {
            MessageBox::critical(getMainWindow(), tr("Auto-Type Error"), errorMsg);
        }
        qWarning() << errorMsg;
        emit autotypeFinished();
        return;
    }

    if (!m_inAutoType.tryLock()) {
        return;
    }

    // Explicitly hide the main window if no target window is specified
    if (window == 0) {
#if defined(Q_OS_MACOS)
        // Check for accessibility permission
        if (!macUtils()->enableAccessibility()) {
            MessageBox::information(nullptr,
                                    tr("Permission Required"),
                                    tr("KeePassXC requires the Accessibility permission in order to perform entry "
                                       "level Auto-Type. If you already granted permission, you may have to restart "
                                       "KeePassXC."));
            return;
        }

        macUtils()->raiseLastActiveWindow();
        m_plugin->hideOwnWindow();
#else
        if (getMainWindow()) {
            getMainWindow()->minimizeOrHide();
        }
#endif
    } else {
        // Restore window state (macOS only) then raise the target window
        restoreWindowState();
        QCoreApplication::processEvents();
        m_plugin->raiseWindow(window);
    }

    // Restore executor mode
    m_executor->mode = mode;

    int delay = qMax(100, config()->get(Config::AutoTypeStartDelay).toInt());
    Tools::wait(delay);

    // Grab the current active window after everything settles
    if (window == 0) {
        window = m_plugin->activeWindow();
    }

    for (const auto& action : asConst(actions)) {
        // Cancel Auto-Type if the active window changed
        if (m_plugin->activeWindow() != window) {
            qWarning("Active window changed, interrupting auto-type.");
            break;
        }

        bool failed = false;
        constexpr int max_retries = 5;
        for (int i = 1; i <= max_retries; i++) {
            auto result = action->exec(m_executor);

            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

            if (result.isOk()) {
                break;
            }

            if (!result.canRetry() || i == max_retries) {
                if (getMainWindow()) {
                    MessageBox::critical(getMainWindow(), tr("Auto-Type Error"), result.errorString());
                }
                failed = true;
                break;
            }

            Tools::wait(delay);
        }

        // Last action failed to complete, cancel the rest of the sequence
        if (failed) {
            break;
        }
    }

    resetAutoTypeState();
    m_inAutoType.unlock();
    emit autotypeFinished();
}

/**
 * Single Autotype entry-point function
 * Look up the Auto-Type sequence for the given entry then perform Auto-Type in the active window
 */
void AutoType::performAutoType(const Entry* entry)
{
    if (!m_plugin) {
        return;
    }

    auto sequences = entry->autoTypeSequences();
    if (!sequences.isEmpty()) {
        executeAutoTypeActions(entry, sequences.first());
    }
}

/**
 * Extra Autotype entry-point function
 * Perform Auto-Type of the directly specified sequence in the active window
 */
void AutoType::performAutoTypeWithSequence(const Entry* entry, const QString& sequence)
{
    if (!m_plugin) {
        return;
    }

    executeAutoTypeActions(entry, sequence);
}

void AutoType::startGlobalAutoType(const QString& search)
{
    // Never Auto-Type into KeePassXC itself
    if (getMainWindow() && (qApp->activeWindow() || qApp->activeModalWidget())) {
        return;
    }

    m_windowForGlobal = m_plugin->activeWindow();
    m_windowTitleForGlobal = m_plugin->activeWindowTitle();
#ifdef Q_OS_MACOS
    // Determine if the user has given proper permissions to KeePassXC to perform Auto-Type
    static bool accessibilityChecked = false;
    if (!accessibilityChecked) {
        if (macUtils()->enableAccessibility() && macUtils()->enableScreenRecording()) {
            accessibilityChecked = true;
        } else if (getMainWindow()) {
            // Does not have required permissions to Auto-Type, ignore the event
            MessageBox::information(
                nullptr,
                tr("Permission Required"),
                tr("KeePassXC requires the Accessibility and Screen Recorder permission in order to perform global "
                   "Auto-Type. Screen Recording is necessary to use the window title to find entries. If you "
                   "already granted permission, you may have to restart KeePassXC."));
            return;
        }
    }

    m_windowState = WindowState::Normal;
    if (getMainWindow()) {
        if (getMainWindow()->isMinimized()) {
            m_windowState = WindowState::Minimized;
        }
        if (getMainWindow()->isHidden()) {
            m_windowState = WindowState::Hidden;
        }
    }
#endif

    emit globalAutoTypeTriggered(search);
}

/**
 * Global Autotype entry-point function
 * Perform global Auto-Type on the active window
 */
void AutoType::performGlobalAutoType(const QList<QSharedPointer<Database>>& dbList, const QString& search)
{
    if (!m_plugin) {
        return;
    }

    if (!m_inGlobalAutoTypeDialog.tryLock()) {
        return;
    }

    if (m_windowTitleForGlobal.isEmpty()) {
        m_inGlobalAutoTypeDialog.unlock();
        return;
    }

    QList<AutoTypeMatch> matchList;
    bool hideExpired = config()->get(Config::AutoTypeHideExpiredEntry).toBool();

    for (const auto& db : dbList) {
        const QList<Entry*> dbEntries = db->rootGroup()->entriesRecursive();
        for (auto entry : dbEntries) {
            auto group = entry->group();
            if (!group || !group->resolveAutoTypeEnabled() || !entry->autoTypeEnabled()) {
                continue;
            }

            if (hideExpired && entry->isExpired()) {
                continue;
            }
            const QSet<QString> sequences = Tools::asSet(entry->autoTypeSequences(m_windowTitleForGlobal));
            for (const auto& sequence : sequences) {
                matchList << AutoTypeMatch(entry, sequence);
            }
        }
    }

    // Show the selection dialog if we always ask, have multiple matches, or no matches
    if (getMainWindow()
        && (config()->get(Config::Security_AutoTypeAsk).toBool() || matchList.size() > 1 || matchList.isEmpty())) {
        // Close any open modal windows that would interfere with the process
        getMainWindow()->closeModalWindow();

        auto* selectDialog = new AutoTypeSelectDialog();
        selectDialog->setMatches(matchList, dbList, m_lastMatch);

        if (!search.isEmpty()) {
            selectDialog->setSearchString(search);
        }

        connect(getMainWindow(), &MainWindow::databaseLocked, selectDialog, &AutoTypeSelectDialog::reject);
        connect(selectDialog,
                &AutoTypeSelectDialog::matchActivated,
                this,
                [this](const AutoTypeMatch& match, bool virtualMode) {
                    m_lastMatch = match;
                    m_lastMatchRetypeTimer.start(config()->get(Config::GlobalAutoTypeRetypeTime).toInt() * 1000);
                    executeAutoTypeActions(match.first,
                                           match.second,
                                           m_windowForGlobal,
                                           virtualMode ? AutoTypeExecutor::Mode::VIRTUAL
                                                       : AutoTypeExecutor::Mode::NORMAL);
                    resetAutoTypeState();
                });
        connect(selectDialog, &QDialog::rejected, this, [this] {
            restoreWindowState();
            resetAutoTypeState();
            emit autotypeFinished();
        });

#ifdef Q_OS_MACOS
        m_plugin->raiseOwnWindow();
        Tools::wait(50);
#endif
        selectDialog->show();
        selectDialog->raise();
        selectDialog->activateWindow();
    } else if (!matchList.isEmpty()) {
        // Only one match and not asking, do it!
        executeAutoTypeActions(matchList.first().first, matchList.first().second, m_windowForGlobal);
        resetAutoTypeState();
    } else {
        // We should never get here
        resetAutoTypeState();
        emit autotypeFinished();
    }
}

void AutoType::restoreWindowState()
{
#ifdef Q_OS_MAC
    if (getMainWindow()) {
        if (m_windowState == WindowState::Minimized) {
            getMainWindow()->showMinimized();
        } else if (m_windowState == WindowState::Hidden) {
            getMainWindow()->hideWindow();
        }
    }
#endif
}

void AutoType::resetAutoTypeState()
{
    m_windowForGlobal = 0;
    m_windowTitleForGlobal.clear();
    Q_UNUSED(m_inGlobalAutoTypeDialog.tryLock());
    m_inGlobalAutoTypeDialog.unlock();
}

/**
 * Parse an autotype sequence into a list of AutoTypeActions.
 * If error is provided then syntax checking will be performed.
 */
QList<QSharedPointer<AutoTypeAction>>
AutoType::parseSequence(const QString& entrySequence, const Entry* entry, QString& error, bool syntaxOnly)
{
    if (!entry) {
        error = tr("Invalid entry provided");
        return {};
    }

    const int maxTypeDelay = 500;
    const int maxWaitDelay = 10000;
    const int maxRepetition = 100;

    QList<QSharedPointer<AutoTypeAction>> actions;
    actions << QSharedPointer<AutoTypeBegin>::create();
    actions << QSharedPointer<AutoTypeDelay>::create(qMax(0, config()->get(Config::AutoTypeDelay).toInt()), true);

    // Replace escaped braces with a template for easier regex
    QString sequence = entrySequence;
    sequence.replace("{{}", "{LEFTBRACE}");
    sequence.replace("{}}", "{RIGHTBRACE}");

    // Quick test for bracket syntax
    if (sequence.count("{") != sequence.count("}")) {
        error = tr("Bracket imbalance detected, found extra { or }");
        return {};
    }

    // Group 1 = modifier key (opt)
    // Group 2 = full placeholder
    // Group 3 = inner placeholder (allows nested placeholders)
    // Group 4 = repeat (opt)
    // Group 5 = character
    QRegularExpression regex("([+%^#]*)(?:({((?>[^{}]+?|(?2))+?)(?:\\s+(\\d+))?})|(.))");
    auto results = regex.globalMatch(sequence);
    while (results.hasNext()) {
        auto match = results.next();

        // Parse modifier keys
        Qt::KeyboardModifiers modifiers;
        if (match.captured(1).contains('+')) {
            modifiers |= Qt::ShiftModifier;
        }
        if (match.captured(1).contains('^')) {
            modifiers |= Qt::ControlModifier;
        }
        if (match.captured(1).contains('%')) {
            modifiers |= Qt::AltModifier;
        }
        if (match.captured(1).contains('#')) {
            modifiers |= Qt::MetaModifier;
        }

        const auto fullPlaceholder = match.captured(2);
        auto placeholder = match.captured(3).toLower();
        auto repeat = 1;
        if (!match.captured(4).isEmpty()) {
            repeat = match.captured(4).toInt();
        }
        auto character = match.captured(5);

        if (placeholder.isEmpty()) {
            if (!character.isEmpty()) {
                // Type a single character with modifier
                actions << QSharedPointer<AutoTypeKey>::create(character[0], modifiers);
            }
            continue;
        }

        if (g_placeholderToKey.contains(placeholder)) {
            // Basic key placeholder, allow repeat
            if (repeat > maxRepetition) {
                error = tr("Too many repetitions detected, max is %1: %2").arg(maxRepetition).arg(fullPlaceholder);
                return {};
            }
            QSharedPointer<AutoTypeKey> action;
            if (g_placeholderToKey[placeholder] == Qt::Key_unknown) {
                action = QSharedPointer<AutoTypeKey>::create(placeholder[0], modifiers);
            } else {
                action = QSharedPointer<AutoTypeKey>::create(g_placeholderToKey[placeholder], modifiers);
            }
            for (int i = 1; i <= repeat && i <= maxRepetition; ++i) {
                actions << action;
            }
        } else if (placeholder.startsWith("delay=")) {
            // Change keypress delay
            int delay = placeholder.replace("delay=", "").toInt();
            if (delay > maxTypeDelay) {
                error = tr("Very slow key press detected, max is %1: %2").arg(maxTypeDelay).arg(fullPlaceholder);
                return {};
            }
            actions << QSharedPointer<AutoTypeDelay>::create(qBound(0, delay, maxTypeDelay), true);
        } else if (placeholder == "delay") {
            // Mid typing delay (wait)
            if (repeat > maxWaitDelay) {
                error = tr("Very long delay detected, max is %1: %2").arg(maxWaitDelay).arg(fullPlaceholder);
                return {};
            }
            actions << QSharedPointer<AutoTypeDelay>::create(qBound(0, repeat, maxWaitDelay));
        } else if (placeholder == "clearfield") {
            // Platform-specific field clearing
            actions << QSharedPointer<AutoTypeClearField>::create();
        } else if (placeholder == "totp") {
            // Entry totp (requires special handling)
            QString totp = entry->totp();
            for (const auto& ch : totp) {
                actions << QSharedPointer<AutoTypeKey>::create(ch);
            }
        } else if (placeholder.startsWith("pickchars")) {
            // Reset to the original capture to preserve case
            placeholder = match.captured(3);

            auto attribute = EntryAttributes::PasswordKey;
            if (placeholder.contains(":")) {
                attribute = placeholder.section(":", 1);
                if (!entry->attributes()->hasKey(attribute)) {
                    error = tr("Entry does not have attribute for PICKCHARS: %1").arg(attribute);
                    return {};
                }
            }

            // Bail out if we are just syntax checking
            if (syntaxOnly) {
                continue;
            }

            // Show pickchars dialog for the desired attribute
            auto password = entry->resolvePlaceholder(entry->attribute(attribute));
            if (!password.isEmpty()) {
                PickcharsDialog pickcharsDialog(password);
                if (pickcharsDialog.exec() == QDialog::Accepted && !pickcharsDialog.selectedChars().isEmpty()) {
                    for (const auto& ch : pickcharsDialog.selectedChars()) {
                        actions << QSharedPointer<AutoTypeKey>::create(ch);
                        if (pickcharsDialog.pressTab()) {
                            actions << QSharedPointer<AutoTypeKey>::create(g_placeholderToKey["tab"]);
                        }
                    }
                    if (pickcharsDialog.pressTab()) {
                        // Remove extra tab
                        actions.removeLast();
                    }
                }
            }
        } else if (placeholder.startsWith("t-conv:")) {
            // Reset to the original capture to preserve case
            placeholder = match.captured(3);
            auto resolved = entry->resolveConversionPlaceholder(placeholder, &error);
            if (!error.isEmpty()) {
                return {};
            }
            for (const QChar& ch : resolved) {
                actions << QSharedPointer<AutoTypeKey>::create(ch);
            }
        } else if (placeholder.startsWith("t-replace-rx:")) {
            // Reset to the original capture to preserve case
            placeholder = match.captured(3);
            auto resolved = entry->resolveRegexPlaceholder(placeholder, &error);
            if (!error.isEmpty()) {
                return {};
            }
            for (const QChar& ch : resolved) {
                actions << QSharedPointer<AutoTypeKey>::create(ch);
            }
        } else if (placeholder.startsWith("mode=")) {
            auto mode = AutoTypeExecutor::Mode::NORMAL;
            if (placeholder.endsWith("virtual")) {
                mode = AutoTypeExecutor::Mode::VIRTUAL;
            }
            actions << QSharedPointer<AutoTypeMode>::create(mode);
        } else if (placeholder.startsWith("beep") || placeholder.startsWith("vkey")
                   || placeholder.startsWith("appactivate") || placeholder.startsWith("c:")) {
            // Ignore these commands
        } else {
            // Attempt to resolve an entry attribute
            auto resolved = entry->resolvePlaceholder(fullPlaceholder);
            if (resolved != fullPlaceholder) {
                // Attribute resolved, add characters to action list
                for (const QChar& ch : resolved) {
                    if (ch == '\n') {
                        actions << QSharedPointer<AutoTypeKey>::create(g_placeholderToKey["enter"]);
                    } else if (ch == '\t') {
                        actions << QSharedPointer<AutoTypeKey>::create(g_placeholderToKey["tab"]);
                    } else {
                        actions << QSharedPointer<AutoTypeKey>::create(ch);
                    }
                }
            } else {
                // Invalid placeholder, issue error and stop processing
                error = tr("Invalid placeholder: %1").arg(fullPlaceholder);
                return {};
            }
        }
    }

    return actions;
}

/**
 * Verify if the syntax of an autotype sequence is correct and doesn't have invalid parameters
 */
bool AutoType::verifyAutoTypeSyntax(const QString& sequence, const Entry* entry, QString& error)
{
    error.clear();
    if (!sequence.isEmpty()) {
        parseSequence(sequence, entry, error, true);
    }
    return error.isEmpty();
}
