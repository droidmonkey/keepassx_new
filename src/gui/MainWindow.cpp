/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QMimeData>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QWindow>

#include "config-keepassx.h"

#include "Application.h"
#include "Clipboard.h"
#include "autotype/AutoType.h"
#include "core/InactivityTimer.h"
#include "core/Resources.h"
#include "gui/AboutDialog.h"
#include "gui/ActionCollection.h"
#include "gui/Icons.h"
#include "gui/MessageBox.h"
#include "gui/SearchWidget.h"
#include "gui/ShortcutSettingsPage.h"
#include "gui/entry/EntryView.h"
#include "gui/osutils/OSUtils.h"
#include "gui/remote/RemoteSettings.h"

#ifdef WITH_XC_UPDATECHECK
#include "gui/UpdateCheckDialog.h"
#include "networking/UpdateChecker.h"
#endif

#ifdef WITH_XC_SSHAGENT
#include "sshagent/AgentSettingsPage.h"
#include "sshagent/SSHAgent.h"
#endif
#ifdef WITH_XC_KEESHARE
#include "keeshare/KeeShare.h"
#include "keeshare/SettingsPageKeeShare.h"
#endif

#ifdef WITH_XC_FDOSECRETS
#include "fdosecrets/FdoSecretsPlugin.h"
#endif

#ifdef WITH_XC_YUBIKEY
#include "keys/drivers/YubiKey.h"
#endif

#ifdef WITH_XC_BROWSER
#include "browser/BrowserService.h"
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS) && !defined(QT_NO_DBUS)
#include "mainwindowadaptor.h"
#endif

const QString MainWindow::BaseWindowTitle = "KeePassXC";

MainWindow* g_MainWindow = nullptr;
MainWindow* getMainWindow()
{
    return g_MainWindow;
}

MainWindow::MainWindow()
    : m_ui(new Ui::MainWindow())
{
    g_MainWindow = this;

    m_ui->setupUi(this);

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS) && !defined(QT_NO_DBUS)
    new MainWindowAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/keepassxc", this);
    dbus.registerService("org.keepassxc.KeePassXC.MainWindow");
#endif

    setAcceptDrops(true);

    if (config()->get(Config::GUI_CompactMode).toBool()) {
        m_ui->toolBar->setIconSize({20, 20});
    }

    // Setup the search widget in the toolbar
    m_searchWidget = new SearchWidget();
    m_searchWidget->connectSignals(m_actionMultiplexer);
    m_searchWidgetAction = m_ui->toolBar->addWidget(m_searchWidget);
    m_searchWidgetAction->setEnabled(false);

    new QShortcut(QKeySequence::Find, this, SLOT(focusSearchWidget()));

    connect(m_searchWidget, &SearchWidget::searchCanceled, this, [this] {
        m_ui->toolBar->setExpanded(false);
        m_ui->toolBar->setVisible(!config()->get(Config::GUI_HideToolbar).toBool());
    });
    connect(m_searchWidget, &SearchWidget::lostFocus, this, [this] {
        m_ui->toolBar->setExpanded(false);
        m_ui->toolBar->setVisible(!config()->get(Config::GUI_HideToolbar).toBool());
    });

    m_countDefaultAttributes = m_ui->menuEntryCopyAttribute->actions().size();

    m_entryContextMenu = new QMenu(this);
    m_entryContextMenu->setSeparatorsCollapsible(true);
    m_entryContextMenu->addAction(m_ui->actionEntryRestore);
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryCopyUsername);
    m_entryContextMenu->addAction(m_ui->actionEntryCopyPassword);
    m_entryContextMenu->addAction(m_ui->actionEntryCopyURL);
    m_entryContextMenu->addAction(m_ui->menuEntryCopyAttribute->menuAction());
    m_entryContextMenu->addAction(m_ui->menuEntryTotp->menuAction());
    m_entryContextMenu->addAction(m_ui->menuTags->menuAction());
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryAutoType);
    m_entryContextMenu->addSeparator();
#ifdef WITH_XC_BROWSER_PASSKEYS
    m_entryContextMenu->addAction(m_ui->actionEntryImportPasskey);
    m_entryContextMenu->addAction(m_ui->actionEntryRemovePasskey);
    m_entryContextMenu->addSeparator();
#endif
    m_entryContextMenu->addAction(m_ui->actionEntryEdit);
    m_entryContextMenu->addAction(m_ui->actionEntryClone);
    m_entryContextMenu->addAction(m_ui->actionEntryDelete);
    m_entryContextMenu->addAction(m_ui->actionEntryNew);
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryMoveUp);
    m_entryContextMenu->addAction(m_ui->actionEntryMoveDown);
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryOpenUrl);
    m_entryContextMenu->addAction(m_ui->actionEntryDownloadIcon);
    m_entryContextMenu->addSeparator();
    m_entryContextMenu->addAction(m_ui->actionEntryAddToAgent);
    m_entryContextMenu->addAction(m_ui->actionEntryRemoveFromAgent);

    m_entryNewContextMenu = new QMenu(this);
    m_entryNewContextMenu->addAction(m_ui->actionEntryNew);

    connect(m_ui->menuRemoteSync, &QMenu::aboutToShow, this, &MainWindow::updateRemoteSyncMenuEntries);

    // Build Entry Level Auto-Type menu
    auto autotypeMenu = new QMenu({}, this);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeSequence);
    autotypeMenu->addSeparator();
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeUsername);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeUsernameEnter);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypePassword);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypePasswordEnter);
    autotypeMenu->addAction(m_ui->actionEntryAutoTypeTOTP);
    m_ui->actionEntryAutoType->setMenu(autotypeMenu);
    auto autoTypeButton = qobject_cast<QToolButton*>(m_ui->toolBar->widgetForAction(m_ui->actionEntryAutoType));
    if (autoTypeButton) {
        autoTypeButton->setPopupMode(QToolButton::MenuButtonPopup);
    }

    auto databaseLockMenu = new QMenu({}, this);
    databaseLockMenu->addAction(m_ui->actionLockAllDatabases);
    m_ui->actionLockDatabaseToolbar->setMenu(databaseLockMenu);
    auto databaseLockButton =
        qobject_cast<QToolButton*>(m_ui->toolBar->widgetForAction(m_ui->actionLockDatabaseToolbar));
    if (databaseLockButton) {
        databaseLockButton->setPopupMode(QToolButton::MenuButtonPopup);
    }

    restoreGeometry(config()->get(Config::GUI_MainWindowGeometry).toByteArray());
    restoreState(config()->get(Config::GUI_MainWindowState).toByteArray());

    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseLocked, this, &MainWindow::databaseLocked);
    connect(m_ui->tabWidget, &DatabaseTabWidget::databaseUnlocked, this, &MainWindow::databaseUnlocked);
    connect(m_ui->tabWidget, &DatabaseTabWidget::activeDatabaseChanged, this, &MainWindow::activeDatabaseChanged);

    initViewMenu();
    initActionCollection();

    m_ui->settingsWidget->addSettingsPage(new ShortcutSettingsPage());

#ifdef WITH_XC_BROWSER
    connect(
        browserService(), &BrowserService::requestUnlock, m_ui->tabWidget, &DatabaseTabWidget::performBrowserUnlock);
#endif

#ifdef WITH_XC_SSHAGENT
    connect(sshAgent(), SIGNAL(error(QString)), this, SLOT(showErrorMessage(QString)));
    connect(sshAgent(), SIGNAL(enabledChanged(bool)), this, SLOT(agentEnabled(bool)));
    m_ui->settingsWidget->addSettingsPage(new AgentSettingsPage());
#endif

#if defined(WITH_XC_KEESHARE)
    KeeShare::init(this);
    m_ui->settingsWidget->addSettingsPage(new SettingsPageKeeShare(m_ui->tabWidget));
    connect(KeeShare::instance(),
            SIGNAL(sharingMessage(QString, MessageWidget::MessageType)),
            SLOT(displayGlobalMessage(QString, MessageWidget::MessageType)));
#endif

#ifdef WITH_XC_FDOSECRETS
    auto fdoSS = new FdoSecretsPlugin(m_ui->tabWidget);
    connect(fdoSS, &FdoSecretsPlugin::error, this, &MainWindow::showErrorMessage);
    connect(fdoSS, &FdoSecretsPlugin::requestSwitchToDatabases, this, &MainWindow::switchToDatabases);
    connect(fdoSS, &FdoSecretsPlugin::requestShowNotification, this, &MainWindow::displayDesktopNotification);
    fdoSS->updateServiceState();
    m_ui->settingsWidget->addSettingsPage(fdoSS);
#endif

#ifdef WITH_XC_YUBIKEY
    connect(YubiKey::instance(), SIGNAL(userInteractionRequest()), SLOT(showYubiKeyPopup()), Qt::QueuedConnection);
    connect(YubiKey::instance(), SIGNAL(challengeCompleted()), SLOT(hideYubiKeyPopup()), Qt::QueuedConnection);
#endif

    setWindowIcon(icons()->applicationIcon());
    m_ui->globalMessageWidget->hideMessage();
    connect(m_ui->globalMessageWidget, &MessageWidget::linkActivated, &MessageWidget::openHttpUrl);

    m_clearHistoryAction = new QAction(tr("Clear history"), m_ui->menuFile);
    m_lastDatabasesActions = new QActionGroup(m_ui->menuRecentDatabases);
    connect(m_clearHistoryAction, SIGNAL(triggered()), this, SLOT(clearLastDatabases()));
    connect(m_lastDatabasesActions, SIGNAL(triggered(QAction*)), this, SLOT(openRecentDatabase(QAction*)));
    connect(m_ui->menuRecentDatabases, SIGNAL(aboutToShow()), this, SLOT(updateLastDatabasesMenu()));

    m_copyAdditionalAttributeActions = new QActionGroup(m_ui->menuEntryCopyAttribute);
    m_actionMultiplexer.connect(
        m_copyAdditionalAttributeActions, SIGNAL(triggered(QAction*)), SLOT(copyAttribute(QAction*)));
    connect(m_ui->menuEntryCopyAttribute, SIGNAL(aboutToShow()), this, SLOT(updateCopyAttributesMenu()));

    m_setTagsMenuActions = new QActionGroup(m_ui->menuTags);
    m_setTagsMenuActions->setExclusive(false);
    m_actionMultiplexer.connect(m_setTagsMenuActions, SIGNAL(triggered(QAction*)), SLOT(setTag(QAction*)));
    connect(m_ui->menuTags, &QMenu::aboutToShow, this, &MainWindow::updateSetTagsMenu);

    Qt::Key globalAutoTypeKey = static_cast<Qt::Key>(config()->get(Config::GlobalAutoTypeKey).toInt());
    Qt::KeyboardModifiers globalAutoTypeModifiers =
        static_cast<Qt::KeyboardModifiers>(config()->get(Config::GlobalAutoTypeModifiers).toInt());
    if (globalAutoTypeKey > 0 && globalAutoTypeModifiers > 0) {
        autoType()->registerGlobalShortcut(globalAutoTypeKey, globalAutoTypeModifiers);
    }

    m_ui->toolbarSeparator->setVisible(false);
    m_showToolbarSeparator = config()->get(Config::GUI_ApplicationTheme).toString() != "classic";

    m_ui->actionEntryAutoType->setVisible(autoType()->isAvailable());
    m_ui->actionAllowScreenCapture->setVisible(osUtils->canPreventScreenCapture());

    m_inactivityTimer = new InactivityTimer(this);
    connect(m_inactivityTimer, SIGNAL(inactivityDetected()), this, SLOT(lockDatabasesAfterInactivity()));
    applySettingsChanges();

    // Qt 5.10 introduced a new "feature" to hide shortcuts in context menus
    // Unfortunately, Qt::AA_DontShowShortcutsInContextMenus is broken, have to manually enable them
    m_ui->actionEntryNew->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryEdit->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryDelete->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryRestore->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryClone->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryTotp->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryDownloadIcon->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryCopyTotp->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryCopyPasswordTotp->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryMoveUp->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryMoveDown->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryCopyUsername->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryCopyPassword->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryAutoTypeSequence->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryOpenUrl->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryCopyURL->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryCopyTitle->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryAddToAgent->setShortcutVisibleInContextMenu(true);
    m_ui->actionEntryRemoveFromAgent->setShortcutVisibleInContextMenu(true);

    connect(m_ui->menuEntries, SIGNAL(aboutToShow()), SLOT(obtainContextFocusLock()));
    connect(m_ui->menuEntries, SIGNAL(aboutToHide()), SLOT(releaseContextFocusLock()));
    connect(m_entryContextMenu, SIGNAL(aboutToShow()), SLOT(obtainContextFocusLock()));
    connect(m_entryContextMenu, SIGNAL(aboutToHide()), SLOT(releaseContextFocusLock()));
    connect(m_entryNewContextMenu, SIGNAL(aboutToShow()), SLOT(obtainContextFocusLock()));
    connect(m_entryNewContextMenu, SIGNAL(aboutToHide()), SLOT(releaseContextFocusLock()));
    connect(m_ui->menuGroups, SIGNAL(aboutToShow()), SLOT(obtainContextFocusLock()));
    connect(m_ui->menuGroups, SIGNAL(aboutToHide()), SLOT(releaseContextFocusLock()));

    // Control window state
    new QShortcut(Qt::CTRL + Qt::Key_M, this, SLOT(minimizeOrHide()));
    new QShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_M, this, SLOT(hideWindow()));
    // Control database tabs
    // Ctrl+Tab is broken on Mac, so use Alt (i.e. the Option key) - https://bugreports.qt.io/browse/QTBUG-8596
    auto dbTabModifier2 = Qt::CTRL;
