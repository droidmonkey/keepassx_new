/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "DatabaseTabWidget.h"

#include <QFileInfo>
#include <QTabBar>

#include "autotype/AutoType.h"
#include "core/Merger.h"
#include "core/Tools.h"
#include "format/CsvExporter.h"
#include "gui/Clipboard.h"
#include "gui/DatabaseOpenDialog.h"
#include "gui/DatabaseWidget.h"
#include "gui/DatabaseWidgetStateSync.h"
#include "gui/FileDialog.h"
#include "gui/MessageBox.h"
#include "gui/export/ExportDialog.h"
#ifdef Q_OS_MACOS
#include "gui/osutils/macutils/MacUtils.h"
#endif
#include "gui/wizard/NewDatabaseWizard.h"

DatabaseTabWidget::DatabaseTabWidget(QWidget* parent)
    : QTabWidget(parent)
    , m_dbWidgetStateSync(new DatabaseWidgetStateSync(this))
    , m_dbWidgetPendingLock(nullptr)
    , m_databaseOpenDialog(new DatabaseOpenDialog(this))
    , m_importWizard(nullptr)
    , m_databaseOpenInProgress(false)
{
    auto* tabBar = new QTabBar(this);
    tabBar->setAcceptDrops(true);
    tabBar->setChangeCurrentOnDrag(true);
    setTabBar(tabBar);
    setDocumentMode(true);

    // clang-format off
    connect(this, SIGNAL(tabCloseRequested(int)), SLOT(closeDatabaseTab(int)));
    connect(this, SIGNAL(currentChanged(int)), SLOT(emitActiveDatabaseChanged()));
    connect(this, SIGNAL(activeDatabaseChanged(DatabaseWidget*)),
            m_dbWidgetStateSync, SLOT(setActive(DatabaseWidget*)));
    connect(autoType(), SIGNAL(globalAutoTypeTriggered(const QString&)), SLOT(performGlobalAutoType(const QString&)));
    connect(autoType(), SIGNAL(autotypeRetypeTimeout()), SLOT(relockPendingDatabase()));
    connect(autoType(), SIGNAL(autotypeFinished()), SLOT(relockPendingDatabase()));
    connect(m_databaseOpenDialog.data(), &DatabaseOpenDialog::dialogFinished,
            this, &DatabaseTabWidget::handleDatabaseUnlockDialogFinished);
    // clang-format on

#ifdef Q_OS_MACOS
    connect(macUtils(), SIGNAL(userSwitched()), SLOT(lockDatabasesOnUserSwitch()));
#endif

    m_lockDelayTimer.setSingleShot(true);
    connect(&m_lockDelayTimer, &QTimer::timeout, this, [this] { lockDatabases(); });
}

DatabaseTabWidget::~DatabaseTabWidget() = default;

void DatabaseTabWidget::toggleTabbar()
{
    if (count() > 1) {
        tabBar()->show();
        setFocusPolicy(Qt::StrongFocus);
        emit tabVisibilityChanged(true);
    } else {
        tabBar()->hide();
        setFocusPolicy(Qt::NoFocus);
        emit tabVisibilityChanged(false);
    }
}

/**
 * Helper method for invoking the new database wizard.
 * The user of this method MUST take ownership of the returned pointer.
 *
 * @return pointer to the configured new database, nullptr on failure
 */
QSharedPointer<Database> DatabaseTabWidget::execNewDatabaseWizard()
{
    // use QScopedPointer to ensure deletion after scope ends, but still parent
    // it to this to make it modal and allow easier access in unit tests
    QScopedPointer<NewDatabaseWizard> wizard(new NewDatabaseWizard(this));
    if (!wizard->exec()) {
        return {};
    }

    auto db = wizard->takeDatabase();
    if (!db) {
        return {};
    }
    Q_ASSERT(db->key());
    Q_ASSERT(db->kdf());
    if (!db->key() || !db->kdf()) {
        MessageBox::critical(this,
                             tr("Database creation error"),
                             tr("The created database has no key or KDF, refusing to save it.\n"
                                "This is definitely a bug, please report it to the developers."),
                             MessageBox::Ok,
                             MessageBox::Ok);
        return {};
    }

    return db;
}

