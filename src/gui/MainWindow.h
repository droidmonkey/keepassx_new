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

#ifndef KEEPASSX_MAINWINDOW_H
#define KEEPASSX_MAINWINDOW_H

#include <QActionGroup>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QTimer>

#include "core/SignalMultiplexer.h"
#include "gui/DatabaseWidget.h"
#include "gui/osutils/ScreenLockListener.h"

namespace Ui
{
    class MainWindow;
}

class InactivityTimer;
class SearchWidget;
class MainWindowEventFilter;

class MainWindow : public QMainWindow
{
    Q_OBJECT

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS) && !defined(QT_NO_DBUS)
    Q_CLASSINFO("D-Bus Interface", "org.keepassxc.KeePassXC.MainWindow")
#endif

public:
    MainWindow();
    ~MainWindow() override;

    QList<DatabaseWidget*> getOpenDatabases();
    void restoreConfigState();
    void setAllowScreenCapture(bool state);

    enum StackedWidgetIndex
    {
        DatabaseTabScreen = 0,
        SettingsScreen = 1,
        WelcomeScreen = 2,
        PasswordGeneratorScreen = 3
    };

signals:
    void databaseUnlocked(DatabaseWidget* dbWidget);
    void databaseLocked(DatabaseWidget* dbWidget);
    void activeDatabaseChanged(DatabaseWidget* dbWidget);
    void databaseUnlockDialogFinished(bool accepted, DatabaseWidget* dbWidget);

public slots:
    void openDatabase(const QString& filePath, const QString& password = {}, const QString& keyfile = {});
    void appExit();
    bool isHardwareKeySupported();
    bool refreshHardwareKeys();
    void displayGlobalMessage(const QString& text,
                              MessageWidget::MessageType type,
                              bool showClosebutton = true,
                              int autoHideTimeout = MessageWidget::DefaultAutoHideTimeout);
    void displayTabMessage(const QString& text,
                           MessageWidget::MessageType type,
                           bool showClosebutton = true,
                           int autoHideTimeout = MessageWidget::DefaultAutoHideTimeout);
    void hideGlobalMessage();
    void showYubiKeyPopup();
    void hideYubiKeyPopup();
    void hide();
    void show();
    void hideWindow();
    void minimizeOrHide();
    void toggleWindow();
    void bringToFront();
    void closeAllDatabases();
    void lockAllDatabases();
    void closeModalWindow();
    void displayDesktopNotification(const QString& msg, QString title = "", int msTimeoutHint = 10000);
    void restartApp(const QString& message);

protected:
    bool event(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private slots:
    void updateMenuActionState();
    void updateToolbarSeparatorVisibility();
    void updateWindowTitle();
    void showAboutDialog();
    void performUpdateCheck();
    void showUpdateCheckDialog();
    void focusWindowChanged(QWindow* focusWindow);
    void hasUpdateAvailable(bool hasUpdate, const QString& version, bool isManuallyRequested);
    void openDonateUrl();
    void openBugReportUrl();
    void openGettingStartedGuide();
    void openUserGuide();
    void openOnlineHelp();
    void openKeyboardShortcuts();
    void switchToDatabases();
    void switchToSettings(bool enabled);
    void togglePasswordGenerator(bool enabled);
    void switchToNewDatabase();
    void switchToOpenDatabase();
    void switchToDatabaseFile(const QString& file);
    void updateRemoteSyncMenuEntries();
    void databaseStatusChanged(DatabaseWidget* dbWidget);
    void databaseTabChanged(int tabIndex);
    void openRecentDatabase(QAction* action);
    void clearLastDatabases();
    void updateLastDatabasesMenu();
    void updateCopyAttributesMenu();
    void updateSetTagsMenu();
    void showEntryContextMenu(const QPoint& globalPos);
    void showGroupContextMenu(const QPoint& globalPos);
    void applySettingsChanges();
    void trayIconTriggered(QSystemTrayIcon::ActivationReason reason);
    void processTrayIconTrigger();
    void lockDatabasesAfterInactivity();
    void handleScreenLock();
    void showErrorMessage(const QString& message);
    void selectNextDatabaseTab();
    void selectPreviousDatabaseTab();
    void selectDatabaseTab(int tabIndex, bool wrap = false);
    void obtainContextFocusLock();
    void releaseContextFocusLock();
    void agentEnabled(bool enabled);
    void updateTrayIcon();
    void updateProgressBar(int percentage, QString message);
    void updateEntryCountLabel();
    void focusSearchWidget();
    void enableMenuAndToolbar();
    void disableMenuAndToolbar();

private:
    static const QString BaseWindowTitle;

    void saveWindowInformation();
    bool saveLastDatabases();
    bool isTrayIconEnabled() const;
    void customOpenUrl(QString url);

    static QStringList kdbxFilesFromUrls(const QList<QUrl>& urls);
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    void initViewMenu();
    void initActionCollection();

    const QScopedPointer<Ui::MainWindow> m_ui;
    SignalMultiplexer m_actionMultiplexer;
    QPointer<QAction> m_clearHistoryAction;
    QPointer<QAction> m_searchWidgetAction;
    QPointer<QMenu> m_entryContextMenu;
    QPointer<QMenu> m_entryNewContextMenu;
    QPointer<QActionGroup> m_lastDatabasesActions;
    QPointer<QActionGroup> m_copyAdditionalAttributeActions;
    QPointer<QActionGroup> m_setTagsMenuActions;
    QPointer<InactivityTimer> m_inactivityTimer;
    QPointer<InactivityTimer> m_touchIDinactivityTimer;
    int m_countDefaultAttributes;
    QPointer<QSystemTrayIcon> m_trayIcon;
    QPointer<ScreenLockListener> m_screenLockListener;
    QPointer<SearchWidget> m_searchWidget;
    QPointer<QProgressBar> m_progressBar;
    QPointer<QLabel> m_progressBarLabel;
    QPointer<QLabel> m_statusBarLabel;

    Q_DISABLE_COPY(MainWindow)

    bool m_appExitCalled = false;
    bool m_appExiting = false;
    bool m_restartRequested = false;
    bool m_contextMenuFocusLock = false;
    bool m_showToolbarSeparator = false;
    bool m_allowScreenCapture = false;
    qint64 m_lastFocusOutTime = 0;
    qint64 m_lastShowTime = 0;
    QTimer m_updateCheckTimer;
    QTimer m_trayIconTriggerTimer;
    QSystemTrayIcon::ActivationReason m_trayIconTriggerReason;

    friend class MainWindowEventFilter;
};

class MainWindowEventFilter : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowEventFilter(QObject* parent);
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTimer m_menubarTimer;
    QTimer m_altCoolDown;
};

/**
 * Return instance of MainWindow created on app load
 * non-gui instances will return nullptr
 *
 * @return MainWindow instance or nullptr
 */
MainWindow* getMainWindow();

#endif // KEEPASSX_MAINWINDOW_H
