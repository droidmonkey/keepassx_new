/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
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

#include "DatabaseOpenDialog.h"

#include "DatabaseOpenWidget.h"
#include "DatabaseTabWidget.h"
#include "DatabaseWidget.h"

#include <QFileInfo>
#include <QLayout>
#include <QShortcut>

#ifdef Q_OS_WIN
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif

DatabaseOpenDialog::DatabaseOpenDialog(QWidget* parent)
    : QDialog(parent)
    , m_view(new DatabaseOpenWidget(this))
    , m_tabBar(new QTabBar(this))
{
    setWindowTitle(tr("Unlock Database - KeePassXC"));
    setWindowFlags(Qt::Dialog);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
#ifdef Q_OS_LINUX
    // Linux requires this to overcome some Desktop Environments (also no Quick Unlock)
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
#endif
    // block input to the main window/application while the dialog is open
    setWindowModality(Qt::ApplicationModal);
#ifdef Q_OS_WIN
    QWindowsWindowFunctions::setWindowActivationBehavior(QWindowsWindowFunctions::AlwaysActivateWindow);
#endif
    connect(m_view, &DatabaseOpenWidget::dialogFinished, this, &DatabaseOpenDialog::complete);

    m_tabBar->setAutoHide(true);
    m_tabBar->setExpanding(false);
    connect(m_tabBar, &QTabBar::currentChanged, this, &DatabaseOpenDialog::tabChanged);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addWidget(m_view);
    setLayout(layout);
    setMinimumWidth(700);

    // set up Ctrl+PageUp / Ctrl+PageDown and Ctrl+Tab / Ctrl+Shift+Tab shortcuts to cycle tabs
    // Ctrl+Tab is broken on Mac, so use Alt (i.e. the Option key) - https://bugreports.qt.io/browse/QTBUG-8596
    auto dbTabModifier = Qt::CTRL;
#ifdef Q_OS_MACOS
    dbTabModifier = Qt::ALT;
#endif
    auto* shortcut = new QShortcut(Qt::CTRL + Qt::Key_PageUp, this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, [this]() { selectTabOffset(-1); });
    shortcut = new QShortcut(dbTabModifier + Qt::SHIFT + Qt::Key_Tab, this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, [this]() { selectTabOffset(-1); });
    shortcut = new QShortcut(Qt::CTRL + Qt::Key_PageDown, this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, [this]() { selectTabOffset(1); });
    shortcut = new QShortcut(dbTabModifier + Qt::Key_Tab, this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, [this]() { selectTabOffset(1); });
}

void DatabaseOpenDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    QTimer::singleShot(100, this, [this] {
        if (m_view->isOnQuickUnlockScreen() && !m_view->unlockingDatabase()) {
            m_view->triggerQuickUnlock();
        }
    });
}

void DatabaseOpenDialog::selectTabOffset(int offset)
{
    if (offset == 0 || m_tabBar->count() <= 1) {
        return;
    }
    int tab = m_tabBar->currentIndex() + offset;
    int last = m_tabBar->count() - 1;
    if (tab < 0) {
        tab = last;
    } else if (tab > last) {
        tab = 0;
    }
    m_tabBar->setCurrentIndex(tab);
}

void DatabaseOpenDialog::addDatabaseTab(DatabaseWidget* dbWidget)
{
    Q_ASSERT(dbWidget);
    if (!dbWidget) {
        return;
    }

    // important - we must add the DB widget first, because addTab will fire
    // tabChanged immediately which will look for a dbWidget in the list
    m_tabDbWidgets.append(dbWidget);
    QFileInfo fileInfo(dbWidget->database()->filePath());
    m_tabBar->addTab(fileInfo.fileName());
    Q_ASSERT(m_tabDbWidgets.count() == m_tabBar->count());
}

void DatabaseOpenDialog::setActiveDatabaseTab(DatabaseWidget* dbWidget)
{
    if (!dbWidget) {
        return;
    }
    int index = m_tabDbWidgets.indexOf(dbWidget);
    if (index != -1) {
        m_tabBar->setCurrentIndex(index);
    }
}

void DatabaseOpenDialog::tabChanged(int index)
{
    if (index < 0 || index >= m_tabDbWidgets.count()) {
        return;
    }

    if (m_tabDbWidgets.count() == m_tabBar->count()) {
        DatabaseWidget* dbWidget = m_tabDbWidgets[index];
        setTarget(dbWidget, dbWidget->database()->filePath());
    } else {
        // if these list sizes don't match, there's a bug somewhere nearby
        qWarning("DatabaseOpenDialog: mismatch between tab count %d and DB count %d",
                 m_tabBar->count(),
                 m_tabDbWidgets.count());
    }
}

/**
 * Sets the target DB and reloads the UI.
 */
void DatabaseOpenDialog::setTarget(DatabaseWidget* dbWidget, const QString& filePath)
{
    // reconnect finished signal to new dbWidget, then reload the UI
    if (m_currentDbWidget) {
        disconnect(this, &DatabaseOpenDialog::dialogFinished, m_currentDbWidget, nullptr);
    }
    connect(this, &DatabaseOpenDialog::dialogFinished, dbWidget, &DatabaseWidget::unlockDatabase);

    m_currentDbWidget = dbWidget;
    m_view->load(filePath);
}

void DatabaseOpenDialog::setIntent(DatabaseOpenDialog::Intent intent)
{
    m_intent = intent;
}

DatabaseOpenDialog::Intent DatabaseOpenDialog::intent() const
{
    return m_intent;
}

void DatabaseOpenDialog::clearForms()
{
    m_view->clearForms();
    m_db.reset();
    m_intent = Intent::None;
    if (m_currentDbWidget) {
        disconnect(this, &DatabaseOpenDialog::dialogFinished, m_currentDbWidget, nullptr);
    }
    m_currentDbWidget.clear();
    m_tabDbWidgets.clear();

    // block signals while removing tabs so that tabChanged doesn't get called
    m_tabBar->blockSignals(true);
    while (m_tabBar->count() > 0) {
        m_tabBar->removeTab(0);
    }
    m_tabBar->blockSignals(false);
}

void DatabaseOpenDialog::showMessage(const QString& text, MessageWidget::MessageType type, int autoHideTimeout)
{
    m_view->showMessage(text, type, autoHideTimeout);
}

QSharedPointer<Database> DatabaseOpenDialog::database() const
{
    return m_db;
}

void DatabaseOpenDialog::done(int result)
{
    hide();

    emit dialogFinished(result == QDialog::Accepted, m_currentDbWidget);
    clearForms();

    QDialog::done(result);

#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    // CDialogs are not really closed, just hidden, pre Qt 6.3?
    if (testAttribute(Qt::WA_DeleteOnClose)) {
        setAttribute(Qt::WA_DeleteOnClose, false);
        deleteLater();
    }
#endif
}

void DatabaseOpenDialog::complete(bool accepted)
{
    // save DB, since DatabaseOpenWidget will reset its data after accept() is called
    m_db = m_view->database();
    if (m_db && m_intent == Intent::RemoteSync) {
        m_db->markAsTemporaryDatabase();
    }

    if (accepted) {
        accept();
    } else {
        reject();
    }
}

void DatabaseOpenDialog::closeEvent(QCloseEvent* e)
{
    emit dialogFinished(false, m_currentDbWidget);
    clearForms();
    QDialog::closeEvent(e);
}