DatabaseWidget* DatabaseTabWidget::newDatabase()
{
    auto db = execNewDatabaseWizard();
    if (!db) {
        return nullptr;
    }

    auto dbWidget = new DatabaseWidget(db, this);
    addDatabaseTab(dbWidget);
    db->markAsModified();
    return dbWidget;
}

void DatabaseTabWidget::openDatabase()
{
    auto filter = QString("%1 (*.kdbx);;%2 (*)").arg(tr("KeePass 2 Database"), tr("All files"));
    auto fileName = fileDialog()->getOpenFileName(this, tr("Open database"), FileDialog::getLastDir("db"), filter);
    if (!fileName.isEmpty()) {
        FileDialog::saveLastDir("db", fileName, true);
        addDatabaseTab(fileName);
    }
}

/**
 * Add a new database tab or switch to an existing one if the
 * database has been opened already.
 *
 * @param filePath database file path
 * @param inBackground optional, don't focus tab after opening
 * @param password optional, password to unlock database
 * @param keyfile optional, path to keyfile to unlock database
 *
 */
void DatabaseTabWidget::addDatabaseTab(const QString& filePath,
                                       bool inBackground,
                                       const QString& password,
                                       const QString& keyfile)
{
    QString cleanFilePath = QDir::toNativeSeparators(filePath);
    QFileInfo fileInfo(cleanFilePath);
    QString canonicalFilePath = fileInfo.canonicalFilePath();

    if (canonicalFilePath.isEmpty()) {
        emit messageGlobal(tr("Failed to open %1. It either does not exist or is not accessible.").arg(cleanFilePath),
                           MessageWidget::Error);
        return;
    }

    for (int i = 0, c = count(); i < c; ++i) {
        auto* dbWidget = databaseWidgetFromIndex(i);
        Q_ASSERT(dbWidget);
        if (dbWidget
            && dbWidget->database()->canonicalFilePath().compare(canonicalFilePath, FILE_CASE_SENSITIVE) == 0) {
            dbWidget->performUnlockDatabase(password, keyfile);
            if (!inBackground) {
                // switch to existing tab if file is already open
                setCurrentIndex(indexOf(dbWidget));
            }
            return;
        }
    }

    auto* dbWidget = new DatabaseWidget(QSharedPointer<Database>::create(cleanFilePath), this);
    addDatabaseTab(dbWidget, inBackground);
    dbWidget->performUnlockDatabase(password, keyfile);
    updateLastDatabases(dbWidget->database());
}

/**
 * Tries to lock the database at the given index and if
 * it succeeds proceed to switch to the first unlocked database tab
 */
void DatabaseTabWidget::lockAndSwitchToFirstUnlockedDatabase(int index)
{
    if (index == -1) {
        index = currentIndex();
    }
    auto dbWidget = databaseWidgetFromIndex(index);
    if (!dbWidget) {
        return;
    }

    if (dbWidget->isLocked()) {
        // Database is already locked, act like lock all databases instead
        lockDatabases();
    } else if (dbWidget->lock()) {
        for (int i = 0, c = count(); i < c; ++i) {
            if (!databaseWidgetFromIndex(i)->isLocked()) {
                setCurrentIndex(i);
                emitActiveDatabaseChanged();
                return;
            }
        }
    }
}

/**
 * Add a new database tab containing the given DatabaseWidget
 * @param filePath
 * @param inBackground optional, don't focus tab after opening
 */
void DatabaseTabWidget::addDatabaseTab(DatabaseWidget* dbWidget, bool inBackground)
{
    Q_ASSERT(dbWidget->database());

    // emit before index change
    emit databaseOpened(dbWidget);

    int index = addTab(dbWidget, "");
    updateTabName(index);
    toggleTabbar();
    if (!inBackground) {
        setCurrentIndex(index);
    }

    connect(dbWidget,
            SIGNAL(requestOpenDatabase(QString, bool, QString, QString)),
            SLOT(addDatabaseTab(QString, bool, QString, QString)));
    connect(dbWidget, SIGNAL(databaseFilePathChanged(QString, QString)), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(closeRequest()), SLOT(closeDatabaseTabFromSender()));
    connect(dbWidget,
            SIGNAL(databaseReplaced(const QSharedPointer<Database>&, const QSharedPointer<Database>&)),
            SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseModified()), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseSaved()), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseSaved()), SLOT(updateLastDatabases()));
    connect(dbWidget, SIGNAL(databaseUnlocked()), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseUnlocked()), SLOT(emitDatabaseLockChanged()));
    connect(dbWidget, SIGNAL(databaseLocked()), SLOT(updateTabName()));
    connect(dbWidget, SIGNAL(databaseLocked()), SLOT(emitDatabaseLockChanged()));
    connect(dbWidget,
            &DatabaseWidget::unlockDatabaseInDialogForSync,
            this,
            &DatabaseTabWidget::unlockDatabaseInDialogForSync);
}