#ifdef Q_OS_MACOS
    dbTabModifier2 = Qt::ALT;
#endif
    new QShortcut(dbTabModifier2 + Qt::Key_Tab, this, SLOT(selectNextDatabaseTab()));
    new QShortcut(Qt::CTRL + Qt::Key_PageDown, this, SLOT(selectNextDatabaseTab()));
    new QShortcut(dbTabModifier2 + Qt::SHIFT + Qt::Key_Tab, this, SLOT(selectPreviousDatabaseTab()));
    new QShortcut(Qt::CTRL + Qt::Key_PageUp, this, SLOT(selectPreviousDatabaseTab()));

    // Tab selection by number, Windows uses Ctrl, macOS uses Command,
    // and Linux uses Alt to emulate a browser-like experience
    auto dbTabModifier = Qt::CTRL;
#ifdef Q_OS_LINUX
    dbTabModifier = Qt::ALT;
#endif
    auto shortcut = new QShortcut(dbTabModifier + Qt::Key_1, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(0); });
    shortcut = new QShortcut(dbTabModifier + Qt::Key_2, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(1); });
    shortcut = new QShortcut(dbTabModifier + Qt::Key_3, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(2); });
    shortcut = new QShortcut(dbTabModifier + Qt::Key_4, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(3); });
    shortcut = new QShortcut(dbTabModifier + Qt::Key_5, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(4); });
    shortcut = new QShortcut(dbTabModifier + Qt::Key_6, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(5); });
    shortcut = new QShortcut(dbTabModifier + Qt::Key_7, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(6); });
    shortcut = new QShortcut(dbTabModifier + Qt::Key_8, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(7); });
    shortcut = new QShortcut(dbTabModifier + Qt::Key_9, this);
    connect(shortcut, &QShortcut::activated, [this]() { selectDatabaseTab(m_ui->tabWidget->count() - 1); });

    m_ui->actionDatabaseNew->setIcon(icons()->icon("document-new"));
    m_ui->actionDatabaseOpen->setIcon(icons()->icon("document-open"));
    m_ui->menuRecentDatabases->setIcon(icons()->icon("document-open-recent"));
    m_ui->actionDatabaseSave->setIcon(icons()->icon("document-save"));
    m_ui->actionDatabaseSaveAs->setIcon(icons()->icon("document-save-as"));
    m_ui->actionDatabaseSaveBackup->setIcon(icons()->icon("document-save-copy"));
    m_ui->actionDatabaseClose->setIcon(icons()->icon("document-close"));
    m_ui->actionReports->setIcon(icons()->icon("reports"));
    m_ui->actionDatabaseSettings->setIcon(icons()->icon("database-settings"));
    m_ui->actionDatabaseSecurity->setIcon(icons()->icon("database-change-key"));
    m_ui->actionPasskeys->setIcon(icons()->icon("passkey"));
    m_ui->actionImportPasskey->setIcon(icons()->icon("document-import"));
    m_ui->actionLockDatabase->setIcon(icons()->icon("database-lock"));
    m_ui->actionLockDatabaseToolbar->setIcon(icons()->icon("database-lock"));
    m_ui->actionLockAllDatabases->setIcon(icons()->icon("database-lock-all"));
    m_ui->actionQuit->setIcon(icons()->icon("application-exit"));
    m_ui->actionDatabaseMerge->setIcon(icons()->icon("database-merge"));
    m_ui->menuRemoteSync->setIcon(icons()->icon("remote-sync"));
    m_ui->actionImport->setIcon(icons()->icon("document-import"));
    m_ui->menuExport->setIcon(icons()->icon("document-export"));

#ifndef WITH_XC_BROWSER_PASSKEYS
    m_ui->actionPasskeys->setVisible(false);
    m_ui->actionImportPasskey->setVisible(false);
    m_ui->actionEntryImportPasskey->setVisible(false);
#endif

    m_ui->actionEntryNew->setIcon(icons()->icon("entry-new"));
    m_ui->actionEntryClone->setIcon(icons()->icon("entry-clone"));
    m_ui->actionEntryEdit->setIcon(icons()->icon("entry-edit"));
    m_ui->actionEntryDelete->setIcon(icons()->icon("entry-delete"));
    m_ui->actionEntryRestore->setIcon(icons()->icon("entry-restore"));
    m_ui->actionEntryAutoType->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeSequence->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeUsername->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeUsernameEnter->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypePassword->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypePasswordEnter->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryAutoTypeTOTP->setIcon(icons()->icon("auto-type"));
    m_ui->actionEntryMoveUp->setIcon(icons()->icon("move-up"));
    m_ui->actionEntryMoveDown->setIcon(icons()->icon("move-down"));
    m_ui->actionEntryCopyUsername->setIcon(icons()->icon("username-copy"));
    m_ui->actionEntryCopyPassword->setIcon(icons()->icon("password-copy"));
    m_ui->actionEntryCopyURL->setIcon(icons()->icon("url-copy"));
    m_ui->menuEntryCopyAttribute->setIcon(icons()->icon("attributes-copy"));
    m_ui->menuEntryTotp->setIcon(icons()->icon("totp"));
    m_ui->actionEntryTotp->setIcon(icons()->icon("totp"));
    m_ui->actionEntryCopyTotp->setIcon(icons()->icon("totp-copy"));
    m_ui->actionEntryCopyPasswordTotp->setIcon(icons()->icon("totp-copy-password"));
    m_ui->actionEntryTotpQRCode->setIcon(icons()->icon("qrcode"));
    m_ui->actionEntrySetupTotp->setIcon(icons()->icon("totp-edit"));
    m_ui->actionEntryImportPasskey->setIcon(icons()->icon("document-import"));
    m_ui->actionEntryAddToAgent->setIcon(icons()->icon("utilities-terminal"));
    m_ui->actionEntryRemoveFromAgent->setIcon(icons()->icon("utilities-terminal"));
    m_ui->menuTags->setIcon(icons()->icon("tag-multiple"));
    m_ui->actionEntryDownloadIcon->setIcon(icons()->icon("favicon-download"));
    m_ui->actionGroupSortAsc->setIcon(icons()->icon("sort-alphabetical-ascending"));
    m_ui->actionGroupSortDesc->setIcon(icons()->icon("sort-alphabetical-descending"));

    m_ui->actionGroupNew->setIcon(icons()->icon("group-new"));
    m_ui->actionGroupEdit->setIcon(icons()->icon("group-edit"));
    m_ui->actionGroupClone->setIcon(icons()->icon("group-clone"));
    m_ui->actionGroupDelete->setIcon(icons()->icon("group-delete"));
    m_ui->actionGroupEmptyRecycleBin->setIcon(icons()->icon("group-empty-trash"));
    m_ui->actionEntryOpenUrl->setIcon(icons()->icon("web"));
    m_ui->actionGroupDownloadFavicons->setIcon(icons()->icon("favicon-download"));

    m_ui->actionSettings->setIcon(icons()->icon("configure"));
    m_ui->actionPasswordGenerator->setIcon(icons()->icon("password-generator"));

    m_ui->actionAbout->setIcon(icons()->icon("help-about"));
    m_ui->actionDonate->setIcon(icons()->icon("donate"));
    m_ui->actionBugReport->setIcon(icons()->icon("bugreport"));
    m_ui->actionGettingStarted->setIcon(icons()->icon("getting-started"));
    m_ui->actionUserGuide->setIcon(icons()->icon("user-guide"));
    m_ui->actionOnlineHelp->setIcon(icons()->icon("system-help"));
    m_ui->actionKeyboardShortcuts->setIcon(icons()->icon("keyboard-shortcuts"));
    m_ui->actionCheckForUpdates->setIcon(icons()->icon("system-software-update"));

#ifdef WITH_XC_BROWSER_PASSKEYS
    m_ui->actionPasskeys->setIcon(icons()->icon("passkey"));
    m_ui->actionImportPasskey->setIcon(icons()->icon("document-import"));
    m_ui->actionEntryImportPasskey->setIcon(icons()->icon("document-import"));
    m_ui->actionEntryRemovePasskey->setIcon(icons()->icon("document-close"));