void DatabaseTabWidget::importFile()
{
    // Show the import wizard
    m_importWizard = new ImportWizard(this);

    connect(m_importWizard.data(), &QWizard::finished, [&](int result) {
        if (result != QDialog::Accepted) {
            return;
        }

        auto db = m_importWizard->database();
        if (!db) {
            // Import wizard was cancelled
            return;
        }

        switch (m_importWizard->importIntoType()) {
        case ImportWizard::EXISTING_DATABASE:
            for (int i = 0, c = count(); i < c; ++i) {
                auto importInto = m_importWizard->importInto();
                // Find the database and group to import into based on import wizard choice
                auto dbWidget = databaseWidgetFromIndex(i);
                if (!dbWidget->isLocked() && dbWidget->database()->uuid() == importInto.first) {
                    auto group = dbWidget->database()->rootGroup()->findGroupByUuid(importInto.second);
                    if (group) {
                        // Extract the root group from the import database
                        auto importGroup = db->setRootGroup(new Group());
                        importGroup->setParent(group);
                        setCurrentIndex(i);
                        return;
                    }
                }
            }
            break;
        case ImportWizard::TEMPORARY_DATABASE: {
            // Use the already created database as temporary database
            auto dbWidget = new DatabaseWidget(db, this);
            addDatabaseTab(dbWidget);
            return;
        }
        default:
            // Start the new database wizard with the imported database
            auto newDb = execNewDatabaseWizard();
            if (newDb) {
                // Merge the imported db into the new one
                Merger merger(db.data(), newDb.data());
                merger.setSkipDatabaseCustomData(true);
                merger.merge();
                // Show the new database
                auto dbWidget = new DatabaseWidget(newDb, this);
                addDatabaseTab(dbWidget);
                newDb->markAsModified();
                return;
            }
        }
    });

    // use `open` instead of `exec`. `exec` should not be used, see https://doc.qt.io/qt-6/qdialog.html#exec
    m_importWizard->show();
}

void DatabaseTabWidget::mergeDatabase()
{
    auto dbWidget = currentDatabaseWidget();
    if (dbWidget && !dbWidget->isLocked()) {
        auto filter = QString("%1 (*.kdbx);;%2 (*)").arg(tr("KeePass 2 Database"), tr("All files"));
        auto fileName =
            fileDialog()->getOpenFileName(this, tr("Merge database"), FileDialog::getLastDir("merge"), filter);
        if (!fileName.isEmpty()) {
            FileDialog::saveLastDir("merge", fileName, true);
            mergeDatabase(fileName);
        }
    }
}

void DatabaseTabWidget::mergeDatabase(const QString& filePath)
{
    unlockDatabaseInDialog(currentDatabaseWidget(), DatabaseOpenDialog::Intent::Merge, filePath);
}

/**
 * Attempt to close the current database and remove its tab afterwards.
 *
 * @param index index of the database tab to close
 * @return true if database was closed successfully
 */
bool DatabaseTabWidget::closeCurrentDatabaseTab()
{
    return closeDatabaseTab(currentIndex());
}

/**
 * Attempt to close the database tab that sent the close request.
 *
 * @param index index of the database tab to close
 * @return true if database was closed successfully
 */
bool DatabaseTabWidget::closeDatabaseTabFromSender()
{
    return closeDatabaseTab(qobject_cast<DatabaseWidget*>(sender()));
}

/**
 * Attempt to close a database and remove its tab afterwards.
 *
 * @param index index of the database tab to close
 * @return true if database was closed successfully
 */
bool DatabaseTabWidget::closeDatabaseTab(int index)
{
    return closeDatabaseTab(qobject_cast<DatabaseWidget*>(widget(index)));
}

/**
 * Attempt to close a database and remove its tab afterwards.
 *
 * @param dbWidget \link DatabaseWidget to close
 * @return true if database was closed successfully
 */
bool DatabaseTabWidget::closeDatabaseTab(DatabaseWidget* dbWidget)
{
    int tabIndex = indexOf(dbWidget);
    if (!dbWidget || tabIndex < 0) {
        return false;
    }

    QString filePath = dbWidget->database()->filePath();
    if (!dbWidget->close()) {
        return false;
    }

    removeTab(tabIndex);
    dbWidget->deleteLater();
    toggleTabbar();
    emit databaseClosed(filePath);
    return true;
}

/**
 * Attempt to close all opened databases.
 * The attempt will be aborted with the first database that cannot be closed.
 *
 * @return true if all databases could be closed.
 */
bool DatabaseTabWidget::closeAllDatabaseTabs()
{
    // Attempt to lock all databases first to prevent closing only a portion of tabs
    if (lockDatabases()) {
        while (count() > 0) {
            if (!closeDatabaseTab(0)) {
                return false;
            }
        }
        return true;
    }

    return false;
}

bool DatabaseTabWidget::saveDatabase(int index)
{
    if (index == -1) {
        index = currentIndex();
    }

    return databaseWidgetFromIndex(index)->save();
}

bool DatabaseTabWidget::saveDatabaseAs(int index)
{
    if (index == -1) {
        index = currentIndex();
    }

    auto* dbWidget = databaseWidgetFromIndex(index);
    bool ok = dbWidget->saveAs();
    if (ok) {
        updateLastDatabases(dbWidget->database());
    }
    return ok;
}

bool DatabaseTabWidget::saveDatabaseBackup(int index)
{
    if (index == -1) {
        index = currentIndex();
    }

    auto* dbWidget = databaseWidgetFromIndex(index);
    bool ok = dbWidget->saveBackup();
    if (ok) {
        updateLastDatabases(dbWidget->database());
    }
    return ok;
}

void DatabaseTabWidget::closeDatabaseFromSender()
{
    auto* dbWidget = qobject_cast<DatabaseWidget*>(sender());
    Q_ASSERT(dbWidget);
    closeDatabaseTab(dbWidget);
}

void DatabaseTabWidget::exportToCsv()
{
    auto db = databaseWidgetFromIndex(currentIndex())->database();
    if (!db) {
        Q_ASSERT(false);
        return;
    }

    if (!warnOnExport()) {
        return;
    }

    auto fileName = fileDialog()->getSaveFileName(
        this, tr("Export database to CSV file"), FileDialog::getLastDir("csv"), tr("CSV file").append(" (*.csv)"));
    if (fileName.isEmpty()) {
        return;
    }

    FileDialog::saveLastDir("csv", fileName, true);

    CsvExporter csvExporter;
    if (!csvExporter.exportDatabase(fileName, db)) {
        emit messageGlobal(tr("Writing the CSV file failed.").append("\n").append(csvExporter.errorString()),
                           MessageWidget::Error);
    }
}

void DatabaseTabWidget::handleExportError(const QString& reason)
{
    emit messageGlobal(tr("Writing the HTML file failed.").append("\n").append(reason), MessageWidget::Error);
}

void DatabaseTabWidget::exportToHtml()
{
    auto db = databaseWidgetFromIndex(currentIndex())->database();
    if (!db) {
        Q_ASSERT(false);
        return;
    }

    auto exportDialog = new ExportDialog(db, this);
    connect(exportDialog, SIGNAL(exportFailed(QString)), SLOT(handleExportError(const QString&)));
    exportDialog->exec();
}

void DatabaseTabWidget::exportToXML()
{
    auto db = databaseWidgetFromIndex(currentIndex())->database();
    if (!db) {
        Q_ASSERT(false);
        return;
    }

    if (!warnOnExport()) {
        return;
    }

    auto fileName = fileDialog()->getSaveFileName(
        this, tr("Export database to XML file"), FileDialog::getLastDir("xml"), tr("XML file").append(" (*.xml)"));
    if (fileName.isEmpty()) {
        return;
    }

    FileDialog::saveLastDir("xml", fileName, true);

    QByteArray xmlData;
    QString err;
    if (!db->extract(xmlData, &err)) {
        emit messageGlobal(tr("Writing the XML file failed").append("\n").append(err), MessageWidget::Error);
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit messageGlobal(tr("Writing the XML file failed").append("\n").append(file.errorString()),
                           MessageWidget::Error);
    }
    file.write(xmlData);
}