#endif

    m_actionMultiplexer.connect(SIGNAL(currentModeChanged(DatabaseWidget::Mode)), this, SLOT(updateMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(groupChanged()), this, SLOT(updateMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(entrySelectionChanged()), this, SLOT(updateMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(databaseNonDataChanged()), this, SLOT(updateMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(groupContextMenuRequested(QPoint)), this, SLOT(showGroupContextMenu(QPoint)));
    m_actionMultiplexer.connect(SIGNAL(entryContextMenuRequested(QPoint)), this, SLOT(showEntryContextMenu(QPoint)));
    m_actionMultiplexer.connect(SIGNAL(groupChanged()), this, SLOT(updateEntryCountLabel()));
    m_actionMultiplexer.connect(SIGNAL(databaseUnlocked()), this, SLOT(updateEntryCountLabel()));
    m_actionMultiplexer.connect(SIGNAL(databaseModified()), this, SLOT(updateEntryCountLabel()));
    m_actionMultiplexer.connect(SIGNAL(searchModeActivated()), this, SLOT(updateEntryCountLabel()));
    m_actionMultiplexer.connect(SIGNAL(listModeActivated()), this, SLOT(updateEntryCountLabel()));

    // Notify search when the active database changes or gets locked
    connect(m_ui->tabWidget,
            SIGNAL(activeDatabaseChanged(DatabaseWidget*)),
            m_searchWidget,
            SLOT(databaseChanged(DatabaseWidget*)));
    connect(m_ui->tabWidget, SIGNAL(databaseLocked(DatabaseWidget*)), m_searchWidget, SLOT(databaseChanged()));

    connect(m_ui->tabWidget, SIGNAL(tabNameChanged()), SLOT(updateWindowTitle()));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(updateWindowTitle()));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(databaseTabChanged(int)));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(updateMenuActionState()));
    connect(m_ui->tabWidget, SIGNAL(databaseLocked(DatabaseWidget*)), SLOT(databaseStatusChanged(DatabaseWidget*)));
    connect(m_ui->tabWidget, SIGNAL(databaseUnlocked(DatabaseWidget*)), SLOT(databaseStatusChanged(DatabaseWidget*)));
    connect(m_ui->tabWidget, SIGNAL(tabVisibilityChanged(bool)), SLOT(updateToolbarSeparatorVisibility()));
    connect(m_ui->stackedWidget, SIGNAL(currentChanged(int)), SLOT(updateMenuActionState()));
    connect(m_ui->stackedWidget, SIGNAL(currentChanged(int)), SLOT(updateWindowTitle()));
    connect(m_ui->stackedWidget, SIGNAL(currentChanged(int)), SLOT(updateToolbarSeparatorVisibility()));
    connect(m_ui->settingsWidget, SIGNAL(accepted()), SLOT(applySettingsChanges()));
    connect(m_ui->settingsWidget, SIGNAL(settingsReset()), SLOT(applySettingsChanges()));
    connect(m_ui->settingsWidget, SIGNAL(accepted()), SLOT(switchToDatabases()));
    connect(m_ui->settingsWidget, SIGNAL(rejected()), SLOT(switchToDatabases()));

    connect(m_ui->actionDatabaseNew, SIGNAL(triggered()), m_ui->tabWidget, SLOT(newDatabase()));
    connect(m_ui->actionDatabaseOpen, SIGNAL(triggered()), m_ui->tabWidget, SLOT(openDatabase()));
    connect(m_ui->actionDatabaseSave, SIGNAL(triggered()), m_ui->tabWidget, SLOT(saveDatabase()));
    connect(m_ui->actionDatabaseSaveAs, SIGNAL(triggered()), m_ui->tabWidget, SLOT(saveDatabaseAs()));
    connect(m_ui->actionDatabaseSaveBackup, SIGNAL(triggered()), m_ui->tabWidget, SLOT(saveDatabaseBackup()));
    connect(m_ui->actionDatabaseClose, SIGNAL(triggered()), m_ui->tabWidget, SLOT(closeCurrentDatabaseTab()));
    connect(m_ui->actionDatabaseMerge, SIGNAL(triggered()), m_ui->tabWidget, SLOT(mergeDatabase()));
    connect(m_ui->actionDatabaseSettings, SIGNAL(toggled(bool)), m_ui->tabWidget, SLOT(showDatabaseSettings(bool)));
    connect(m_ui->actionDatabaseSecurity, SIGNAL(triggered()), m_ui->tabWidget, SLOT(showDatabaseSecurity()));
    connect(m_ui->actionReports, SIGNAL(toggled(bool)), m_ui->tabWidget, SLOT(showDatabaseReports(bool)));
#ifdef WITH_XC_BROWSER_PASSKEYS
    connect(m_ui->actionPasskeys, SIGNAL(triggered()), m_ui->tabWidget, SLOT(showPasskeys()));
    connect(m_ui->actionImportPasskey, SIGNAL(triggered()), m_ui->tabWidget, SLOT(importPasskey()));
    connect(m_ui->actionEntryImportPasskey, SIGNAL(triggered()), m_ui->tabWidget, SLOT(importPasskeyToEntry()));
    connect(m_ui->actionEntryRemovePasskey, SIGNAL(triggered()), m_ui->tabWidget, SLOT(removePasskeyFromEntry()));
#endif
    connect(m_ui->actionImport, SIGNAL(triggered()), m_ui->tabWidget, SLOT(importFile()));
    connect(m_ui->actionExportCsv, SIGNAL(triggered()), m_ui->tabWidget, SLOT(exportToCsv()));
    connect(m_ui->actionExportHtml, SIGNAL(triggered()), m_ui->tabWidget, SLOT(exportToHtml()));
    connect(m_ui->actionExportXML, SIGNAL(triggered()), m_ui->tabWidget, SLOT(exportToXML()));
    connect(
        m_ui->actionLockDatabase, SIGNAL(triggered()), m_ui->tabWidget, SLOT(lockAndSwitchToFirstUnlockedDatabase()));
    connect(m_ui->actionLockDatabaseToolbar, SIGNAL(triggered()), m_ui->actionLockDatabase, SIGNAL(triggered()));
    connect(m_ui->actionLockAllDatabases, SIGNAL(triggered()), m_ui->tabWidget, SLOT(lockDatabases()));
    connect(m_ui->actionQuit, SIGNAL(triggered()), SLOT(appExit()));

    m_actionMultiplexer.connect(m_ui->actionEntryNew, SIGNAL(triggered()), SLOT(createEntry()));
    m_actionMultiplexer.connect(m_ui->actionEntryClone, SIGNAL(triggered()), SLOT(cloneEntry()));
    m_actionMultiplexer.connect(m_ui->actionEntryEdit, SIGNAL(triggered()), SLOT(switchToEntryEdit()));
    m_actionMultiplexer.connect(m_ui->actionEntryDelete, SIGNAL(triggered()), SLOT(deleteSelectedEntries()));
    m_actionMultiplexer.connect(m_ui->actionEntryRestore, SIGNAL(triggered()), SLOT(restoreSelectedEntries()));

    m_actionMultiplexer.connect(m_ui->actionEntryTotp, SIGNAL(triggered()), SLOT(showTotp()));
    m_actionMultiplexer.connect(m_ui->actionEntrySetupTotp, SIGNAL(triggered()), SLOT(setupTotp()));

    m_actionMultiplexer.connect(m_ui->actionEntryCopyTotp, SIGNAL(triggered()), SLOT(copyTotp()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyPasswordTotp, SIGNAL(triggered()), SLOT(copyPasswordTotp()));
    m_actionMultiplexer.connect(m_ui->actionEntryTotpQRCode, SIGNAL(triggered()), SLOT(showTotpKeyQrCode()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyTitle, SIGNAL(triggered()), SLOT(copyTitle()));
    m_actionMultiplexer.connect(m_ui->actionEntryMoveUp, SIGNAL(triggered()), SLOT(moveEntryUp()));
    m_actionMultiplexer.connect(m_ui->actionEntryMoveDown, SIGNAL(triggered()), SLOT(moveEntryDown()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyUsername, SIGNAL(triggered()), SLOT(copyUsername()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyPassword, SIGNAL(triggered()), SLOT(copyPassword()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyURL, SIGNAL(triggered()), SLOT(copyURL()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyNotes, SIGNAL(triggered()), SLOT(copyNotes()));
    m_actionMultiplexer.connect(m_ui->actionEntryAutoType, SIGNAL(triggered()), SLOT(performAutoType()));
    m_actionMultiplexer.connect(m_ui->actionEntryAutoTypeSequence, SIGNAL(triggered()), SLOT(performAutoType()));
    m_actionMultiplexer.connect(
        m_ui->actionEntryAutoTypeUsername, SIGNAL(triggered()), SLOT(performAutoTypeUsername()));
    m_actionMultiplexer.connect(
        m_ui->actionEntryAutoTypeUsernameEnter, SIGNAL(triggered()), SLOT(performAutoTypeUsernameEnter()));
    m_actionMultiplexer.connect(
        m_ui->actionEntryAutoTypePassword, SIGNAL(triggered()), SLOT(performAutoTypePassword()));
    m_actionMultiplexer.connect(
        m_ui->actionEntryAutoTypePasswordEnter, SIGNAL(triggered()), SLOT(performAutoTypePasswordEnter()));
    m_actionMultiplexer.connect(m_ui->actionEntryAutoTypeTOTP, SIGNAL(triggered()), SLOT(performAutoTypeTOTP()));
    m_actionMultiplexer.connect(m_ui->actionEntryOpenUrl, SIGNAL(triggered()), SLOT(openUrl()));
    m_actionMultiplexer.connect(m_ui->actionEntryDownloadIcon, SIGNAL(triggered()), SLOT(downloadSelectedFavicons()));
#ifdef WITH_XC_SSHAGENT
    m_actionMultiplexer.connect(m_ui->actionEntryAddToAgent, SIGNAL(triggered()), SLOT(addToAgent()));
    m_actionMultiplexer.connect(m_ui->actionEntryRemoveFromAgent, SIGNAL(triggered()), SLOT(removeFromAgent()));
#endif

    m_actionMultiplexer.connect(m_ui->actionGroupNew, SIGNAL(triggered()), SLOT(createGroup()));
    m_actionMultiplexer.connect(m_ui->actionGroupEdit, SIGNAL(triggered()), SLOT(switchToGroupEdit()));
    m_actionMultiplexer.connect(m_ui->actionGroupClone, SIGNAL(triggered()), SLOT(cloneGroup()));
    m_actionMultiplexer.connect(m_ui->actionGroupDelete, SIGNAL(triggered()), SLOT(deleteGroup()));
    m_actionMultiplexer.connect(m_ui->actionGroupEmptyRecycleBin, SIGNAL(triggered()), SLOT(emptyRecycleBin()));
    m_actionMultiplexer.connect(m_ui->actionGroupSortAsc, SIGNAL(triggered()), SLOT(sortGroupsAsc()));
    m_actionMultiplexer.connect(m_ui->actionGroupSortDesc, SIGNAL(triggered()), SLOT(sortGroupsDesc()));
    m_actionMultiplexer.connect(m_ui->actionGroupDownloadFavicons, SIGNAL(triggered()), SLOT(downloadAllFavicons()));

    connect(m_ui->actionSettings, SIGNAL(toggled(bool)), SLOT(switchToSettings(bool)));
    connect(m_ui->actionPasswordGenerator, SIGNAL(toggled(bool)), SLOT(togglePasswordGenerator(bool)));
    connect(m_ui->passwordGeneratorWidget, &PasswordGeneratorWidget::closed, this, [this] {
        togglePasswordGenerator(false);
    });
    m_ui->passwordGeneratorWidget->setStandaloneMode(true);

    connect(m_ui->welcomeWidget, SIGNAL(newDatabase()), SLOT(switchToNewDatabase()));
    connect(m_ui->welcomeWidget, SIGNAL(openDatabase()), SLOT(switchToOpenDatabase()));
    connect(m_ui->welcomeWidget, SIGNAL(openDatabaseFile(QString)), SLOT(switchToDatabaseFile(QString)));
    connect(m_ui->welcomeWidget, SIGNAL(importFile()), m_ui->tabWidget, SLOT(importFile()));

    connect(m_ui->actionAbout, SIGNAL(triggered()), SLOT(showAboutDialog()));
    connect(m_ui->actionDonate, SIGNAL(triggered()), SLOT(openDonateUrl()));
    connect(m_ui->actionBugReport, SIGNAL(triggered()), SLOT(openBugReportUrl()));
    connect(m_ui->actionGettingStarted, SIGNAL(triggered()), SLOT(openGettingStartedGuide()));
    connect(m_ui->actionUserGuide, SIGNAL(triggered()), SLOT(openUserGuide()));
    connect(m_ui->actionOnlineHelp, SIGNAL(triggered()), SLOT(openOnlineHelp()));
    connect(m_ui->actionKeyboardShortcuts, SIGNAL(triggered()), SLOT(openKeyboardShortcuts()));
    connect(m_ui->actionAllowScreenCapture, &QAction::toggled, this, &MainWindow::setAllowScreenCapture);

    connect(osUtils, &OSUtilsBase::statusbarThemeChanged, this, &MainWindow::updateTrayIcon);

    // Install event filter for empty-area drag and menubar toggle
    auto* eventFilter = new MainWindowEventFilter(this);
    m_ui->menubar->installEventFilter(eventFilter);
    m_ui->toolBar->installEventFilter(eventFilter);
    m_ui->tabWidget->tabBar()->installEventFilter(eventFilter);
    installEventFilter(eventFilter);

#ifdef Q_OS_MACOS
    setUnifiedTitleAndToolBarOnMac(true);
#endif

#ifdef WITH_XC_UPDATECHECK
    connect(m_ui->actionCheckForUpdates, SIGNAL(triggered()), SLOT(showUpdateCheckDialog()));
    connect(UpdateChecker::instance(),
            SIGNAL(updateCheckFinished(bool, QString, bool)),
            SLOT(hasUpdateAvailable(bool, QString, bool)));
    // Setup an update check every hour (checked only occur every 7 days)
    connect(&m_updateCheckTimer, &QTimer::timeout, this, &MainWindow::performUpdateCheck);
    m_updateCheckTimer.start(3.6e6);
    // Perform the startup update check after 500 ms
    QTimer::singleShot(500, this, SLOT(performUpdateCheck()));
#else
    m_ui->actionCheckForUpdates->setVisible(false);
#endif

#ifndef WITH_XC_NETWORKING
    m_ui->actionGroupDownloadFavicons->setVisible(false);
    m_ui->actionEntryDownloadIcon->setVisible(false);
#endif
#ifndef WITH_XC_DOCS
    m_ui->actionGettingStarted->setVisible(false);
    m_ui->actionUserGuide->setVisible(false);
    m_ui->actionKeyboardShortcuts->setVisible(false);
#endif

    // clang-format off
    connect(m_ui->tabWidget, SIGNAL(messageGlobal(QString,MessageWidget::MessageType)),
        SLOT(displayGlobalMessage(QString,MessageWidget::MessageType)));
    // clang-format on

    connect(m_ui->tabWidget, SIGNAL(messageDismissGlobal()), this, SLOT(hideGlobalMessage()));

#ifndef Q_OS_HAIKU
    m_screenLockListener = new ScreenLockListener(this);
    connect(m_screenLockListener, SIGNAL(screenLocked()), SLOT(handleScreenLock()));
#endif

    // Tray Icon setup
    connect(Application::instance(), SIGNAL(focusWindowChanged(QWindow*)), SLOT(focusWindowChanged(QWindow*)));
    m_trayIconTriggerReason = QSystemTrayIcon::Unknown;
    m_trayIconTriggerTimer.setSingleShot(true);
    connect(&m_trayIconTriggerTimer, SIGNAL(timeout()), SLOT(processTrayIconTrigger()));

    if (config()->hasAccessError()) {
        m_ui->globalMessageWidget->showMessage(tr("Access error for config file %1").arg(config()->getFileName()),
                                               MessageWidget::Error);
    }

    // Properly shutdown on logoff, restart, and shutdown
    connect(qApp, &QGuiApplication::commitDataRequest, this, [this] { m_appExitCalled = true; });

#if defined(KEEPASSXC_BUILD_TYPE_SNAPSHOT) || defined(KEEPASSXC_BUILD_TYPE_PRE_RELEASE)
    auto* hidePreRelWarn = new QAction(tr("Don't show again for this version"), m_ui->globalMessageWidget);
    m_ui->globalMessageWidget->addAction(hidePreRelWarn);
    auto hidePreRelWarnConn = QSharedPointer<QMetaObject::Connection>::create();
    *hidePreRelWarnConn = connect(m_ui->globalMessageWidget, &KMessageWidget::hideAnimationFinished, [=] {
        m_ui->globalMessageWidget->removeAction(hidePreRelWarn);
        disconnect(*hidePreRelWarnConn);
        hidePreRelWarn->deleteLater();
    });
    connect(hidePreRelWarn, &QAction::triggered, [=] {
        m_ui->globalMessageWidget->animatedHide();
        config()->set(Config::Messages_HidePreReleaseWarning, KEEPASSXC_VERSION);
    });
#endif
#if defined(KEEPASSXC_BUILD_TYPE_SNAPSHOT)
    if (config()->get(Config::Messages_HidePreReleaseWarning) != KEEPASSXC_VERSION) {
        m_ui->globalMessageWidget->showMessage(
            tr("WARNING: You are using an unstable build of KeePassXC.\n"
               "There is a high risk of corruption, maintain a backup of your databases.\n"
               "This version is not meant for production use."),
            MessageWidget::Warning,
            -1);
    }
#elif defined(KEEPASSXC_BUILD_TYPE_PRE_RELEASE)
    if (config()->get(Config::Messages_HidePreReleaseWarning) != KEEPASSXC_VERSION) {
        m_ui->globalMessageWidget->showMessage(
            tr("NOTE: You are using a pre-release version of KeePassXC.\n"
               "Expect some bugs and minor issues, this version is meant for testing purposes."),
            MessageWidget::Information,
            -1);
    }
#endif

    connect(qApp, SIGNAL(anotherInstanceStarted()), this, SLOT(bringToFront()));
    connect(qApp, SIGNAL(applicationActivated()), this, SLOT(bringToFront()));
    connect(qApp, SIGNAL(openFile(QString)), this, SLOT(openDatabase(QString)));
    connect(qApp, SIGNAL(quitSignalReceived()), this, SLOT(appExit()), Qt::DirectConnection);

    // Setup the status bar
    statusBar()->setFixedHeight(24);
    m_progressBarLabel = new QLabel(statusBar());
    m_progressBarLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBarLabel);
    m_progressBar = new QProgressBar(statusBar());
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(false);
    m_progressBar->setMaximumWidth(100);
    m_progressBar->setFixedHeight(15);
    m_progressBar->setMaximum(100);
    statusBar()->addPermanentWidget(m_progressBar);
    connect(clipboard(), &Clipboard::updateCountdown, this, &MainWindow::updateProgressBar);
    m_actionMultiplexer.connect(SIGNAL(updateSyncProgress(int, QString)), this, SLOT(updateProgressBar(int, QString)));
    m_actionMultiplexer.connect(SIGNAL(databaseSyncInProgress()), this, SLOT(disableMenuAndToolbar()));
    m_actionMultiplexer.connect(SIGNAL(databaseSyncCompleted(QString)), this, SLOT(enableMenuAndToolbar()));
    m_actionMultiplexer.connect(SIGNAL(databaseSyncFailed(QString, const QString)), this, SLOT(enableMenuAndToolbar()));
    m_statusBarLabel = new QLabel(statusBar());
    m_statusBarLabel->setObjectName("statusBarLabel");
    statusBar()->addPermanentWidget(m_statusBarLabel);

    restoreConfigState();
    updateMenuActionState();
}

MainWindow::~MainWindow()
{
#ifdef WITH_XC_SSHAGENT
    sshAgent()->removeAllIdentities();
#endif
}

/**
 * Restore the main window's state after launch
 */
void MainWindow::restoreConfigState()
{
    if (config()->get(Config::OpenPreviousDatabasesOnStartup).toBool()) {
        const QStringList fileNames = config()->get(Config::LastOpenedDatabases).toStringList();
        for (const QString& filename : fileNames) {
            if (!filename.isEmpty() && QFile::exists(filename)) {
                openDatabase(filename);
            }
        }
        auto lastActiveFile = config()->get(Config::LastActiveDatabase).toString();
        if (!lastActiveFile.isEmpty()) {
            openDatabase(lastActiveFile);
        }
    }
}

QList<DatabaseWidget*> MainWindow::getOpenDatabases()
{
    QList<DatabaseWidget*> dbWidgets;
    for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
        dbWidgets << m_ui->tabWidget->databaseWidgetFromIndex(i);
    }
    return dbWidgets;
}

void MainWindow::showErrorMessage(const QString& message)
{
    m_ui->globalMessageWidget->showMessage(message, MessageWidget::Error);
}

void MainWindow::appExit()
{
    m_appExitCalled = true;
    close();
}

/**
 * Returns if application was built with hardware key support.
 * Intended to be used by 3rd-party applications using DBus.
 *
 * @return True if built with hardware key support, false otherwise
 */
bool MainWindow::isHardwareKeySupported()
{
#ifdef WITH_XC_YUBIKEY
    return true;
#else
    return false;
#endif
}

/**
 * Refreshes list of hardware keys known.
 * Triggers the DatabaseOpenWidget to automatically select the key last used for a database if found.
 * Intended to be used by 3rd-party applications using DBus.
 *
 * @return True if any key was found, false otherwise or if application lacks hardware key support
 */
bool MainWindow::refreshHardwareKeys()
{
#ifdef WITH_XC_YUBIKEY
    auto yk = YubiKey::instance();
    // find keys sync to allow returning if any key was found
    bool found = yk->findValidKeys();
    // emit signal so DatabaseOpenWidget can select last used key
    // emit here manually because sync findValidKeys() cannot do that properly
    emit yk->detectComplete(found);
    return found;
#else
    return false;
#endif
}

void MainWindow::updateLastDatabasesMenu()
{
    m_ui->menuRecentDatabases->clear();

    const QStringList lastDatabases = config()->get(Config::LastDatabases).toStringList();
    for (const QString& database : lastDatabases) {
        QAction* action = m_ui->menuRecentDatabases->addAction(database);
        action->setData(database);
        m_lastDatabasesActions->addAction(action);
    }
    m_ui->menuRecentDatabases->addSeparator();
    m_ui->menuRecentDatabases->addAction(m_clearHistoryAction);
}

void MainWindow::updateCopyAttributesMenu()
{
    DatabaseWidget* dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (!dbWidget) {
        return;
    }

    if (dbWidget->numberOfSelectedEntries() != 1) {
        return;
    }

    QList<QAction*> actions = m_ui->menuEntryCopyAttribute->actions();
    for (int i = m_countDefaultAttributes; i < actions.size(); i++) {
        delete actions[i];
    }

    const QStringList customEntryAttributes = dbWidget->customEntryAttributes();
    for (const QString& key : customEntryAttributes) {
        QAction* action = m_ui->menuEntryCopyAttribute->addAction(key);
        action->setData(QVariant(key));
        m_copyAdditionalAttributeActions->addAction(action);
    }
}

void MainWindow::updateSetTagsMenu()
{
    // Remove all existing actions
    m_ui->menuTags->clear();

    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (dbWidget) {
        // Enumerate tags applied to the selected entries
        QSet<QString> selectedTags;
        for (auto entry : dbWidget->entryView()->selectedEntries()) {
            for (auto tag : entry->tagList()) {
                selectedTags.insert(tag);
            }
        }

        // Add known database tags as actions and set checked if
        // a selected entry has that tag
        for (auto tag : dbWidget->database()->tagList()) {
            auto action = m_ui->menuTags->addAction(icons()->icon("tag"), tag);
            action->setCheckable(true);
            action->setChecked(selectedTags.contains(tag));
            m_setTagsMenuActions->addAction(action);
        }
    }

    // If no tags exist in the database then show a tip to the user
    if (m_ui->menuTags->isEmpty()) {
        auto action = m_ui->menuTags->addAction(tr("No Tags"));
        action->setEnabled(false);
    }
}

void MainWindow::openRecentDatabase(QAction* action)
{
    openDatabase(action->data().toString());
}

void MainWindow::clearLastDatabases()
{
    config()->remove(Config::LastDatabases);
    bool inWelcomeWidget = (m_ui->stackedWidget->currentIndex() == 2);

    if (inWelcomeWidget) {
        m_ui->welcomeWidget->refreshLastDatabases();
    }
}

void MainWindow::openDatabase(const QString& filePath, const QString& password, const QString& keyfile)
{
    m_ui->tabWidget->addDatabaseTab(filePath, false, password, keyfile);
}

void MainWindow::updateMenuActionState()
{
    // MainWindow State
    int currentIndex = m_ui->stackedWidget->currentIndex();
    bool hasLockableDatabase = m_ui->tabWidget->hasLockableDatabases();
    bool inAppSettings = (currentIndex == SettingsScreen);
    bool inPasswordGenerator = (currentIndex == PasswordGeneratorScreen);

    auto dbWidget = (currentIndex == DatabaseTabScreen ? m_ui->tabWidget->currentDatabaseWidget() : nullptr);
    auto dbMode = (dbWidget ? dbWidget->currentMode() : DatabaseWidget::Mode::None);

    // Database State
    bool databaseUnlocked = (dbWidget && !dbWidget->isLocked());
    bool inDatabase = (dbMode == DatabaseWidget::Mode::ViewMode);
    bool inDatabaseSettings = (dbMode == DatabaseWidget::Mode::DatabaseSettingsMode);
    bool inReports = (dbMode == DatabaseWidget::Mode::ReportsMode);
    bool editingEntry = (dbMode == DatabaseWidget::Mode::EditEntryMode);

    // Synchronize toggle buttons
    m_ui->actionDatabaseSettings->blockSignals(true);
    m_ui->actionPasswordGenerator->blockSignals(true);
    m_ui->actionReports->blockSignals(true);
    m_ui->actionSettings->blockSignals(true);

    m_ui->actionDatabaseSettings->setChecked(inDatabaseSettings);
    m_ui->actionPasswordGenerator->setChecked(inPasswordGenerator);
    m_ui->actionReports->setChecked(inReports);
    m_ui->actionSettings->setChecked(inAppSettings);

    m_ui->actionDatabaseSettings->blockSignals(false);
    m_ui->actionPasswordGenerator->blockSignals(false);
    m_ui->actionReports->blockSignals(false);
    m_ui->actionSettings->blockSignals(false);

    // Entry State
    bool singleEntrySelected = (inDatabase && dbWidget->numberOfSelectedEntries() == 1);
    bool singleEntryOrEditing = (singleEntrySelected || editingEntry);
    bool multiEntrySelected = (inDatabase && dbWidget->numberOfSelectedEntries() > 0);

    // Group State
    bool groupSelected = (inDatabase && dbWidget->isGroupSelected());
    bool groupHasChildren = (groupSelected && dbWidget->currentGroup()->hasChildren());
    bool groupHasEntries = (groupSelected && !dbWidget->currentGroup()->entries().isEmpty());
    bool inRecycleBin = (inDatabase && dbWidget->isRecycleBinSelected());

    bool entryViewSorted = (inDatabase && dbWidget->isSorted());
    bool entryViewAtTop = (inDatabase && dbWidget->currentEntryIndex() == 0);
    bool entryViewAtBottom =
        (groupSelected && dbWidget->currentEntryIndex() == dbWidget->currentGroup()->entries().size() - 1);

    m_ui->actionEntryNew->setEnabled(inDatabase && !inRecycleBin);
    m_ui->actionEntryClone->setEnabled(singleEntrySelected && !inRecycleBin);
    m_ui->actionEntryEdit->setEnabled(singleEntrySelected);
    m_ui->actionEntryDelete->setEnabled(multiEntrySelected);
    m_ui->actionEntryRestore->setVisible(multiEntrySelected && inRecycleBin);
    m_ui->actionEntryRestore->setEnabled(multiEntrySelected && inRecycleBin);
    if (dbWidget) {
        m_ui->actionEntryRestore->setText(tr("Restore Entry(s)", "", dbWidget->numberOfSelectedEntries()));
        m_ui->actionEntryRestore->setToolTip(tr("Restore Entry(s)", "", dbWidget->numberOfSelectedEntries()));
    }
    m_ui->actionEntryMoveUp->setVisible(inDatabase && !entryViewSorted);
    m_ui->actionEntryMoveDown->setVisible(inDatabase && !entryViewSorted);
    m_ui->actionEntryMoveUp->setEnabled(singleEntrySelected && !entryViewSorted && !entryViewAtTop);
    m_ui->actionEntryMoveDown->setEnabled(singleEntrySelected && !entryViewSorted && !entryViewAtBottom);
    m_ui->actionEntryCopyTitle->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasTitle());
    m_ui->actionEntryCopyUsername->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasUsername());
    // NOTE: Copy password is enabled even if the selected entry's password is blank to prevent Ctrl+C
    // from copying information from the currently selected cell in the entry view table.
    m_ui->actionEntryCopyPassword->setEnabled(singleEntryOrEditing);
    m_ui->actionEntryCopyURL->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasUrl());
    m_ui->actionEntryCopyNotes->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasNotes());
    m_ui->menuEntryCopyAttribute->setEnabled(singleEntryOrEditing);
    m_ui->menuEntryTotp->setEnabled(singleEntrySelected);
    m_ui->menuTags->setEnabled(multiEntrySelected);
    m_ui->actionEntryAutoType->setEnabled(singleEntrySelected && dbWidget->currentEntryHasAutoTypeEnabled());
    m_ui->actionEntryAutoType->menu()->setEnabled(singleEntrySelected && dbWidget->currentEntryHasAutoTypeEnabled());
    m_ui->actionEntryAutoTypeSequence->setText(singleEntrySelected
                                                   ? dbWidget->currentSelectedEntry()->effectiveAutoTypeSequence()
                                                   : Group::RootAutoTypeSequence);
    m_ui->actionEntryAutoTypeSequence->setEnabled(singleEntrySelected);
    m_ui->actionEntryAutoTypeUsername->setEnabled(singleEntrySelected && dbWidget->currentEntryHasUsername());
    m_ui->actionEntryAutoTypeUsernameEnter->setEnabled(singleEntrySelected && dbWidget->currentEntryHasUsername());
    m_ui->actionEntryAutoTypePassword->setEnabled(singleEntrySelected && dbWidget->currentEntryHasPassword());
    m_ui->actionEntryAutoTypePasswordEnter->setEnabled(singleEntrySelected && dbWidget->currentEntryHasPassword());
    m_ui->actionEntryAutoTypeTOTP->setEnabled(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntryAutoTypeTOTP->setVisible(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntryOpenUrl->setEnabled(singleEntryOrEditing && dbWidget->currentEntryHasUrl());
    m_ui->actionEntryTotp->setEnabled(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntryCopyTotp->setEnabled(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntryCopyPasswordTotp->setEnabled(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntrySetupTotp->setEnabled(singleEntrySelected);
    m_ui->actionEntryTotpQRCode->setEnabled(singleEntrySelected && dbWidget->currentEntryHasTotp());
    m_ui->actionEntryDownloadIcon->setEnabled((multiEntrySelected && !singleEntrySelected)
                                              || (singleEntrySelected && dbWidget->currentEntryHasUrl()));
#ifdef WITH_XC_BROWSER_PASSKEYS
    m_ui->actionEntryImportPasskey->setVisible(singleEntrySelected);
    m_ui->actionEntryImportPasskey->setEnabled(singleEntrySelected);
    m_ui->actionEntryRemovePasskey->setVisible(singleEntrySelected && dbWidget->currentEntryHasPasskey());
    m_ui->actionEntryRemovePasskey->setEnabled(singleEntrySelected && dbWidget->currentEntryHasPasskey());
#endif
#ifdef WITH_XC_SSHAGENT
    bool hasSSHKey = singleEntrySelected && sshAgent()->isEnabled() && dbWidget->currentEntryHasSshKey();
    m_ui->actionEntryAddToAgent->setVisible(hasSSHKey);
    m_ui->actionEntryAddToAgent->setEnabled(hasSSHKey);
    m_ui->actionEntryRemoveFromAgent->setVisible(hasSSHKey);
    m_ui->actionEntryRemoveFromAgent->setEnabled(hasSSHKey);
#endif

    m_ui->actionGroupNew->setEnabled(groupSelected && !inRecycleBin);
    m_ui->actionGroupEdit->setEnabled(groupSelected);
    m_ui->actionGroupClone->setEnabled(groupSelected && dbWidget->canCloneCurrentGroup());
    m_ui->actionGroupDelete->setEnabled(groupSelected && dbWidget->canDeleteCurrentGroup());
    m_ui->actionGroupSortAsc->setVisible(groupHasChildren);
    m_ui->actionGroupSortAsc->setEnabled(groupHasChildren);
    m_ui->actionGroupSortDesc->setVisible(groupHasChildren);
    m_ui->actionGroupSortDesc->setEnabled(groupHasChildren);
    m_ui->actionGroupEmptyRecycleBin->setVisible(inRecycleBin);
    m_ui->actionGroupEmptyRecycleBin->setEnabled(inRecycleBin);
#ifdef WITH_XC_NETWORKING
    m_ui->actionGroupDownloadFavicons->setVisible(!inRecycleBin);
#endif
    m_ui->actionGroupDownloadFavicons->setEnabled(groupSelected && groupHasEntries && !inRecycleBin);

    // Database Menu
    m_ui->actionDatabaseSave->setEnabled(m_ui->tabWidget->canSave());
    m_ui->actionDatabaseSaveAs->setEnabled(databaseUnlocked);
    m_ui->actionDatabaseSaveBackup->setEnabled(databaseUnlocked);
    m_ui->actionDatabaseClose->setEnabled(dbWidget);
    m_ui->actionLockDatabase->setEnabled(databaseUnlocked);
    m_ui->actionLockAllDatabases->setEnabled(hasLockableDatabase);
    m_ui->actionLockDatabaseToolbar->setEnabled(hasLockableDatabase);
    m_ui->actionDatabaseSettings->setEnabled(inDatabase || inDatabaseSettings);
    m_ui->actionDatabaseSecurity->setEnabled(inDatabase || inDatabaseSettings);
    m_ui->actionReports->setEnabled(inDatabase || inReports);
    m_ui->menuRemoteSync->setEnabled(inDatabase || inDatabaseSettings);
    m_ui->menuExport->setEnabled(inDatabase);
    m_ui->actionDatabaseMerge->setEnabled(inDatabase);
#ifdef WITH_XC_BROWSER_PASSKEYS
    m_ui->actionPasskeys->setEnabled(inDatabase || inReports);
    m_ui->actionImportPasskey->setEnabled(inDatabase);
#endif

    m_searchWidgetAction->setEnabled(inDatabase);
}

void MainWindow::updateToolbarSeparatorVisibility()
{
    if (!m_showToolbarSeparator) {
        m_ui->toolbarSeparator->setVisible(false);
        return;
    }

    switch (m_ui->stackedWidget->currentIndex()) {
    case DatabaseTabScreen:
        m_ui->toolbarSeparator->setVisible(!m_ui->tabWidget->tabBar()->isVisible()
                                           && m_ui->tabWidget->tabBar()->count() == 1);
        break;
    case SettingsScreen:
        m_ui->toolbarSeparator->setVisible(true);
        break;
    default:
        m_ui->toolbarSeparator->setVisible(false);
    }
}

void MainWindow::updateWindowTitle()
{
    QString customWindowTitlePart;
    int stackedWidgetIndex = m_ui->stackedWidget->currentIndex();
    int tabWidgetIndex = m_ui->tabWidget->currentIndex();
    bool isModified = m_ui->tabWidget->isModified(tabWidgetIndex);

    if (stackedWidgetIndex == DatabaseTabScreen && tabWidgetIndex != -1) {
        customWindowTitlePart = m_ui->tabWidget->tabName(tabWidgetIndex);
        if (isModified) {
            // remove asterisk '*' from title
            customWindowTitlePart.remove(customWindowTitlePart.size() - 1, 1);
        }
        m_ui->actionDatabaseSave->setEnabled(m_ui->tabWidget->canSave(tabWidgetIndex));
    } else if (stackedWidgetIndex == 1) {
        customWindowTitlePart = tr("Settings");
    }

    QString windowTitle;
    if (customWindowTitlePart.isEmpty()) {
        windowTitle = BaseWindowTitle;
    } else {
        windowTitle = QString("%1[*] - %2").arg(customWindowTitlePart, BaseWindowTitle);
    }

    if (customWindowTitlePart.isEmpty() || stackedWidgetIndex == 1) {
        setWindowFilePath("");
    } else {
        setWindowFilePath(m_ui->tabWidget->databaseWidgetFromIndex(tabWidgetIndex)->database()->filePath());
    }

    setWindowTitle(windowTitle);
    setWindowModified(isModified);

    updateTrayIcon();
}

void MainWindow::showAboutDialog()
{
    auto* aboutDialog = new AboutDialog(this);
    // Auto close the about dialog before attempting database locks
    if (m_ui->tabWidget->currentDatabaseWidget()) {
        connect(m_ui->tabWidget->currentDatabaseWidget(),
                &DatabaseWidget::databaseLockRequested,
                aboutDialog,
                &AboutDialog::close);
    }
    aboutDialog->open();
}

void MainWindow::performUpdateCheck()
{
#ifdef WITH_XC_UPDATECHECK
    if (!config()->get(Config::UpdateCheckMessageShown).toBool()) {
        auto result =
            MessageBox::question(this,
                                 tr("Check for updates on startup?"),
                                 tr("Would you like KeePassXC to check for updates on startup?") + "\n\n"
                                     + tr("You can always check for updates manually from the application menu."),
                                 MessageBox::Yes | MessageBox::No,
                                 MessageBox::Yes);

        config()->set(Config::GUI_CheckForUpdates, (result == MessageBox::Yes));
        config()->set(Config::UpdateCheckMessageShown, true);
    }

    if (config()->get(Config::GUI_CheckForUpdates).toBool()) {
        updateCheck()->checkForUpdates(false);
    }

#endif
}

void MainWindow::hasUpdateAvailable(bool hasUpdate, const QString& version, bool isManuallyRequested)
{
#ifdef WITH_XC_UPDATECHECK
    if (hasUpdate && !isManuallyRequested) {
        auto* updateCheckDialog = new UpdateCheckDialog(this);
        updateCheckDialog->showUpdateCheckResponse(hasUpdate, version);
        updateCheckDialog->show();
    }
#else
    Q_UNUSED(hasUpdate)
    Q_UNUSED(version)
    Q_UNUSED(isManuallyRequested)
#endif
}

void MainWindow::showUpdateCheckDialog()
{
#ifdef WITH_XC_UPDATECHECK
    updateCheck()->checkForUpdates(true);
    auto* updateCheckDialog = new UpdateCheckDialog(this);
    updateCheckDialog->show();
#endif
}

void MainWindow::customOpenUrl(QString url)
{
#ifdef KEEPASSXC_DIST_APPIMAGE
    QProcess::execute("xdg-open", {url});
#else
    QDesktopServices::openUrl(QUrl(url));
#endif
}

void MainWindow::openDonateUrl()
{
    customOpenUrl("https://keepassxc.org/donate");
}

void MainWindow::openBugReportUrl()
{
    customOpenUrl("https://github.com/keepassxreboot/keepassxc/issues");
}

void MainWindow::openGettingStartedGuide()
{
    customOpenUrl(QString("file:///%1").arg(resources()->dataPath("docs/KeePassXC_GettingStarted.html")));
}

void MainWindow::openUserGuide()
{
    customOpenUrl(QString("file:///%1").arg(resources()->dataPath("docs/KeePassXC_UserGuide.html")));
}

void MainWindow::openOnlineHelp()
{
    customOpenUrl("https://keepassxc.org/docs/");
}

void MainWindow::openKeyboardShortcuts()
{
    customOpenUrl(QString("file:///%1").arg(resources()->dataPath("docs/KeePassXC_KeyboardShortcuts.html")));
}

void MainWindow::switchToDatabases()
{
    if (m_ui->tabWidget->currentIndex() == -1) {
        m_ui->stackedWidget->setCurrentIndex(WelcomeScreen);
    } else {
        m_ui->stackedWidget->setCurrentIndex(DatabaseTabScreen);
    }
}

void MainWindow::switchToSettings(bool enabled)
{
    if (enabled) {
        m_ui->settingsWidget->loadSettings();
        m_ui->stackedWidget->setCurrentIndex(SettingsScreen);
    } else {
        switchToDatabases();
    }
}

void MainWindow::togglePasswordGenerator(bool enabled)
{
    if (enabled) {
        m_ui->passwordGeneratorWidget->loadSettings();
        m_ui->passwordGeneratorWidget->regeneratePassword();
        m_ui->stackedWidget->setCurrentIndex(PasswordGeneratorScreen);
    } else {
        m_ui->passwordGeneratorWidget->saveSettings();
        switchToDatabases();
    }
}

void MainWindow::switchToNewDatabase()
{
    m_ui->tabWidget->newDatabase();
    switchToDatabases();
}

void MainWindow::switchToOpenDatabase()
{
    m_ui->tabWidget->openDatabase();
    switchToDatabases();
}

void MainWindow::switchToDatabaseFile(const QString& file)
{
    m_ui->tabWidget->addDatabaseTab(file);
    switchToDatabases();
}

void MainWindow::updateRemoteSyncMenuEntries()
{
    m_ui->menuRemoteSync->clear();

    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (dbWidget) {
        // Setup sync shortcut
        auto action = m_ui->menuRemoteSync->addAction(tr("Setup Remote Sync…"));
        connect(action, &QAction::triggered, dbWidget, &DatabaseWidget::switchToRemoteSettings);

        m_ui->menuRemoteSync->addSeparator();

        // Build remote sync menu
        for (const auto params : dbWidget->getRemoteParams()) {
            auto* remoteSyncAction = new QAction(params->name, this);
            m_ui->menuRemoteSync->addAction(remoteSyncAction);
            connect(remoteSyncAction, &QAction::triggered, dbWidget, [=] { dbWidget->syncWithRemote(params); });
        }
    }
}

void MainWindow::databaseStatusChanged(DatabaseWidget* dbWidget)
{
    Q_UNUSED(dbWidget);
    updateTrayIcon();
}

/**
 * Select a database tab by its index. Stays bounded to first/last tab
 * on overflow unless wrap is true.
 *
 * @param tabIndex 0-based tab index selector
 * @param wrap if true wrap around to first/last tab
 */
void MainWindow::selectDatabaseTab(int tabIndex, bool wrap)
{
    if (m_ui->stackedWidget->currentIndex() == DatabaseTabScreen) {
        if (wrap) {
            if (tabIndex < 0) {
                tabIndex = m_ui->tabWidget->count() - 1;
            } else if (tabIndex >= m_ui->tabWidget->count()) {
                tabIndex = 0;
            }
        } else {
            tabIndex = qBound(0, tabIndex, m_ui->tabWidget->count() - 1);
        }

        m_ui->tabWidget->setCurrentIndex(tabIndex);
    }
}

void MainWindow::selectNextDatabaseTab()
{
    selectDatabaseTab(m_ui->tabWidget->currentIndex() + 1, true);
}

void MainWindow::selectPreviousDatabaseTab()
{
    selectDatabaseTab(m_ui->tabWidget->currentIndex() - 1, true);
}

void MainWindow::databaseTabChanged(int tabIndex)
{
    if (tabIndex != -1 && m_ui->stackedWidget->currentIndex() == WelcomeScreen) {
        m_ui->stackedWidget->setCurrentIndex(DatabaseTabScreen);
    } else if (tabIndex == -1 && m_ui->stackedWidget->currentIndex() == DatabaseTabScreen) {
        m_ui->stackedWidget->setCurrentIndex(WelcomeScreen);
    }

    m_actionMultiplexer.setCurrentObject(m_ui->tabWidget->currentDatabaseWidget());
    updateEntryCountLabel();
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        const auto keyevent = static_cast<QKeyEvent*>(event);
        // Did we get a ShortcutOverride event with the same key sequence as the OS default
        // copy-to-clipboard shortcut?
        if (keyevent->matches(QKeySequence::Copy)) {
            // If so, we ask the database widget to check if any of its sub-widgets has
            // text selected, and to copy it to the clipboard if that is the case.
            // We do this because that is what the user likely expects to happen, yet Qt does not
            // behave like that (at least on some platforms).
            auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
            if (dbWidget && dbWidget->copyFocusedTextSelection()) {
                // Note: instead of actively copying the selected text to the clipboard
                // above, simply accepting the event would have a similar effect (Qt
                // would deliver it as a key press to the current widget, which would
                // trigger the built-in copy-to-clipboard behaviour). However, that
                // would not come with our special (configurable) behaviour of
                // clearing the clipboard after a certain time period.
                keyevent->accept();
                return true;
            }
        }
    }
    return QMainWindow::event(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    Q_UNUSED(event)
#ifdef Q_OS_WIN
    // Qt Hack - Prevent white flicker when showing window
    QTimer::singleShot(50, this, [=] { setProperty("windowOpacity", 1.0); });
#endif
}

void MainWindow::hideEvent(QHideEvent* event)
{
    Q_UNUSED(event)
#ifdef Q_OS_WIN
    // Qt Hack - Prevent white flicker when showing window
    setProperty("windowOpacity", 0.0);
#endif
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_appExiting) {
        event->accept();
        return;
    }

    // Ignore event and hide to tray if this is not an actual close
    // request by the system's session manager.
    if (config()->get(Config::GUI_MinimizeOnClose).toBool() && !m_appExitCalled && !isHidden()
        && !qApp->isSavingSession()) {
        event->ignore();
        hideWindow();
        return;
    }

    m_appExiting = saveLastDatabases();
    if (m_appExiting) {
        saveWindowInformation();
        event->accept();
        m_restartRequested ? kpxcApp->restart() : QApplication::quit();
        return;
    }

    m_appExitCalled = false;
    m_restartRequested = false;
    event->ignore();
}

void MainWindow::changeEvent(QEvent* event)
{
    if ((event->type() == QEvent::WindowStateChange) && isMinimized()) {
        if (isTrayIconEnabled() && config()->get(Config::GUI_MinimizeToTray).toBool()) {
            event->ignore();
            hide();
        }

        if (config()->get(Config::Security_LockDatabaseMinimize).toBool()) {
            m_ui->tabWidget->lockDatabasesDelayed();
        }
    } else {
        QMainWindow::changeEvent(event);
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (!event->modifiers()) {
        // Allow for direct focus of search, group view, and entry view
        auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
        if (dbWidget && dbWidget->isEntryViewActive()) {
            if (event->key() == Qt::Key_F1) {
                dbWidget->focusOnGroups(true);
                return;
            } else if (event->key() == Qt::Key_F2) {
                dbWidget->focusOnEntries(true);
                return;
            } else if (event->key() == Qt::Key_F3 || event->key() == Qt::Key_F6) {
                focusSearchWidget();
                return;
            } else if (event->key() == Qt::Key_Escape && dbWidget->isSearchActive()) {
                m_searchWidget->clearSearch();
                return;
            }
        }
    }

    QMainWindow::keyPressEvent(event);
}

bool MainWindow::focusNextPrevChild(bool next)
{
    // Only navigate around the main window if the database widget is showing the entry view
    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (dbWidget && dbWidget->isVisible() && dbWidget->isEntryViewActive()) {
        // Search Widget <-> Tab Widget <-> DbWidget
        if (next) {
            if (m_searchWidget->hasFocus()) {
                if (m_ui->tabWidget->count() > 1) {
                    m_ui->tabWidget->setFocus(Qt::TabFocusReason);
                } else {
                    dbWidget->setFocus(Qt::TabFocusReason);
                }
            } else if (m_ui->tabWidget->hasFocus()) {
                dbWidget->setFocus(Qt::TabFocusReason);
            } else {
                focusSearchWidget();
            }
        } else {
            if (m_searchWidget->hasFocus()) {
                dbWidget->setFocus(Qt::BacktabFocusReason);
            } else if (m_ui->tabWidget->hasFocus()) {
                focusSearchWidget();
            } else {
                if (m_ui->tabWidget->count() > 1) {
                    m_ui->tabWidget->setFocus(Qt::BacktabFocusReason);
                } else {
                    focusSearchWidget();
                }
            }
        }
        return true;
    }

    // Defer to Qt to make a decision, this maintains normal behavior
    return QMainWindow::focusNextPrevChild(next);
}

void MainWindow::focusSearchWidget()
{
    if (m_searchWidgetAction->isEnabled()) {
        m_ui->toolBar->setVisible(true);
        m_ui->toolBar->setExpanded(true);
        m_searchWidget->focusSearch();
    }
}

void MainWindow::enableMenuAndToolbar()
{
    m_ui->toolBar->setDisabled(false);
    m_ui->menubar->setDisabled(false);
}

void MainWindow::disableMenuAndToolbar()
{
    m_ui->toolBar->setDisabled(true);
    m_ui->menubar->setDisabled(true);
}

void MainWindow::saveWindowInformation()
{
    if (isVisible()) {
        config()->set(Config::GUI_MainWindowGeometry, saveGeometry());
        config()->set(Config::GUI_MainWindowState, saveState());
    }
}

bool MainWindow::saveLastDatabases()
{
    if (config()->get(Config::OpenPreviousDatabasesOnStartup).toBool()) {
        auto currentDbWidget = m_ui->tabWidget->currentDatabaseWidget();
        if (currentDbWidget && !currentDbWidget->database()->isTemporaryDatabase()) {
            config()->set(Config::LastActiveDatabase, currentDbWidget->database()->filePath());
        } else {
            config()->remove(Config::LastActiveDatabase);
        }

        QStringList openDatabases;
        for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
            auto dbWidget = m_ui->tabWidget->databaseWidgetFromIndex(i);
            if (!dbWidget->database()->isTemporaryDatabase()) {
                openDatabases.append(QDir::toNativeSeparators(dbWidget->database()->filePath()));
            }
        }

        config()->set(Config::LastOpenedDatabases, openDatabases);
    } else {
        config()->remove(Config::LastActiveDatabase);
        config()->remove(Config::LastOpenedDatabases);
    }

    return m_ui->tabWidget->closeAllDatabaseTabs();
}

void MainWindow::updateTrayIcon()
{
    if (config()->get(Config::GUI_ShowTrayIcon).toBool()) {
        if (!m_trayIcon) {
            m_trayIcon = new QSystemTrayIcon(this);
            auto* menu = new QMenu(this);

            auto* actionToggle = new QAction(tr("Toggle window"), menu);
            menu->addAction(actionToggle);
            actionToggle->setIcon(icons()->icon("keepassxc-monochrome-dark"));

            menu->addAction(m_ui->actionLockAllDatabases);

#ifdef Q_OS_MACOS
            auto actionQuit = new QAction(tr("Quit KeePassXC"), menu);
            connect(actionQuit, SIGNAL(triggered()), SLOT(appExit()));
            menu->addAction(actionQuit);
#else
            menu->addAction(m_ui->actionQuit);
#endif
            m_trayIcon->setContextMenu(menu);

            connect(m_trayIcon,
                    SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
                    SLOT(trayIconTriggered(QSystemTrayIcon::ActivationReason)));
            connect(actionToggle, SIGNAL(triggered()), SLOT(toggleWindow()));
        }

        bool showUnlocked = m_ui->tabWidget->hasLockableDatabases();
        m_trayIcon->setIcon(icons()->trayIcon(showUnlocked));
        m_trayIcon->setToolTip(windowTitle().replace("[*]", isWindowModified() ? "*" : ""));
        m_trayIcon->show();

        if (!isTrayIconEnabled() || !QSystemTrayIcon::isSystemTrayAvailable()) {
            // Try to show tray icon after 5 seconds, try 5 times
            // This can happen if KeePassXC starts before the system tray is available
            static int trayIconAttempts = 0;
            if (trayIconAttempts < 5) {
                QTimer::singleShot(5000, this, &MainWindow::updateTrayIcon);
                ++trayIconAttempts;
            }
        }
    } else {
        if (m_trayIcon) {
            m_trayIcon->hide();
            delete m_trayIcon;
        }
    }

    QApplication::setQuitOnLastWindowClosed(!isTrayIconEnabled());
}

void MainWindow::updateProgressBar(int percentage, QString message)
{
    if (percentage < 0) {
        m_progressBar->setVisible(false);
        m_progressBarLabel->setVisible(false);
    } else {
        m_progressBar->setValue(percentage);
        m_progressBar->setVisible(true);
        m_progressBarLabel->setText(message);
        m_progressBarLabel->setVisible(true);
    }
}

void MainWindow::updateEntryCountLabel()
{
    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (dbWidget && dbWidget->currentMode() == DatabaseWidget::Mode::ViewMode) {
        int numEntries = dbWidget->entryView()->model()->rowCount();
        m_statusBarLabel->setText(tr("%1 Entry(s)", "", numEntries).arg(numEntries));
    } else {
        m_statusBarLabel->setText("");
    }
}

void MainWindow::obtainContextFocusLock()
{
    m_contextMenuFocusLock = true;
}

void MainWindow::releaseContextFocusLock()
{
    m_contextMenuFocusLock = false;
}

void MainWindow::agentEnabled(bool enabled)
{
    m_ui->actionEntryAddToAgent->setVisible(enabled);
    m_ui->actionEntryRemoveFromAgent->setVisible(enabled);
}

void MainWindow::showEntryContextMenu(const QPoint& globalPos)
{
    bool entrySelected = false;
    auto dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (dbWidget) {
        entrySelected = dbWidget->numberOfSelectedEntries() > 0;
    }

    if (entrySelected) {
        m_entryContextMenu->popup(globalPos);
    } else {
        m_entryNewContextMenu->popup(globalPos);
    }
}

void MainWindow::showGroupContextMenu(const QPoint& globalPos)
{
    m_ui->menuGroups->popup(globalPos);
}

void MainWindow::applySettingsChanges()
{
    int timeout = config()->get(Config::Security_LockDatabaseIdleSeconds).toInt() * 1000;
    if (timeout <= 0) {
        timeout = 60;
    }

    m_inactivityTimer->setInactivityTimeout(timeout);
    if (config()->get(Config::Security_LockDatabaseIdle).toBool()) {
        m_inactivityTimer->activate();
    } else {
        m_inactivityTimer->deactivate();
    }

    m_ui->actionShowToolbar->setChecked(!config()->get(Config::GUI_HideToolbar).toBool());
    m_ui->actionShowMenubar->setChecked(!config()->get(Config::GUI_HideMenubar).toBool());
    m_ui->menubar->setHidden(config()->get(Config::GUI_HideMenubar).toBool());
    m_ui->toolBar->setHidden(config()->get(Config::GUI_HideToolbar).toBool());
    auto movable = config()->get(Config::GUI_MovableToolbar).toBool();
    m_ui->toolBar->setMovable(movable);
    if (!movable) {
        // Move the toolbar back to the top of the main window
        addToolBar(Qt::TopToolBarArea, m_ui->toolBar);
    }

    bool isOk = false;
    const auto toolButtonStyle =
        static_cast<Qt::ToolButtonStyle>(config()->get(Config::GUI_ToolButtonStyle).toInt(&isOk));
    if (isOk) {
        m_ui->toolBar->setToolButtonStyle(toolButtonStyle);
    }

    updateTrayIcon();
}

void MainWindow::setAllowScreenCapture(bool state)
{
    m_allowScreenCapture = state;
    for (auto window : qApp->topLevelWindows()) {
        if (window->isVisible()) {
            osUtils->setPreventScreenCapture(window, !m_allowScreenCapture);
        }
    }
    m_ui->actionAllowScreenCapture->blockSignals(true);
    m_ui->actionAllowScreenCapture->setChecked(m_allowScreenCapture);
    m_ui->actionAllowScreenCapture->blockSignals(false);
}

void MainWindow::focusWindowChanged(QWindow* window)
{
    if (window != windowHandle()) {
        m_lastFocusOutTime = Clock::currentMilliSecondsSinceEpoch();
    }

    if (!osUtils->setPreventScreenCapture(window, !m_allowScreenCapture) && !m_allowScreenCapture) {
        displayGlobalMessage(QObject::tr("Warning: Failed to block screenshot capture on a top-level window."),
                             MessageWidget::Error);
    }
}

void MainWindow::trayIconTriggered(QSystemTrayIcon::ActivationReason reason)
{
    if (!m_trayIconTriggerTimer.isActive()) {
        m_trayIconTriggerTimer.start(150);
    }
    // Overcome Qt bug https://bugreports.qt.io/browse/QTBUG-69698
    // Store last issued tray icon activation reason to properly
    // capture doubleclick events
    m_trayIconTriggerReason = reason;
}

void MainWindow::processTrayIconTrigger()
{
#ifdef Q_OS_MACOS
    // Do not toggle the window on macOS and just show the context menu instead.
    // Right click detection doesn't seem to be working anyway
    // and anything else will only trigger the context menu AND
    // toggle the window at the same time, which is confusing at best.
    // Showing only a context menu for tray icons seems to be best
    // practice on macOS anyway, so this is probably fine.
    return;
#endif

    if (m_trayIconTriggerReason == QSystemTrayIcon::DoubleClick) {
        // Always toggle window on double click
        toggleWindow();
    } else if (m_trayIconTriggerReason == QSystemTrayIcon::Trigger
               || m_trayIconTriggerReason == QSystemTrayIcon::MiddleClick) {
        // Toggle window if is not in front.
#ifdef Q_OS_WIN
        // If on Windows, check if focus switched within the 500 milliseconds since
        // clicking the tray icon removes focus from main window.
        if (isHidden() || (Clock::currentMilliSecondsSinceEpoch() - m_lastFocusOutTime) <= 500) {
#else
        // If on Linux, check if the window has focus.
        if (hasFocus() || isHidden() || windowHandle()->isActive()) {
#endif
            toggleWindow();
        } else {
            bringToFront();
        }
    }
}

void MainWindow::show()
{
#ifndef Q_OS_WIN
    m_lastShowTime = Clock::currentMilliSecondsSinceEpoch();
#endif
#ifdef Q_OS_MACOS
    // Unset minimize state to avoid weird fly-in effects
    setWindowState(windowState() & ~Qt::WindowMinimized);
    macUtils()->toggleForegroundApp(true);
#endif
    QMainWindow::show();
}

void MainWindow::hide()
{
#ifndef Q_OS_WIN
    qint64 current_time = Clock::currentMilliSecondsSinceEpoch();
    if (current_time - m_lastShowTime < 250) {
        return;
    }
#endif
    QMainWindow::hide();
#ifdef Q_OS_MACOS
    macUtils()->toggleForegroundApp(false);
#endif
}

void MainWindow::hideWindow()
{
    saveWindowInformation();

    // Only hide if tray icon is active, otherwise window will be gone forever
    if (isTrayIconEnabled()) {
        // On X11, the window should NOT be minimized and hidden at the same time. See issue #1595.
        // On macOS, we are skipping minimization as well to avoid playing the magic lamp animation.
        if (QGuiApplication::platformName() != "xcb" && QGuiApplication::platformName() != "cocoa") {
            setWindowState(windowState() | Qt::WindowMinimized);
        }
        hide();
    } else {
        showMinimized();
    }

    if (config()->get(Config::Security_LockDatabaseMinimize).toBool()) {
        m_ui->tabWidget->lockDatabasesDelayed();
    }
}

void MainWindow::minimizeOrHide()
{
    if (config()->get(Config::GUI_MinimizeToTray).toBool()) {
        hideWindow();
    } else {
        showMinimized();
    }
}

void MainWindow::toggleWindow()
{
    if (isVisible() && !isMinimized()) {
        hideWindow();
    } else {
        bringToFront();
    }
}

void MainWindow::closeModalWindow()
{
    if (qApp->modalWindow()) {
        qApp->modalWindow()->close();
    }
}

void MainWindow::lockDatabasesAfterInactivity()
{
    if (!m_ui->tabWidget->lockDatabases()) {
        m_inactivityTimer->activate();
    }
}

bool MainWindow::isTrayIconEnabled() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

void MainWindow::displayGlobalMessage(const QString& text,
                                      MessageWidget::MessageType type,
                                      bool showClosebutton,
                                      int autoHideTimeout)
{
    m_ui->globalMessageWidget->setCloseButtonVisible(showClosebutton);
    m_ui->globalMessageWidget->showMessage(text, type, autoHideTimeout);
}

void MainWindow::displayTabMessage(const QString& text,
                                   MessageWidget::MessageType type,
                                   bool showClosebutton,
                                   int autoHideTimeout)
{
    m_ui->tabWidget->currentDatabaseWidget()->showMessage(text, type, showClosebutton, autoHideTimeout);
}

void MainWindow::hideGlobalMessage()
{
    m_ui->globalMessageWidget->hideMessage();
}

void MainWindow::showYubiKeyPopup()
{
    displayGlobalMessage(tr("Please present or touch your YubiKey to continue…"),
                         MessageWidget::Information,
                         false,
                         MessageWidget::DisableAutoHide);
    setEnabled(false);
}

void MainWindow::hideYubiKeyPopup()
{
    hideGlobalMessage();
    setEnabled(true);
}

void MainWindow::bringToFront()
{
    ensurePolished();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    show();
    raise();
    activateWindow();
}

void MainWindow::handleScreenLock()
{
    if (config()->get(Config::Security_LockDatabaseScreenLock).toBool()) {
        lockDatabasesAfterInactivity();
    }
}

QStringList MainWindow::kdbxFilesFromUrls(const QList<QUrl>& urls)
{
    QStringList kdbxFiles;
    for (const QUrl& url : urls) {
        const QFileInfo fInfo(url.toLocalFile());
        const bool isKdbxFile = fInfo.isFile() && fInfo.suffix().toLower() == "kdbx";
        if (isKdbxFile) {
            kdbxFiles.append(fInfo.absoluteFilePath());
        }
    }

    return kdbxFiles;
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        const QStringList kdbxFiles = kdbxFilesFromUrls(mimeData->urls());
        if (!kdbxFiles.isEmpty()) {
            event->acceptProposedAction();
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        const QStringList kdbxFiles = kdbxFilesFromUrls(mimeData->urls());
        if (!kdbxFiles.isEmpty()) {
            event->acceptProposedAction();
        }
        for (const QString& kdbxFile : kdbxFiles) {
            openDatabase(kdbxFile);
        }
    }
}

void MainWindow::closeAllDatabases()
{
    m_ui->tabWidget->closeAllDatabaseTabs();
}

void MainWindow::lockAllDatabases()
{
    lockDatabasesAfterInactivity();
}

void MainWindow::displayDesktopNotification(const QString& msg, QString title, int msTimeoutHint)
{
    if (!m_trayIcon || !QSystemTrayIcon::supportsMessages()) {
        return;
    }

    if (title.isEmpty()) {
        title = BaseWindowTitle;
    }

    m_trayIcon->showMessage(title, msg, icons()->applicationIcon(), msTimeoutHint);
}

void MainWindow::restartApp(const QString& message)
{
    auto ans = MessageBox::question(
        this, tr("Restart Application?"), message, MessageBox::Yes | MessageBox::No, MessageBox::Yes);
    if (ans == MessageBox::Yes) {
        m_appExitCalled = true;
        m_restartRequested = true;
        close();
    } else {
        m_restartRequested = false;
    }
}

void MainWindow::initViewMenu()
{
    m_ui->actionThemeAuto->setData("auto");
    m_ui->actionThemeLight->setData("light");
    m_ui->actionThemeDark->setData("dark");
    m_ui->actionThemeClassic->setData("classic");

    auto themeActions = new QActionGroup(this);
    themeActions->addAction(m_ui->actionThemeAuto);
    themeActions->addAction(m_ui->actionThemeLight);
    themeActions->addAction(m_ui->actionThemeDark);
    themeActions->addAction(m_ui->actionThemeClassic);

    auto theme = config()->get(Config::GUI_ApplicationTheme).toString();
    for (auto action : themeActions->actions()) {
        if (action->data() == theme) {
            action->setChecked(true);
            break;
        }
    }

    connect(themeActions, &QActionGroup::triggered, this, [this, theme](QAction* action) {
        config()->set(Config::GUI_ApplicationTheme, action->data());
        if ((action->data() == "classic" || theme == "classic") && action->data() != theme) {
            restartApp(tr("You must restart the application to apply this setting. Would you like to restart now?"));
        } else {
            kpxcApp->applyTheme();
        }
    });

    bool compact = config()->get(Config::GUI_CompactMode).toBool();
    m_ui->actionCompactMode->setChecked(compact);
    connect(m_ui->actionCompactMode, &QAction::toggled, this, [this, compact](bool checked) {
        config()->set(Config::GUI_CompactMode, checked);
        if (checked != compact) {
            restartApp(tr("You must restart the application to apply this setting. Would you like to restart now?"));
        }
    });

#ifdef Q_OS_MACOS
    m_ui->actionShowMenubar->setVisible(false);
#else
    m_ui->actionShowMenubar->setChecked(!config()->get(Config::GUI_HideMenubar).toBool());
    connect(m_ui->actionShowMenubar, &QAction::toggled, this, [this](bool checked) {
        config()->set(Config::GUI_HideMenubar, !checked);
        applySettingsChanges();
    });
#endif

    m_ui->actionShowToolbar->setChecked(!config()->get(Config::GUI_HideToolbar).toBool());
    connect(m_ui->actionShowToolbar, &QAction::toggled, this, [this](bool checked) {
        config()->set(Config::GUI_HideToolbar, !checked);
        applySettingsChanges();
    });

    m_ui->actionShowGroupPanel->setChecked(!config()->get(Config::GUI_HideGroupPanel).toBool());
    connect(m_ui->actionShowGroupPanel, &QAction::toggled, this, [](bool checked) {
        config()->set(Config::GUI_HideGroupPanel, !checked);
    });

    m_ui->actionShowPreviewPanel->setChecked(!config()->get(Config::GUI_HidePreviewPanel).toBool());
    connect(m_ui->actionShowPreviewPanel, &QAction::toggled, this, [](bool checked) {
        config()->set(Config::GUI_HidePreviewPanel, !checked);
    });

    connect(m_ui->actionAlwaysOnTop, &QAction::toggled, this, [this](bool checked) {
        config()->set(Config::GUI_AlwaysOnTop, checked);
        if (checked) {
            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        } else {
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        }
        show();
    });
    // Set checked after connecting to act on a toggle in state (default state is unchecked)
    m_ui->actionAlwaysOnTop->setChecked(config()->get(Config::GUI_AlwaysOnTop).toBool());

    m_ui->actionHideUsernames->setChecked(config()->get(Config::GUI_HideUsernames).toBool());
    connect(m_ui->actionHideUsernames, &QAction::toggled, this, [](bool checked) {
        config()->set(Config::GUI_HideUsernames, checked);
    });

    m_ui->actionHidePasswords->setChecked(config()->get(Config::GUI_HidePasswords).toBool());
    connect(m_ui->actionHidePasswords, &QAction::toggled, this, [](bool checked) {
        config()->set(Config::GUI_HidePasswords, checked);
    });
}

void MainWindow::initActionCollection()
{
    auto ac = ActionCollection::instance();
    ac->addActions({// Database Menu
                    m_ui->actionDatabaseNew,
                    m_ui->actionDatabaseOpen,
                    m_ui->actionDatabaseSave,
                    m_ui->actionDatabaseSaveAs,
                    m_ui->actionDatabaseSaveBackup,
                    m_ui->actionDatabaseClose,
                    m_ui->actionLockDatabase,
                    m_ui->actionLockAllDatabases,
                    m_ui->actionDatabaseSettings,
                    m_ui->actionDatabaseSecurity,
                    m_ui->actionReports,
                    m_ui->actionPasskeys,
                    m_ui->actionDatabaseMerge,
                    m_ui->actionImportPasskey,
                    m_ui->actionImportCsv,
                    m_ui->actionImportOpVault,
                    m_ui->actionImportKeePass1,
                    m_ui->actionExportCsv,
                    m_ui->actionExportHtml,
                    m_ui->actionExportXML,
                    m_ui->actionQuit,
                    // Entry Menu
                    m_ui->actionEntryNew,
                    m_ui->actionEntryEdit,
                    m_ui->actionEntryClone,
                    m_ui->actionEntryDelete,
                    m_ui->actionEntryCopyUsername,
                    m_ui->actionEntryCopyPassword,
                    m_ui->actionEntryCopyURL,
                    m_ui->actionEntryCopyTitle,
                    m_ui->actionEntryCopyNotes,
                    m_ui->actionEntryTotp,
                    m_ui->actionEntryTotpQRCode,
                    m_ui->actionEntrySetupTotp,
                    m_ui->actionEntryCopyTotp,
                    m_ui->actionEntryCopyPasswordTotp,
                    m_ui->actionEntryAutoTypeSequence,
                    m_ui->actionEntryAutoTypeUsername,
                    m_ui->actionEntryAutoTypeUsernameEnter,
                    m_ui->actionEntryAutoTypePassword,
                    m_ui->actionEntryAutoTypePasswordEnter,
                    m_ui->actionEntryAutoTypeTOTP,
                    m_ui->actionEntryDownloadIcon,
                    m_ui->actionEntryOpenUrl,
                    m_ui->actionEntryMoveUp,
                    m_ui->actionEntryMoveDown,
                    m_ui->actionEntryAddToAgent,
                    m_ui->actionEntryRemoveFromAgent,
                    m_ui->actionEntryRestore,
                    // Group Menu
                    m_ui->actionGroupNew,
                    m_ui->actionGroupEdit,
                    m_ui->actionGroupClone,
                    m_ui->actionGroupDelete,
                    m_ui->actionGroupDownloadFavicons,
                    m_ui->actionGroupSortAsc,
                    m_ui->actionGroupSortDesc,
                    m_ui->actionGroupEmptyRecycleBin,
                    // Tools Menu
                    m_ui->actionPasswordGenerator,
                    m_ui->actionSettings,
                    // View Menu
                    m_ui->actionThemeAuto,
                    m_ui->actionThemeLight,
                    m_ui->actionThemeDark,
                    m_ui->actionThemeClassic,
                    m_ui->actionCompactMode,
#ifndef Q_OS_MACOS
                    m_ui->actionShowMenubar,
#endif
                    m_ui->actionShowToolbar,
                    m_ui->actionShowGroupPanel,
                    m_ui->actionShowPreviewPanel,
                    m_ui->actionAllowScreenCapture,
                    m_ui->actionAlwaysOnTop,
                    m_ui->actionHideUsernames,
                    m_ui->actionHidePasswords,
                    // Help Menu
                    m_ui->actionGettingStarted,
                    m_ui->actionUserGuide,
                    m_ui->actionKeyboardShortcuts,
                    m_ui->actionOnlineHelp,
                    m_ui->actionCheckForUpdates,
                    m_ui->actionDonate,
                    m_ui->actionBugReport,
                    m_ui->actionAbout});

    // Register as default any shortcuts that were set in the .ui file
    for (const auto action : ac->actions()) {
        if (!action->shortcut().isEmpty()) {
            ac->setDefaultShortcut(action, action->shortcut());
        }
    }

    // Actions with standard shortcuts (if no standard shortcut exists, leave the existing
    // shortcuts from the .ui file in place)
    ac->setDefaultShortcut(m_ui->actionDatabaseOpen, QKeySequence::Open);
    ac->setDefaultShortcut(m_ui->actionDatabaseSave, QKeySequence::Save);
    ac->setDefaultShortcut(m_ui->actionDatabaseSaveAs, QKeySequence::SaveAs);
    ac->setDefaultShortcut(m_ui->actionDatabaseClose, QKeySequence::Close);
    ac->setDefaultShortcut(m_ui->actionSettings, QKeySequence::Preferences);
    ac->setDefaultShortcut(m_ui->actionQuit, QKeySequence::Quit);
    ac->setDefaultShortcut(m_ui->actionEntryNew, QKeySequence::New);

    // Prevent conflicts with global Mac shortcuts (force Control on all platforms)
    // Note: Qt::META means Ctrl on Mac.
#ifdef Q_OS_MAC
    ac->setDefaultShortcut(m_ui->actionEntryAddToAgent, Qt::META + Qt::Key_H);
    ac->setDefaultShortcut(m_ui->actionEntryRemoveFromAgent, Qt::META + Qt::SHIFT + Qt::Key_H);
#endif

    QTimer::singleShot(1, ac, &ActionCollection::restoreShortcuts);
}

MainWindowEventFilter::MainWindowEventFilter(QObject* parent)
    : QObject(parent)
{
    m_altCoolDown.setInterval(250);
    m_altCoolDown.setSingleShot(true);

    m_menubarTimer.setInterval(250);
    m_menubarTimer.setSingleShot(false);
    connect(&m_menubarTimer, &QTimer::timeout, this, [this] {
        auto mainwindow = getMainWindow();
        if (mainwindow && mainwindow->m_ui->menubar->isVisible() && config()->get(Config::GUI_HideMenubar).toBool()) {
            // If the menu bar is visible with no active menu, hide it
            if (!mainwindow->m_ui->menubar->activeAction()) {
                mainwindow->m_ui->menubar->setVisible(false);
                m_altCoolDown.start();
                m_menubarTimer.stop();
            }
            // Conditions to hide the menubar or stop the timer have not been met
            return;
        }
        // We no longer need the timer
        m_menubarTimer.stop();
    });
}

/**
 * MainWindow event filter to initiate empty-area drag on the toolbar, menubar, and tabbar.
 * Also shows menubar with Alt when menubar itself is hidden.
 */
bool MainWindowEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    auto* mainWindow = getMainWindow();
    if (!mainWindow || !mainWindow->m_ui) {
        return QObject::eventFilter(watched, event);
    }

    auto eventType = event->type();
    if (eventType == QEvent::MouseButtonPress) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        // startSystemMove was introduced in Qt 5.15
        auto mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if (watched == mainWindow->m_ui->menubar) {
            if (!mainWindow->m_ui->menubar->actionAt(mouseEvent->pos())) {
                mainWindow->windowHandle()->startSystemMove();
                return false;
            }
        } else if (watched == mainWindow->m_ui->toolBar) {
            if (!mainWindow->m_ui->toolBar->isMovable() || mainWindow->m_ui->toolBar->cursor() != Qt::SizeAllCursor) {
                mainWindow->windowHandle()->startSystemMove();
                return false;
            }
        } else if (watched == mainWindow->m_ui->tabWidget->tabBar()) {
            if (mainWindow->m_ui->tabWidget->tabBar()->tabAt(mouseEvent->pos()) == -1) {
                mainWindow->windowHandle()->startSystemMove();
                return true;
            }
        }
#endif
    } else if (eventType == QEvent::KeyRelease && watched == mainWindow) {
        auto keyEvent = dynamic_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Alt && !keyEvent->modifiers() && config()->get(Config::GUI_HideMenubar).toBool()
            && !m_altCoolDown.isActive()) {
            auto menubar = mainWindow->m_ui->menubar;
            menubar->setVisible(!menubar->isVisible());
            if (menubar->isVisible()) {
                menubar->setActiveAction(mainWindow->m_ui->menuFile->menuAction());
                m_menubarTimer.start();
            } else {
                m_menubarTimer.stop();
            }
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}