bool DatabaseTabWidget::warnOnExport()
{
    auto ans =
        MessageBox::question(this,
                             tr("Export Confirmation"),
                             tr("You are about to export your database to an unencrypted file. This will leave your "
                                "passwords and sensitive information vulnerable! Are you sure you want to continue?"),
                             MessageBox::Yes | MessageBox::No,
                             MessageBox::No);
    return ans == MessageBox::Yes;
}

void DatabaseTabWidget::showDatabaseReports(bool state)
{
    if (state) {
        currentDatabaseWidget()->switchToDatabaseReports();
    } else {
        currentDatabaseWidget()->switchToMainView();
    }
}

void DatabaseTabWidget::showDatabaseSettings(bool state)
{
    if (state) {
        currentDatabaseWidget()->switchToDatabaseSettings();
    } else {
        currentDatabaseWidget()->switchToMainView();
    }
}

void DatabaseTabWidget::showDatabaseSecurity()
{
    currentDatabaseWidget()->switchToDatabaseSecurity();
}

#ifdef WITH_XC_BROWSER_PASSKEYS
void DatabaseTabWidget::showPasskeys()
{
    currentDatabaseWidget()->switchToPasskeys();
}

void DatabaseTabWidget::importPasskey()
{
    currentDatabaseWidget()->showImportPasskeyDialog();
}

void DatabaseTabWidget::importPasskeyToEntry()
{
    currentDatabaseWidget()->showImportPasskeyDialog(true);
}

void DatabaseTabWidget::removePasskeyFromEntry()
{
    currentDatabaseWidget()->removePasskeyFromEntry();
}
#endif

bool DatabaseTabWidget::isModified(int index) const
{
    if (count() == 0) {
        return false;
    }

    if (index == -1) {
        index = currentIndex();
    }

    auto db = databaseWidgetFromIndex(index)->database();
    return db && db->isModified();
}

bool DatabaseTabWidget::canSave(int index) const
{
    return isModified(index);
}

bool DatabaseTabWidget::hasLockableDatabases() const
{
    for (int i = 0, c = count(); i < c; ++i) {
        if (!databaseWidgetFromIndex(i)->isLocked()) {
            return true;
        }
    }
    return false;
}

/**
 * Get the tab's (original) display name without platform-specific
 * mangling that may occur when reading back the actual widget's \link tabText()
 *
 * @param index tab index
 * @return tab name
 */
QString DatabaseTabWidget::tabName(int index)
{
    auto dbWidget = databaseWidgetFromIndex(index);
    if (!dbWidget) {
        return {};
    }

    auto tabName = dbWidget->displayName();

    if (dbWidget->isLocked()) {
        tabName = tr("%1 [Locked]", "Database tab name modifier").arg(tabName);
    }

    if (dbWidget->database()->isTemporaryDatabase()) {
        tabName = tr("%1 [Temporary]", "Database tab name modifier").arg(tabName);
    }

    // needs to be last check, as MainWindow may remove the asterisk again
    if (dbWidget->database()->isModified()) {
        tabName.append("*");
    }

    return tabName;
}

/**
 * Update of the given tab index or of the sending
 * DatabaseWidget if `index` == -1.
 */
void DatabaseTabWidget::updateTabName(int index)
{
    auto* dbWidget = databaseWidgetFromIndex(index);
    if (!dbWidget) {
        dbWidget = qobject_cast<DatabaseWidget*>(sender());
    }
    Q_ASSERT(dbWidget);
    if (!dbWidget) {
        return;
    }
    index = indexOf(dbWidget);
    setTabText(index, tabName(index));
    setTabToolTip(index, dbWidget->displayFilePath());
    emit tabNameChanged();
}

DatabaseWidget* DatabaseTabWidget::databaseWidgetFromIndex(int index) const
{
    return qobject_cast<DatabaseWidget*>(widget(index));
}

DatabaseWidget* DatabaseTabWidget::currentDatabaseWidget()
{
    return qobject_cast<DatabaseWidget*>(currentWidget());
}

/**
 * Attempt to lock all open databases
 *
 * @return return true if all databases are locked
 */
bool DatabaseTabWidget::lockDatabases()
{
    int numLocked = 0;
    int c = count();
    for (int i = 0; i < c; ++i) {
        auto dbWidget = databaseWidgetFromIndex(i);
        if (dbWidget->lock()) {
            ++numLocked;
            if (dbWidget->database()->filePath().isEmpty()) {
                // If we locked a database without a file close the tab
                closeDatabaseTab(dbWidget);
            }
        }
    }

    return numLocked == c;
}

void DatabaseTabWidget::lockDatabasesDelayed()
{
    // Delay at least 1 second and up to 20 seconds depending on clipboard state.
    // This allows for Auto-Type, Browser Extension, and clipboard to function
    // even with "Lock on Minimize" setting enabled.
    int lockDelay = qBound(1, clipboard()->secondsToClear(), 20);
    m_lockDelayTimer.setInterval(lockDelay * 1000);
    if (!m_lockDelayTimer.isActive()) {
        m_lockDelayTimer.start();
    }
}

void DatabaseTabWidget::lockDatabasesOnUserSwitch()
{
    if (config()->get(Config::Security_LockDatabaseOnUserSwitch).toBool()) {
        lockDatabases();
    }
}

/**
 * Unlock a database with an unlock popup dialog.
 *
 * @param dbWidget DatabaseWidget which to connect signals to
 * @param intent intent for unlocking
 */
void DatabaseTabWidget::unlockDatabaseInDialog(DatabaseWidget* dbWidget, DatabaseOpenDialog::Intent intent)
{
    unlockDatabaseInDialog(dbWidget, intent, dbWidget->database()->filePath());
}

/**
 * Unlock a database with an unlock popup dialog.
 *
 * @param dbWidget DatabaseWidget which to connect signals to
 * @param intent intent for unlocking
 * @param file path of the database to be unlocked
 */
void DatabaseTabWidget::unlockDatabaseInDialog(DatabaseWidget* dbWidget,
                                               DatabaseOpenDialog::Intent intent,
                                               const QString& filePath)
{
    m_databaseOpenDialog->clearForms();
    m_databaseOpenDialog->setIntent(intent);
    m_databaseOpenDialog->setTarget(dbWidget, filePath);
    displayUnlockDialog();
}

/**
 * Unlock a database with an unlock popup dialog.
 * The dialog allows the user to select any open & locked database.
 *
 * @param intent intent for unlocking
 */
void DatabaseTabWidget::unlockAnyDatabaseInDialog(DatabaseOpenDialog::Intent intent)
{
    m_databaseOpenDialog->clearForms();
    m_databaseOpenDialog->setIntent(intent);

    // add a tab to the dialog for each open unlocked database
    for (int i = 0, c = count(); i < c; ++i) {
        auto* dbWidget = databaseWidgetFromIndex(i);
        if (dbWidget && dbWidget->isLocked()) {
            m_databaseOpenDialog->addDatabaseTab(dbWidget);
        }
    }
    // default to the current tab
    m_databaseOpenDialog->setActiveDatabaseTab(currentDatabaseWidget());
    displayUnlockDialog();
}

void DatabaseTabWidget::unlockDatabaseInDialogForSync(const QString& filePath)
{
    unlockDatabaseInDialog(currentDatabaseWidget(), DatabaseOpenDialog::Intent::RemoteSync, filePath);
}

/**
 * Display the unlock dialog after it's been initialized.
 * This is an internal method, it should only be called by unlockDatabaseInDialog or unlockAnyDatabaseInDialog.
 */
void DatabaseTabWidget::displayUnlockDialog()
{
#ifdef Q_OS_MACOS
    auto intent = m_databaseOpenDialog->intent();
    if (intent == DatabaseOpenDialog::Intent::AutoType || intent == DatabaseOpenDialog::Intent::Browser) {
        macUtils()->raiseOwnWindow();
        Tools::wait(200);
    }
#endif

    m_databaseOpenDialog->show();
    m_databaseOpenDialog->raise();
    m_databaseOpenDialog->activateWindow();
}

/**
 * Actions to take when the unlock dialog has completed.
 */
void DatabaseTabWidget::handleDatabaseUnlockDialogFinished(bool accepted, DatabaseWidget* dbWidget)
{
    // change the active tab to the database that was just unlocked in the dialog
    auto intent = m_databaseOpenDialog->intent();
    if (accepted && intent != DatabaseOpenDialog::Intent::Merge && intent != DatabaseOpenDialog::Intent::RemoteSync) {
        int index = indexOf(dbWidget);
        if (index != -1) {
            setCurrentIndex(index);
        }
    }

    // if unlocked for AutoType, set pending lock flag if needed
    if (intent == DatabaseOpenDialog::Intent::AutoType && config()->get(Config::Security_RelockAutoType).toBool()) {
        m_dbWidgetPendingLock = dbWidget;
    }

    // If browser extension requested the unlock make sure cancel is handled
    m_databaseOpenInProgress = false;

    // signal other objects that the dialog finished
    emit databaseUnlockDialogFinished(accepted, dbWidget);
}

/**
 * This function relock the pending database when autotype has been performed successfully
 * A database is marked as pending when it's unlocked after a global Auto-Type invocation
 */
void DatabaseTabWidget::relockPendingDatabase()
{
    if (!m_dbWidgetPendingLock || !config()->get(Config::Security_RelockAutoType).toBool()) {
        return;
    }

    if (m_dbWidgetPendingLock->isLocked() || !m_dbWidgetPendingLock->database()->isInitialized()) {
        m_dbWidgetPendingLock = nullptr;
        return;
    }

    m_dbWidgetPendingLock->lock();
    m_dbWidgetPendingLock = nullptr;
}

void DatabaseTabWidget::updateLastDatabases(const QSharedPointer<Database>& database)
{
    if (database->isTemporaryDatabase() || database->filePath().isEmpty()) {
        return;
    }
    auto filename = database->filePath();
    if (!config()->get(Config::RememberLastDatabases).toBool()) {
        config()->remove(Config::LastDatabases);
    } else {
        QStringList lastDatabases = config()->get(Config::LastDatabases).toStringList();
        lastDatabases.prepend(QDir::toNativeSeparators(filename));
        lastDatabases.removeDuplicates();

        while (lastDatabases.count() > config()->get(Config::NumberOfRememberedLastDatabases).toInt()) {
            lastDatabases.removeLast();
        }
        config()->set(Config::LastDatabases, lastDatabases);
    }
}

void DatabaseTabWidget::updateLastDatabases()
{
    auto dbWidget = currentDatabaseWidget();

    if (dbWidget) {
        updateLastDatabases(dbWidget->database());
    }
}

void DatabaseTabWidget::emitActiveDatabaseChanged()
{
    emit activeDatabaseChanged(currentDatabaseWidget());
}

void DatabaseTabWidget::emitDatabaseLockChanged()
{
    auto* dbWidget = qobject_cast<DatabaseWidget*>(sender());
    Q_ASSERT(dbWidget);
    if (!dbWidget) {
        return;
    }

    if (dbWidget->isLocked()) {
        emit databaseLocked(dbWidget);
    } else {
        emit databaseUnlocked(dbWidget);
        m_databaseOpenInProgress = false;
    }
}

void DatabaseTabWidget::performGlobalAutoType(const QString& search)
{
    auto currentDbWidget = currentDatabaseWidget();
    if (!currentDbWidget) {
        // No open databases, nothing to do
        return;
    } else if (currentDbWidget->isLocked()) {
        // Current database tab is locked, match behavior of browser unlock - prompt with
        // the unlock dialog even if there are additional unlocked open database tabs.
        currentDbWidget->setSearchStringForAutoType(search);
        unlockAnyDatabaseInDialog(DatabaseOpenDialog::Intent::AutoType);
    } else {
        // Current database is unlocked, use it for AutoType along with any other unlocked databases
        QList<QSharedPointer<Database>> unlockedDatabases;
        for (int i = 0, c = count(); i < c; ++i) {
            auto* dbWidget = databaseWidgetFromIndex(i);
            if (!dbWidget->isLocked()) {
                dbWidget->setSearchStringForAutoType(search);
                unlockedDatabases.append(dbWidget->database());
            }
        }

        Q_ASSERT(!unlockedDatabases.isEmpty());
        autoType()->performGlobalAutoType(unlockedDatabases, search);
    }
}

void DatabaseTabWidget::performBrowserUnlock()
{
    if (m_databaseOpenInProgress) {
        return;
    }

    m_databaseOpenInProgress = true;
    auto dbWidget = currentDatabaseWidget();
    if (dbWidget && dbWidget->isLocked()) {
        unlockAnyDatabaseInDialog(DatabaseOpenDialog::Intent::Browser);
    }
}
