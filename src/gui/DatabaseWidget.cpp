/*
 * Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 * Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#include "DatabaseWidget.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDesktopServices>
#include <QHostInfo>
#include <QInputDialog>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QProcess>
#include <QSplitter>
#include <QTextDocumentFragment>
#include <QTextEdit>

#include "autotype/AutoType.h"
#include "core/AsyncTask.h"
#include "core/EntrySearcher.h"
#include "core/Merger.h"
#include "core/Tools.h"
#include "gui/Clipboard.h"
#include "gui/CloneDialog.h"
#include "gui/DatabaseOpenDialog.h"
#include "gui/DatabaseOpenWidget.h"
#include "gui/EntryPreviewWidget.h"
#include "gui/FileDialog.h"
#include "gui/GuiTools.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "gui/TotpDialog.h"
#include "gui/TotpExportSettingsDialog.h"
#include "gui/TotpSetupDialog.h"
#include "gui/dbsettings/DatabaseSettingsDialog.h"
#include "gui/entry/EntryView.h"
#include "gui/group/EditGroupWidget.h"
#include "gui/group/GroupView.h"
#include "gui/reports/ReportsDialog.h"
#include "gui/tag/TagView.h"
#include "gui/widgets/ElidedLabel.h"
#include "keeshare/KeeShare.h"
#include "remote/RemoteHandler.h"
#include "remote/RemoteSettings.h"

#ifdef WITH_XC_NETWORKING
#include "gui/IconDownloaderDialog.h"
#endif

#ifdef WITH_XC_SSHAGENT
#include "sshagent/SSHAgent.h"
#endif

#ifdef WITH_XC_BROWSER_PASSKEYS
#include "gui/passkeys/PasskeyImporter.h"
#endif

DatabaseWidget::DatabaseWidget(QSharedPointer<Database> db, QWidget* parent)
    : QStackedWidget(parent)
    , m_db(std::move(db))
    , m_mainWidget(new QWidget(this))
    , m_mainSplitter(new QSplitter(m_mainWidget))
    , m_groupSplitter(new QSplitter(this))
    , m_messageWidget(new MessageWidget(this))
    , m_previewView(new EntryPreviewWidget(this))
    , m_previewSplitter(new QSplitter(m_mainWidget))
    , m_searchingLabel(new QLabel(this))
    , m_shareLabel(new ElidedLabel(this))
    , m_editEntryWidget(new EditEntryWidget(this))
    , m_editGroupWidget(new EditGroupWidget(this))
    , m_historyEditEntryWidget(new EditEntryWidget(this))
    , m_reportsDialog(new ReportsDialog(this))
    , m_databaseSettingDialog(new DatabaseSettingsDialog(this))
    , m_databaseOpenWidget(new DatabaseOpenWidget(this))
    , m_groupView(new GroupView(m_db.data(), this))
    , m_tagView(new TagView(this))
    , m_saveAttempts(0)
    , m_remoteSettings(new RemoteSettings(m_db, this))
    , m_entrySearcher(new EntrySearcher(false))
{
    Q_ASSERT(m_db);

    m_messageWidget->setHidden(true);

    auto mainLayout = new QVBoxLayout();
    mainLayout->addWidget(m_messageWidget);
    auto hbox = new QHBoxLayout();
    mainLayout->addLayout(hbox);
    hbox->addWidget(m_mainSplitter);
    m_mainWidget->setLayout(mainLayout);

    // Setup searches and tags view and place under groups
    m_tagView->setObjectName("tagView");
    m_tagView->setDatabase(m_db);
    connect(m_tagView, SIGNAL(activated(QModelIndex)), this, SLOT(filterByTag()));
    connect(m_tagView, SIGNAL(clicked(QModelIndex)), this, SLOT(filterByTag()));

    auto tagsWidget = new QWidget();
    auto tagsLayout = new QVBoxLayout();
    auto tagsTitle = new QLabel(tr("Searches and Tags"));
    tagsTitle->setProperty("title", true);
    tagsWidget->setObjectName("tagWidget");
    tagsWidget->setLayout(tagsLayout);
    tagsLayout->addWidget(tagsTitle);
    tagsLayout->addWidget(m_tagView);
    tagsLayout->setMargin(0);

    m_groupSplitter->setOrientation(Qt::Vertical);
    m_groupSplitter->setChildrenCollapsible(true);
    m_groupSplitter->addWidget(m_groupView);
    m_groupSplitter->addWidget(tagsWidget);
    m_groupSplitter->setStretchFactor(0, 100);
    m_groupSplitter->setStretchFactor(1, 0);
    m_groupSplitter->setSizes({1, 1});
    // Initial visibility based on config value
    m_groupSplitter->setVisible(!config()->get(Config::GUI_HideGroupPanel).toBool());

    auto rightHandSideWidget = new QWidget(m_mainSplitter);
    auto rightHandSideVBox = new QVBoxLayout();
    rightHandSideVBox->setMargin(0);
    rightHandSideVBox->addWidget(m_searchingLabel);
#ifdef WITH_XC_KEESHARE
    rightHandSideVBox->addWidget(m_shareLabel);
#endif
    rightHandSideVBox->addWidget(m_previewSplitter);
    rightHandSideWidget->setLayout(rightHandSideVBox);
    m_entryView = new EntryView(rightHandSideWidget);

    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->addWidget(m_groupSplitter);
    m_mainSplitter->addWidget(rightHandSideWidget);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 100);
    m_mainSplitter->setSizes({1, 1});

    m_previewSplitter->setOrientation(Qt::Vertical);
    m_previewSplitter->setChildrenCollapsible(true);

    m_groupView->setObjectName("groupView");
    m_groupView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_groupView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(emitGroupContextMenuRequested(QPoint)));

    m_entryView->setObjectName("entryView");
    m_entryView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_entryView->displayGroup(m_db->rootGroup());
    connect(m_entryView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(emitEntryContextMenuRequested(QPoint)));

    // Add a notification for when we are searching
    m_searchingLabel->setObjectName("SearchBanner");
    m_searchingLabel->setText(tr("Searching…"));
    m_searchingLabel->setAlignment(Qt::AlignCenter);
    m_searchingLabel->setVisible(false);

#ifdef WITH_XC_KEESHARE
    m_shareLabel->setObjectName("KeeShareBanner");
    m_shareLabel->setRawText(tr("Shared group…"));
    m_shareLabel->setAlignment(Qt::AlignCenter);
    m_shareLabel->setVisible(false);
#endif

    m_previewView->setObjectName("previewWidget");
    m_previewView->hide();
    m_previewSplitter->addWidget(m_entryView);
    m_previewSplitter->addWidget(m_previewView);
    m_previewSplitter->setStretchFactor(0, 100);
    m_previewSplitter->setStretchFactor(1, 0);
    m_previewSplitter->setSizes({1, 1});

    m_editEntryWidget->setObjectName("editEntryWidget");
    m_historyEditEntryWidget->setObjectName("editEntryHistoryWidget");
    m_editGroupWidget->setObjectName("editGroupWidget");
    m_reportsDialog->setObjectName("reportsDialog");
    m_databaseSettingDialog->setObjectName("databaseSettingsDialog");
    m_databaseOpenWidget->setObjectName("databaseOpenWidget");

    addChildWidget(m_mainWidget);
    addChildWidget(m_editEntryWidget);
    addChildWidget(m_editGroupWidget);
    addChildWidget(m_reportsDialog);
    addChildWidget(m_databaseSettingDialog);
    addChildWidget(m_historyEditEntryWidget);
    addChildWidget(m_databaseOpenWidget);

    // clang-format off
    connect(m_mainSplitter, SIGNAL(splitterMoved(int,int)), SIGNAL(splitterSizesChanged()));
    connect(m_groupSplitter, SIGNAL(splitterMoved(int,int)), SIGNAL(splitterSizesChanged()));
    connect(m_previewSplitter, SIGNAL(splitterMoved(int,int)), SIGNAL(splitterSizesChanged()));
    connect(this, SIGNAL(currentModeChanged(DatabaseWidget::Mode)), m_previewView, SLOT(setDatabaseMode(DatabaseWidget::Mode)));
    connect(m_previewView, SIGNAL(entryUrlActivated(Entry*)), SLOT(openUrlForEntry(Entry*)));
    connect(m_previewView, SIGNAL(copyTextRequested(const QString&)), SLOT(setClipboardTextAndMinimize(const QString&)));
    connect(m_entryView, SIGNAL(viewStateChanged()), SIGNAL(entryViewStateChanged()));
    connect(m_groupView, SIGNAL(groupSelectionChanged()), SLOT(onGroupChanged()));
    connect(m_groupView, &GroupView::groupFocused, this, [this] { m_previewView->setGroup(currentGroup()); });
    connect(m_entryView, SIGNAL(entryActivated(Entry*,EntryModel::ModelColumn)),
        SLOT(entryActivationSignalReceived(Entry*,EntryModel::ModelColumn)));
    connect(m_entryView, SIGNAL(entrySelectionChanged(Entry*)), SLOT(onEntryChanged(Entry*)));
    connect(m_editEntryWidget, SIGNAL(editFinished(bool)), SLOT(switchToMainView(bool)));
    connect(m_editEntryWidget, SIGNAL(historyEntryActivated(Entry*)), SLOT(switchToHistoryView(Entry*)));
    connect(m_historyEditEntryWidget, SIGNAL(editFinished(bool)), SLOT(switchBackToEntryEdit()));
    connect(m_editGroupWidget, SIGNAL(editFinished(bool)), SLOT(switchToMainView(bool)));
    connect(m_reportsDialog, SIGNAL(editFinished(bool)), SLOT(switchToMainView(bool)));
    connect(m_databaseSettingDialog, SIGNAL(editFinished(bool)), SLOT(switchToMainView(bool)));
    connect(m_databaseOpenWidget, SIGNAL(dialogFinished(bool)), SLOT(loadDatabase(bool)));
    connect(this, SIGNAL(currentChanged(int)), SLOT(emitCurrentModeChanged()));
    connect(this, SIGNAL(requestGlobalAutoType(const QString&)), parent, SLOT(performGlobalAutoType(const QString&)));
    connect(config(), &Config::changed, this, &DatabaseWidget::onConfigChanged);
    // clang-format on

    connectDatabaseSignals();

    m_blockAutoSave = false;
    m_reloading = false;

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    connect(m_autosaveTimer, SIGNAL(timeout()), this, SLOT(onAutosaveDelayTimeout()));

    m_searchLimitGroup = config()->get(Config::SearchLimitGroup).toBool();

#ifdef WITH_XC_KEESHARE
    // We need to reregister the database to allow exports
    // from a newly created database
    KeeShare::instance()->connectDatabase(m_db, {});
#endif

    if (m_db->isInitialized()) {
        switchToMainView();
    } else {
        switchToOpenDatabase();
    }
}

DatabaseWidget::DatabaseWidget(const QString& filePath, QWidget* parent)
    : DatabaseWidget(QSharedPointer<Database>::create(filePath), parent)
{
}

DatabaseWidget::~DatabaseWidget()
{
    // Trigger any Database deletion related signals manually by
    // explicitly clearing the Database pointer, instead of leaving it to ~QSharedPointer.
    // QSharedPointer may behave differently depending on whether it is cleared by the `clear` method
    // or by its destructor. In the latter case, the ref counter may not be correctly maintained
    // if a copy of the QSharedPointer is created in any slots activated by the Database destructor.
    // More details: https://github.com/keepassxreboot/keepassxc/issues/6393.
    m_db.clear();
}

QSharedPointer<Database> DatabaseWidget::database() const
{
    return m_db;
}

DatabaseWidget::Mode DatabaseWidget::currentMode() const
{
    auto mode = Mode::None;
    auto widget = currentWidget();
    if (widget == m_mainWidget) {
        mode = Mode::ViewMode;
    } else if (widget == m_databaseOpenWidget) {
        mode = Mode::LockedMode;
    } else if (widget == m_reportsDialog) {
        mode = Mode::ReportsMode;
    } else if (widget == m_databaseSettingDialog) {
        mode = Mode::DatabaseSettingsMode;
    } else if (widget == m_editEntryWidget || widget == m_historyEditEntryWidget) {
        mode = Mode::EditEntryMode;
    } else if (widget == m_editGroupWidget) {
        mode = Mode::EditGroupMode;
    } else {
        // We are missing a condition if we reach here
        Q_ASSERT(false);
    }

    return mode;
}

bool DatabaseWidget::isLocked() const
{
    return currentMode() == Mode::LockedMode;
}

bool DatabaseWidget::isSaving() const
{
    return m_db->isSaving();
}

bool DatabaseWidget::isSorted() const
{
    return m_entryView->isSorted();
}

bool DatabaseWidget::isSearchActive() const
{
    return m_entryView->inSearchMode();
}

bool DatabaseWidget::isEntryViewActive() const
{
    return currentWidget() == m_mainWidget;
}

bool DatabaseWidget::isEntryEditActive() const
{
    return currentWidget() == m_editEntryWidget;
}

bool DatabaseWidget::isGroupEditActive() const
{
    return currentWidget() == m_editGroupWidget;
}

bool DatabaseWidget::isEditWidgetModified() const
{
    if (currentWidget() == m_editEntryWidget) {
        return m_editEntryWidget->isModified();
    } else if (currentWidget() == m_editGroupWidget) {
        return m_editGroupWidget->isModified();
    }
    return false;
}

QString DatabaseWidget::displayName() const
{
    if (!m_db) {
        return {};
    }

    auto displayName = m_db->metadata()->name();
    if (!m_db->filePath().isEmpty()) {
        if (displayName.isEmpty()) {
            displayName = displayFileName();
        }
    } else {
        if (displayName.isEmpty()) {
            displayName = tr("New Database");
        } else {
            displayName = tr("%1 [New Database]", "Database tab name modifier").arg(displayName);
        }
    }

    return displayName;
}

QString DatabaseWidget::displayFileName() const
{
    if (m_db) {
        QFileInfo fileinfo(m_db->filePath());
        return fileinfo.fileName();
    }
    return {};
}

QString DatabaseWidget::displayFilePath() const
{
    if (m_db) {
        return m_db->canonicalFilePath();
    }
    return {};
}

QHash<Config::ConfigKey, QList<int>> DatabaseWidget::splitterSizes() const
{
    return {{Config::GUI_SplitterState, m_mainSplitter->sizes()},
            {Config::GUI_PreviewSplitterState, m_previewSplitter->sizes()},
            {Config::GUI_GroupSplitterState, m_groupSplitter->sizes()}};
}

void DatabaseWidget::setSplitterSizes(const QHash<Config::ConfigKey, QList<int>>& sizes)
{
    // Set the splitter sizes, if the size is invalid set a default ratio based on this widget size
    for (auto itr = sizes.constBegin(); itr != sizes.constEnd(); ++itr) {
        auto value = itr.value();
        switch (itr.key()) {
        case Config::GUI_SplitterState:
            if (value.size() < 2) {
                value = QList({static_cast<int>(width() * 0.25), static_cast<int>(width() * 0.75)});
            }
            m_mainSplitter->setSizes(value);
            break;
        case Config::GUI_PreviewSplitterState:
            if (value.size() < 2) {
                value = QList({static_cast<int>(height() * 0.8), static_cast<int>(height() * 0.2)});
            }
            m_previewSplitter->setSizes(value);
            break;
        case Config::GUI_GroupSplitterState:
            if (value.size() < 2) {
                value = QList({static_cast<int>(height() * 0.6), static_cast<int>(height() * 0.4)});
            }
            m_groupSplitter->setSizes(value);
            break;
        default:
            break;
        }
    }
}

void DatabaseWidget::onConfigChanged(Config::ConfigKey key)
{
    if (key == Config::GUI_HideGroupPanel) {
        // Toggle the group splitter visibility and reset the size
        m_groupSplitter->setVisible(!config()->get(Config::GUI_HideGroupPanel).toBool());
        setSplitterSizes({{Config::GUI_SplitterState, QList<int>({})}});
    }
}

void DatabaseWidget::setSearchStringForAutoType(const QString& search)
{
    m_searchStringForAutoType = search;
}

/**
 * Get current view state of entry view
 */
QByteArray DatabaseWidget::entryViewState() const
{
    return m_entryView->viewState();
}

/**
 * Set view state of entry view
 */
bool DatabaseWidget::setEntryViewState(const QByteArray& state) const
{
    return m_entryView->setViewState(state);
}

void DatabaseWidget::clearAllWidgets()
{
    m_editEntryWidget->clear();
    m_historyEditEntryWidget->clear();
    m_editGroupWidget->clear();
    m_previewView->clear();
}

void DatabaseWidget::emitCurrentModeChanged()
{
    emit currentModeChanged(currentMode());
}

void DatabaseWidget::createEntry()
{
    Q_ASSERT(m_groupView->currentGroup());
    if (!m_groupView->currentGroup()) {
        return;
    }

    m_newEntry.reset(new Entry());

    m_newEntry->setUuid(QUuid::createUuid());
    m_newEntry->setUsername(m_db->metadata()->defaultUserName());
    m_newParent = m_groupView->currentGroup();
    m_newParent->applyGroupIconOnCreateTo(m_newEntry.data());
    switchToEntryEdit(m_newEntry.data(), true);
}

void DatabaseWidget::replaceDatabase(QSharedPointer<Database> db)
{
    Q_ASSERT(!isEntryEditActive() && !isGroupEditActive());

    // Save off new parent UUID which will be valid when creating a new entry
    QUuid newParentUuid;
    if (m_newParent) {
        newParentUuid = m_newParent->uuid();
    }

    // TODO: instead of increasing the ref count temporarily, there should be a clean
    // break from the old database. Without this crashes occur due to the change
    // signals triggering dangling pointers.
    auto oldDb = m_db;
    m_db = std::move(db);
    connectDatabaseSignals();
    m_groupView->changeDatabase(m_db);
    m_tagView->setDatabase(m_db);
    m_remoteSettings->setDatabase(m_db);

    // Restore the new parent group pointer, if not found default to the root group
    // this prevents data loss when merging a database while creating a new entry
    if (!newParentUuid.isNull()) {
        m_newParent = m_db->rootGroup()->findGroupByUuid(newParentUuid);
        if (!m_newParent) {
            m_newParent = m_db->rootGroup();
        }
    }

    emit databaseReplaced(oldDb, m_db);

#if defined(WITH_XC_KEESHARE)
    KeeShare::instance()->connectDatabase(m_db, oldDb);
#else
    // Keep the instance active till the end of this function
    Q_UNUSED(oldDb);
#endif

    oldDb->releaseData();
}

void DatabaseWidget::cloneEntry()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    auto cloneDialog = new CloneDialog(this, m_db.data(), currentEntry);
    connect(cloneDialog, &CloneDialog::entryCloned, this, [this](auto entry) {
        refreshSearch();
        m_entryView->setCurrentEntry(entry);
    });

    cloneDialog->show();
}

void DatabaseWidget::showTotp()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    auto totpDialog = new TotpDialog(this, currentEntry);
    connect(this, &DatabaseWidget::databaseLockRequested, totpDialog, &TotpDialog::close);
    totpDialog->open();
}

void DatabaseWidget::copyTotp()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }
    setClipboardTextAndMinimize(currentEntry->totp());
}

void DatabaseWidget::setupTotp()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    auto setupTotpDialog = new TotpSetupDialog(this, currentEntry);
    connect(setupTotpDialog, SIGNAL(totpUpdated()), SIGNAL(entrySelectionChanged()));
    if (currentWidget() == m_editEntryWidget) {
        // Entry is being edited, tell it when we are finished updating TOTP
        connect(setupTotpDialog, SIGNAL(totpUpdated()), m_editEntryWidget, SLOT(updateTotp()));
    }
    connect(this, &DatabaseWidget::databaseLockRequested, setupTotpDialog, &TotpSetupDialog::close);
    setupTotpDialog->open();
}

void DatabaseWidget::expireSelectedEntries()
{
    const QModelIndexList selected = m_entryView->selectionModel()->selectedRows();
    for (const auto& index : selected) {
        auto entry = m_entryView->entryFromIndex(index);
        if (entry) {
            entry->expireNow();
        }
    }
}

void DatabaseWidget::deleteSelectedEntries()
{
    const QModelIndexList selected = m_entryView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }

    // Resolve entries from the selection model
    QList<Entry*> selectedEntries;
    for (const QModelIndex& index : selected) {
        selectedEntries.append(m_entryView->entryFromIndex(index));
    }

    deleteEntries(std::move(selectedEntries));
}

void DatabaseWidget::restoreSelectedEntries()
{
    const QModelIndexList selected = m_entryView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }

    // Resolve entries from the selection model
    QList<Entry*> selectedEntries;
    for (auto& index : selected) {
        selectedEntries.append(m_entryView->entryFromIndex(index));
    }

    for (auto* entry : selectedEntries) {
        if (entry->previousParentGroup()) {
            entry->setGroup(entry->previousParentGroup());
        }
    }
}

void DatabaseWidget::deleteEntries(QList<Entry*> selectedEntries, bool confirm)
{
    if (selectedEntries.isEmpty()) {
        return;
    }

    // Find the index above the first entry for selection after deletion
    auto index = m_entryView->indexFromEntry(selectedEntries.first());
    index = m_entryView->indexAbove(index);

    // Confirm entry removal before moving forward
    auto recycleBin = m_db->metadata()->recycleBin();
    bool permanent = (recycleBin && recycleBin->findEntryByUuid(selectedEntries.first()->uuid()))
                     || !m_db->metadata()->recycleBinEnabled();

    if (confirm && !GuiTools::confirmDeleteEntries(this, selectedEntries, permanent)) {
        return;
    }

    GuiTools::deleteEntriesResolveReferences(this, selectedEntries, permanent);

    // Select the row above the deleted entries
    if (index.isValid()) {
        m_entryView->setCurrentIndex(index);
    } else {
        m_entryView->setFirstEntryActive();
    }
}

void DatabaseWidget::setFocus(Qt::FocusReason reason)
{
    focusNextPrevChild(reason == Qt::TabFocusReason);
}

void DatabaseWidget::focusOnEntries(bool editIfFocused)
{
    if (isEntryViewActive()) {
        if (editIfFocused && m_entryView->hasFocus()) {
            switchToEntryEdit();
        } else {
            m_entryView->setFocus();
        }
    }
}

void DatabaseWidget::focusOnGroups(bool editIfFocused)
{
    if (isEntryViewActive()) {
        if (editIfFocused && m_groupView->hasFocus()) {
            switchToGroupEdit();
        } else {
            m_groupView->setFocus();
        }
    }
}

void DatabaseWidget::moveEntryUp()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        currentEntry->moveUp();
        m_entryView->setCurrentEntry(currentEntry);
    }
}

void DatabaseWidget::moveEntryDown()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        currentEntry->moveDown();
        m_entryView->setCurrentEntry(currentEntry);
    }
}

void DatabaseWidget::copyTitle()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->title()));
    }
}

void DatabaseWidget::copyUsername()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->username()));
    }
}

void DatabaseWidget::copyPassword()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->password()));
    }
}

void DatabaseWidget::copyPasswordTotp()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(
            currentEntry->resolveMultiplePlaceholders(currentEntry->password()).append(currentEntry->totp()));
    }
}

void DatabaseWidget::copyURL()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->url()));
    }
}

void DatabaseWidget::copyNotes()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(currentEntry->resolveMultiplePlaceholders(currentEntry->notes()));
    }
}

void DatabaseWidget::copyAttribute(QAction* action)
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        setClipboardTextAndMinimize(
            currentEntry->resolveMultiplePlaceholders(currentEntry->attributes()->value(action->data().toString())));
    }
}

bool DatabaseWidget::copyFocusedTextSelection()
{
    // If a focused child widget has text selected, copy that text to the clipboard
    // and return true. Otherwise, return false.

    const bool clearClipboard = config()->get(Config::Security_ClearClipboard).toBool();

    const auto plainTextEdit = qobject_cast<QPlainTextEdit*>(focusWidget());
    if (plainTextEdit && plainTextEdit->textCursor().hasSelection()) {
        clipboard()->setText(plainTextEdit->textCursor().selectedText(), clearClipboard);
        return true;
    }

    const auto label = qobject_cast<QLabel*>(focusWidget());
    if (label && label->hasSelectedText()) {
        clipboard()->setText(label->selectedText(), clearClipboard);
        return true;
    }

    const auto textEdit = qobject_cast<QTextEdit*>(focusWidget());
    if (textEdit && textEdit->textCursor().hasSelection()) {
        clipboard()->setText(textEdit->textCursor().selection().toPlainText(), clearClipboard);
        return true;
    }

    return false;
}

void DatabaseWidget::filterByTag()
{
    QStringList searchTerms;
    const auto selections = m_tagView->selectionModel()->selectedIndexes();
    for (const auto& index : selections) {
        searchTerms << index.data(Qt::UserRole).toString();
    }
    emit requestSearch(searchTerms.join(" "));
}

void DatabaseWidget::setTag(QAction* action)
{
    auto tag = action->text();
    auto state = action->isChecked();
    for (auto entry : m_entryView->selectedEntries()) {
        state ? entry->addTag(tag) : entry->removeTag(tag);
    }
}

void DatabaseWidget::showTotpKeyQrCode()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        auto totpDisplayDialog = new TotpExportSettingsDialog(this, currentEntry);
        connect(this, &DatabaseWidget::databaseLockRequested, totpDisplayDialog, &TotpExportSettingsDialog::close);
        totpDisplayDialog->open();
    }
}

void DatabaseWidget::setClipboardTextAndMinimize(const QString& text)
{
    clipboard()->setText(text);
    if (config()->get(Config::HideWindowOnCopy).toBool()) {
        if (config()->get(Config::MinimizeOnCopy).toBool()) {
            getMainWindow()->minimizeOrHide();
        } else if (config()->get(Config::DropToBackgroundOnCopy).toBool()) {
            window()->lower();
        }
    }
}

#ifdef WITH_XC_SSHAGENT
void DatabaseWidget::addToAgent()
{
    Entry* currentEntry = m_entryView->currentEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    KeeAgentSettings settings;
    if (!settings.fromEntry(currentEntry)) {
        return;
    }

    SSHAgent* agent = SSHAgent::instance();
    OpenSSHKey key;
    if (settings.toOpenSSHKey(currentEntry, key, true)) {
        if (!agent->addIdentity(key, settings, database()->uuid())) {
            m_messageWidget->showMessage(agent->errorString(), MessageWidget::Error);
        }
    } else {
        m_messageWidget->showMessage(settings.errorString(), MessageWidget::Error);
    }
}

void DatabaseWidget::removeFromAgent()
{
    Entry* currentEntry = m_entryView->currentEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return;
    }

    KeeAgentSettings settings;
    if (!settings.fromEntry(currentEntry)) {
        return;
    }

    SSHAgent* agent = SSHAgent::instance();
    OpenSSHKey key;
    if (settings.toOpenSSHKey(currentEntry, key, false)) {
        if (!agent->removeIdentity(key)) {
            m_messageWidget->showMessage(agent->errorString(), MessageWidget::Error);
        }
    } else {
        m_messageWidget->showMessage(settings.errorString(), MessageWidget::Error);
    }
}
#endif

void DatabaseWidget::performAutoType(const QString& sequence)
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        // TODO: Include name of previously active window in confirmation question
        if (config()->get(Config::Security_AutoTypeAsk).toBool()
            && MessageBox::question(
                   this, tr("Confirm Auto-Type"), tr("Perform Auto-Type into the previously active window?"))
                   != MessageBox::Yes) {
            return;
        }

        if (sequence.isEmpty()) {
            autoType()->performAutoType(currentEntry);
        } else {
            autoType()->performAutoTypeWithSequence(currentEntry, sequence);
        }
    }
}

void DatabaseWidget::performAutoTypeUsername()
{
    performAutoType(QStringLiteral("{USERNAME}"));
}

void DatabaseWidget::performAutoTypeUsernameEnter()
{
    performAutoType(QStringLiteral("{USERNAME}{ENTER}"));
}

void DatabaseWidget::performAutoTypePassword()
{
    performAutoType(QStringLiteral("{PASSWORD}"));
}

void DatabaseWidget::performAutoTypePasswordEnter()
{
    performAutoType(QStringLiteral("{PASSWORD}{ENTER}"));
}

void DatabaseWidget::performAutoTypeTOTP()
{
    performAutoType(QStringLiteral("{TOTP}"));
}

void DatabaseWidget::openUrl()
{
    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        openUrlForEntry(currentEntry);
    }
}

void DatabaseWidget::downloadSelectedFavicons()
{
#ifdef WITH_XC_NETWORKING
    QList<Entry*> selectedEntries;
    for (const auto& index : m_entryView->selectionModel()->selectedRows()) {
        selectedEntries.append(m_entryView->entryFromIndex(index));
    }

    // Force download even if icon already exists
    performIconDownloads(selectedEntries, true);
#endif
}

void DatabaseWidget::downloadAllFavicons()
{
#ifdef WITH_XC_NETWORKING
    auto currentGroup = m_groupView->currentGroup();
    if (currentGroup) {
        performIconDownloads(currentGroup->entries());
    }
#endif
}

void DatabaseWidget::downloadFaviconInBackground(Entry* entry)
{
#ifdef WITH_XC_NETWORKING
    performIconDownloads({entry}, true, true);
#else
    Q_UNUSED(entry);
#endif
}

void DatabaseWidget::performIconDownloads(const QList<Entry*>& entries, bool force, bool downloadInBackground)
{
#ifdef WITH_XC_NETWORKING
    auto* iconDownloaderDialog = new IconDownloaderDialog(this);
    connect(this, SIGNAL(databaseLockRequested()), iconDownloaderDialog, SLOT(close()));

    if (downloadInBackground && entries.count() > 0) {
        iconDownloaderDialog->downloadFaviconInBackground(m_db, entries.first());
    } else {
        iconDownloaderDialog->downloadFavicons(m_db, entries, force);
    }
#else
    Q_UNUSED(entries);
    Q_UNUSED(force);
    Q_UNUSED(downloadInBackground);
#endif
}

void DatabaseWidget::openUrlForEntry(Entry* entry)
{
    Q_ASSERT(entry);
    if (!entry) {
        return;
    }

    QString cmdString = entry->resolveMultiplePlaceholders(entry->url());
    if (cmdString.startsWith("cmd://")) {
        // check if decision to execute command was stored
        bool launch = (entry->attributes()->value(EntryAttributes::RememberCmdExecAttr) == "1");

        // otherwise ask user
        if (!launch && cmdString.length() > 6) {
            QString cmdTruncated = entry->resolveMultiplePlaceholders(entry->maskPasswordPlaceholders(entry->url()));
            cmdTruncated = cmdTruncated.mid(6);
            if (cmdTruncated.length() > 400) {
                cmdTruncated = cmdTruncated.left(400) + " […]";
            }
            QMessageBox msgbox(QMessageBox::Icon::Question,
                               tr("Execute command?"),
                               tr("Do you really want to execute the following command?<br><br>%1<br>")
                                   .arg(cmdTruncated.toHtmlEscaped()),
                               QMessageBox::Yes | QMessageBox::No,
                               this);
            msgbox.setDefaultButton(QMessageBox::No);

            auto checkbox = new QCheckBox(tr("Remember my choice"), &msgbox);
            msgbox.setCheckBox(checkbox);
            bool remember = false;
            QObject::connect(checkbox, &QCheckBox::stateChanged, [&](int state) {
                if (static_cast<Qt::CheckState>(state) == Qt::CheckState::Checked) {
                    remember = true;
                }
            });

            int result = msgbox.exec();
            launch = (result == QMessageBox::Yes);

            if (remember) {
                entry->attributes()->set(EntryAttributes::RememberCmdExecAttr, result == QMessageBox::Yes ? "1" : "0");
            }
        }

        if (launch) {
            const QString cmd = cmdString.mid(6);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            QStringList cmdList = QProcess::splitCommand(cmd);
            if (!cmdList.isEmpty()) {
                const QString program = cmdList.takeFirst();
                QProcess::startDetached(program, cmdList);
            }
#else
            QProcess::startDetached(cmd);
#endif

            if (config()->get(Config::MinimizeOnOpenUrl).toBool()) {
                getMainWindow()->minimizeOrHide();
            }
        }
    } else if (cmdString.startsWith("kdbx://")) {
        openDatabaseFromEntry(entry, false);
    } else {
        QUrl url = QUrl::fromUserInput(entry->resolveMultiplePlaceholders(entry->url()));
        if (!url.isEmpty()) {
#ifdef KEEPASSXC_DIST_APPIMAGE
            QProcess::execute("xdg-open", {url.toString(QUrl::FullyEncoded)});
#else
            QDesktopServices::openUrl(url);
#endif

            if (config()->get(Config::MinimizeOnOpenUrl).toBool()) {
                getMainWindow()->minimizeOrHide();
            }
        }
    }
}

Entry* DatabaseWidget::currentSelectedEntry() const
{
    if (currentWidget() == m_editEntryWidget) {
        return m_editEntryWidget->currentEntry();
    }

    return m_entryView->currentEntry();
}

void DatabaseWidget::createGroup()
{
    Q_ASSERT(m_groupView->currentGroup());
    if (!m_groupView->currentGroup()) {
        return;
    }

    m_newGroup.reset(new Group());
    m_newGroup->setUuid(QUuid::createUuid());
    m_newParent = m_groupView->currentGroup();
    switchToGroupEdit(m_newGroup.data(), true);
}

void DatabaseWidget::cloneGroup()
{
    Group* currentGroup = m_groupView->currentGroup();
    Q_ASSERT(currentGroup && canCloneCurrentGroup());
    if (!currentGroup || !canCloneCurrentGroup()) {
        return;
    }

    m_newGroup.reset(currentGroup->clone(Entry::CloneCopy, Group::CloneDefault | Group::CloneRenameTitle));
    m_newParent = currentGroup->parentGroup();
    switchToGroupEdit(m_newGroup.data(), true);
}

void DatabaseWidget::deleteGroup()
{
    Group* currentGroup = m_groupView->currentGroup();
    Q_ASSERT(currentGroup && canDeleteCurrentGroup());
    if (!currentGroup || !canDeleteCurrentGroup()) {
        return;
    }

    auto* recycleBin = m_db->metadata()->recycleBin();
    bool inRecycleBin = recycleBin && recycleBin->findGroupByUuid(currentGroup->uuid());
    bool isRecycleBin = recycleBin && (currentGroup == recycleBin);
    bool isRecycleBinSubgroup = recycleBin && currentGroup->findGroupByUuid(recycleBin->uuid());
    if (inRecycleBin || isRecycleBin || isRecycleBinSubgroup || !m_db->metadata()->recycleBinEnabled()) {
        auto result = MessageBox::question(
            this,
            tr("Delete group"),
            tr("Do you really want to delete the group \"%1\" for good?").arg(currentGroup->name().toHtmlEscaped()),
            MessageBox::Delete | MessageBox::Cancel,
            MessageBox::Cancel);

        if (result == MessageBox::Delete) {
            delete currentGroup;
        }
    } else {
        auto result = MessageBox::question(this,
                                           tr("Move group to recycle bin?"),
                                           tr("Do you really want to move the group "
                                              "\"%1\" to the recycle bin?")
                                               .arg(currentGroup->name().toHtmlEscaped()),
                                           MessageBox::Move | MessageBox::Cancel,
                                           MessageBox::Cancel);
        if (result == MessageBox::Move) {
            m_db->recycleGroup(currentGroup);
        }
    }
}

int DatabaseWidget::addChildWidget(QWidget* w)
{
    w->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    int index = QStackedWidget::addWidget(w);
    adjustSize();
    return index;
}

void DatabaseWidget::syncWithRemote(const RemoteParams* params)
{
    setDisabled(true);
    emit databaseSyncInProgress();

    QScopedPointer<RemoteHandler> remoteHandler(new RemoteHandler(this));
    RemoteHandler::RemoteResult result;
    result.success = false;
    result.errorMessage = tr("Remote Sync did not contain any download or upload commands.");

    // Download the database
    if (!params->downloadCommand.isEmpty()) {
        emit updateSyncProgress(25, tr("Downloading..."));
        // Start a download first then merge and upload in the callback
        result = remoteHandler->download(params);
        if (result.success) {
            QString error;
            QSharedPointer<Database> remoteDb = QSharedPointer<Database>::create();
            if (!remoteDb->open(result.filePath, m_db->key(), &error)) {
                // Failed to open downloaded remote database with same key
                // Unlock downloaded remote database via dialog
                syncDatabaseWithLockedDatabase(result.filePath, params);
                return;
            }
            remoteDb->markAsTemporaryDatabase();
            if (!syncWithDatabase(remoteDb, error)) {
                // Something failed during the sync process
                result.success = false;
                result.errorMessage = error;
            }
        }
    }

    uploadAndFinishSync(params, result);
}

void DatabaseWidget::syncDatabaseWithLockedDatabase(const QString& filePath, const RemoteParams* params)
{
    // disconnect any previously added slots to these signal
    disconnect(this, &DatabaseWidget::databaseSyncUnlocked, nullptr, nullptr);
    disconnect(this, &DatabaseWidget::databaseSyncUnlockFailed, nullptr, nullptr);

    connect(this, &DatabaseWidget::databaseSyncUnlocked, [this, params](const RemoteHandler::RemoteResult& result) {
        uploadAndFinishSync(params, result);
    });
    connect(this, &DatabaseWidget::databaseSyncUnlockFailed, [this, params](const RemoteHandler::RemoteResult& result) {
        finishSync(params, result);
    });

    emit unlockDatabaseInDialogForSync(filePath);
}

void DatabaseWidget::uploadAndFinishSync(const RemoteParams* params, RemoteHandler::RemoteResult result)
{
    QScopedPointer<RemoteHandler> remoteHandler(new RemoteHandler(this));
    if (result.success && !params->uploadCommand.isEmpty()) {
        emit updateSyncProgress(75, tr("Uploading..."));
        result = remoteHandler->upload(result.filePath, params);
    }

    finishSync(params, result);
}

void DatabaseWidget::finishSync(const RemoteParams* params, RemoteHandler::RemoteResult result)
{
    setDisabled(false);
    emit updateSyncProgress(-1, "");
    if (result.success) {
        emit databaseSyncCompleted(params->name);
        showMessage(tr("Remote sync '%1' completed successfully!").arg(params->name), MessageWidget::Positive, false);
    } else {
        emit databaseSyncFailed(params->name, result.errorMessage);
        showErrorMessage(tr("Remote sync '%1' failed: %2").arg(params->name, result.errorMessage));
    }
}

QList<RemoteParams*> DatabaseWidget::getRemoteParams() const
{
    return m_remoteSettings->getAllRemoteParams();
}

void DatabaseWidget::switchToMainView(bool previousDialogAccepted)
{
    setCurrentWidget(m_mainWidget);

    if (m_newGroup) {
        if (previousDialogAccepted) {
            m_newGroup->setParent(m_newParent);
            m_groupView->setCurrentGroup(m_newGroup.take());
            m_groupView->expandGroup(m_newParent);
        } else {
            m_newGroup.reset();
        }

        m_newParent = nullptr;
    } else if (m_newEntry) {
        if (previousDialogAccepted) {
            m_newEntry->setGroup(m_newParent);
            m_entryView->setFocus();
            m_entryView->setCurrentEntry(m_newEntry.take());
        } else {
            m_newEntry.reset();
        }

        m_newParent = nullptr;
    } else {
        // Workaround: ensure entries are focused so search doesn't reset
        m_entryView->setFocus();
    }
}

void DatabaseWidget::switchToHistoryView(Entry* entry)
{
    auto entryTitle = m_editEntryWidget->currentEntry() ? m_editEntryWidget->currentEntry()->title() : "";
    m_historyEditEntryWidget->loadEntry(entry, false, true, entryTitle, m_db);
    setCurrentWidget(m_historyEditEntryWidget);
}

void DatabaseWidget::switchBackToEntryEdit()
{
    setCurrentWidget(m_editEntryWidget);
}

void DatabaseWidget::switchToEntryEdit(Entry* entry)
{
    switchToEntryEdit(entry, false);
}

void DatabaseWidget::switchToEntryEdit(Entry* entry, bool create)
{
    // If creating an entry, it will be in `currentGroup()` so it's
    // okay to use but when editing, the entry may not be in
    // `currentGroup()` so we get the entry's group.
    Group* group;
    if (create) {
        group = currentGroup();
    } else {
        group = entry->group();
        // Ensure we have only this entry selected
        m_entryView->setCurrentEntry(entry);
    }

    Q_ASSERT(group);

    // Setup the entry edit widget and display
    m_editEntryWidget->loadEntry(entry, create, false, group->name(), m_db);
    setCurrentWidget(m_editEntryWidget);
}

void DatabaseWidget::switchToGroupEdit(Group* group, bool create)
{
    m_editGroupWidget->loadGroup(group, create, m_db);
    setCurrentWidget(m_editGroupWidget);
}

void DatabaseWidget::connectDatabaseSignals()
{
    // relayed Database events
    connect(m_db.data(),
            SIGNAL(filePathChanged(QString, QString)),

            SIGNAL(databaseFilePathChanged(QString, QString)));
    connect(m_db.data(), &Database::modified, this, &DatabaseWidget::databaseModified);
    connect(m_db.data(), &Database::modified, this, &DatabaseWidget::onDatabaseModified);
    connect(m_db.data(), &Database::databaseSaved, this, &DatabaseWidget::databaseSaved);
    connect(m_db.data(), &Database::databaseFileChanged, this, &DatabaseWidget::reloadDatabaseFile);
    connect(m_db.data(), &Database::databaseNonDataChanged, this, &DatabaseWidget::databaseNonDataChanged);
    connect(m_db.data(), &Database::databaseNonDataChanged, this, &DatabaseWidget::onDatabaseNonDataChanged);
}

void DatabaseWidget::loadDatabase(bool accepted)
{
    auto* openWidget = qobject_cast<DatabaseOpenWidget*>(sender());
    Q_ASSERT(openWidget);
    if (!openWidget) {
        return;
    }

    if (accepted) {
        emit databaseAboutToUnlock();
        replaceDatabase(openWidget->database());
        switchToMainView();
        processAutoOpen();

        restoreGroupEntryFocus(m_groupBeforeLock, m_entryBeforeLock);

        // Only show expired entries if first unlock and option is enabled
        if (m_groupBeforeLock.isNull() && config()->get(Config::GUI_ShowExpiredEntriesOnDatabaseUnlock).toBool()) {
            int expirationOffset = config()->get(Config::GUI_ShowExpiredEntriesOnDatabaseUnlockOffsetDays).toInt();
            if (expirationOffset <= 0) {
                m_nextSearchLabelText = tr("Expired entries");
            } else {
                m_nextSearchLabelText =
                    tr("Entries expiring within %1 day(s)", "", expirationOffset).arg(expirationOffset);
            }
            requestSearch(QString("is:expired-%1").arg(expirationOffset));
        }

        m_groupBeforeLock = QUuid();
        m_entryBeforeLock = QUuid();
        m_saveAttempts = 0;
        emit databaseUnlocked();
#ifdef WITH_XC_SSHAGENT
        sshAgent()->databaseUnlocked(m_db);
#endif
        if (config()->get(Config::MinimizeAfterUnlock).toBool()) {
            getMainWindow()->minimizeOrHide();
        }
    } else {
        if (m_databaseOpenWidget->database()) {
            m_databaseOpenWidget->database().reset();
        }
        emit closeRequest();
    }
}

void DatabaseWidget::mergeDatabase(bool accepted)
{
    if (accepted) {
        if (!m_db) {
            showMessage(tr("No current database."), MessageWidget::Error);
            return;
        }

        auto* senderDialog = qobject_cast<DatabaseOpenDialog*>(sender());

        Q_ASSERT(senderDialog);
        if (!senderDialog) {
            return;
        }
        auto srcDb = senderDialog->database();

        if (!srcDb) {
            showMessage(tr("No source database, nothing to do."), MessageWidget::Error);
            return;
        }

        Merger merger(srcDb.data(), m_db.data());
        QStringList changeList = merger.merge();

        if (!changeList.isEmpty()) {
            showMessage(tr("Successfully merged the database files."), MessageWidget::Information);
        } else {
            showMessage(tr("Database was not modified by merge operation."), MessageWidget::Information);
        }
    }

    switchToMainView();
    emit databaseMerged(m_db);
}

void DatabaseWidget::syncUnlockedDatabase(bool accepted)
{
    if (accepted) {
        if (!m_db) {
            showMessage(tr("No current database."), MessageWidget::Error);
            return;
        }

        auto* senderDialog = qobject_cast<DatabaseOpenDialog*>(sender());

        Q_ASSERT(senderDialog);
        if (!senderDialog) {
            return;
        }
        auto destinationDb = senderDialog->database();

        if (!destinationDb) {
            showMessage(tr("No source database, nothing to do."), MessageWidget::Error);
            return;
        }

        RemoteHandler::RemoteResult result;
        QString error;
        result.success = syncWithDatabase(destinationDb, error);
        result.errorMessage = error;
        result.filePath = destinationDb->filePath();

        emit databaseSyncUnlocked(result);
    }
    switchToMainView();
}

bool DatabaseWidget::syncWithDatabase(const QSharedPointer<Database>& otherDb, QString& error)
{
    emit updateSyncProgress(50, tr("Syncing..."));
    Merger firstMerge(m_db.data(), otherDb.data());
    Merger secondMerge(otherDb.data(), m_db.data());
    QStringList changeList = firstMerge.merge() + secondMerge.merge();

    if (!changeList.isEmpty()) {
        // Save synced databases
        if (!m_db->save(Database::Atomic, {}, &error)) {
            error = tr("Error while saving database %1: %2").arg(m_db->filePath(), error);
            return false;
        }
        if (!otherDb->save(Database::Atomic, {}, &error)) {
            error = tr("Error while saving database %1: %2").arg(otherDb->filePath(), error);
            return false;
        }
    }
    return true;
}

/**
 * Unlock the database.
 *
 * @param accepted true if the unlock dialog or widget was confirmed with OK
 */
void DatabaseWidget::unlockDatabase(bool accepted)
{
    auto* senderDialog = qobject_cast<DatabaseOpenDialog*>(sender());

    if (!accepted) {
        if (!senderDialog && (!m_db || !m_db->isInitialized())) {
            emit closeRequest();
        }
        if (senderDialog && senderDialog->intent() == DatabaseOpenDialog::Intent::RemoteSync) {
            RemoteHandler::RemoteResult result;
            result.success = false;
            result.errorMessage = "Remote database unlock cancelled.";
            emit databaseSyncUnlockFailed(result);
        }
        return;
    }

    if (senderDialog) {
        if (senderDialog->intent() == DatabaseOpenDialog::Intent::Merge) {
            mergeDatabase(accepted);
            return;
        } else if (senderDialog->intent() == DatabaseOpenDialog::Intent::RemoteSync) {
            syncUnlockedDatabase(accepted);
            return;
        }
    }

    emit databaseAboutToUnlock();
    QSharedPointer<Database> db;
    if (senderDialog) {
        db = senderDialog->database();
    } else {
        db = m_databaseOpenWidget->database();
    }
    replaceDatabase(db);

    restoreGroupEntryFocus(m_groupBeforeLock, m_entryBeforeLock);
    m_groupBeforeLock = QUuid();
    m_entryBeforeLock = QUuid();

    switchToMainView();
    processAutoOpen();
    emit databaseUnlocked();

#ifdef WITH_XC_SSHAGENT
    sshAgent()->databaseUnlocked(m_db);
#endif

    if (config()->get(Config::MinimizeAfterUnlock).toBool()) {
        getMainWindow()->minimizeOrHide();
    }

    if (senderDialog && senderDialog->intent() == DatabaseOpenDialog::Intent::AutoType) {
        // Rather than starting AutoType directly for this database, signal the parent DatabaseTabWidget to
        // restart AutoType now that this database is unlocked, so that other open+unlocked databases
        // can be included in the search.
        emit requestGlobalAutoType(m_searchStringForAutoType);
    }
}

void DatabaseWidget::entryActivationSignalReceived(Entry* entry, EntryModel::ModelColumn column)
{
    Q_ASSERT(entry);
    if (!entry) {
        return;
    }

    // Implement 'copy-on-doubleclick' functionality for certain columns
    switch (column) {
    case EntryModel::Username:
        if (config()->get(Config::Security_EnableCopyOnDoubleClick).toBool()) {
            setClipboardTextAndMinimize(entry->resolveMultiplePlaceholders(entry->username()));
        } else {
            switchToEntryEdit(entry);
        }
        break;
    case EntryModel::Password:
        if (config()->get(Config::Security_EnableCopyOnDoubleClick).toBool()) {
            setClipboardTextAndMinimize(entry->resolveMultiplePlaceholders(entry->password()));
        } else {
            switchToEntryEdit(entry);
        }
        break;
    case EntryModel::Totp:
        if (entry->hasTotp()) {
            setClipboardTextAndMinimize(entry->totp());
        } else {
            setupTotp();
        }
        break;
    case EntryModel::ParentGroup:
        // Call this first to clear out of search mode, otherwise
        // the desired entry is not properly selected
        endSearch();
        m_groupView->setCurrentGroup(entry->group());
        m_entryView->setCurrentEntry(entry);
        break;
    // TODO: switch to 'Notes' tab in details view/pane
    // case EntryModel::Notes:
    //    break;
    // TODO: switch to 'Attachments' tab in details view/pane
    // case EntryModel::Attachments:
    //    break;
    case EntryModel::Url:
        if (!entry->url().isEmpty() && config()->get(Config::OpenURLOnDoubleClick).toBool()) {
            openUrlForEntry(entry);
            break;
        }
        // Note, order matters here. We want to fall into the default case.
        [[fallthrough]];
    default:
        switchToEntryEdit(entry);
    }
}

void DatabaseWidget::switchToDatabaseReports()
{
    if (currentMode() != Mode::ReportsMode) {
        m_reportsDialog->load(m_db);
        setCurrentWidget(m_reportsDialog);
    }
}

void DatabaseWidget::switchToDatabaseSettings()
{
    if (currentMode() != Mode::DatabaseSettingsMode) {
        m_databaseSettingDialog->load(m_db);
        setCurrentWidget(m_databaseSettingDialog);
    }
}

void DatabaseWidget::switchToOpenDatabase()
{
    if (currentWidget() != m_databaseOpenWidget || m_databaseOpenWidget->filename() != m_db->filePath()) {
        switchToOpenDatabase(m_db->filePath());
    }
}

void DatabaseWidget::switchToOpenDatabase(const QString& filePath)
{
    m_databaseOpenWidget->load(filePath);
    setCurrentWidget(m_databaseOpenWidget);
}

void DatabaseWidget::switchToOpenDatabase(const QString& filePath, const QString& password, const QString& keyFile)
{
    switchToOpenDatabase(filePath);
    m_databaseOpenWidget->enterKey(password, keyFile);
}

void DatabaseWidget::switchToEntryEdit()
{
    auto entry = m_entryView->currentEntry();
    if (!entry) {
        return;
    }

    switchToEntryEdit(entry, false);
}

void DatabaseWidget::switchToGroupEdit()
{
    auto group = m_groupView->currentGroup();
    if (!group) {
        return;
    }

    switchToGroupEdit(group, false);
}

void DatabaseWidget::sortGroupsAsc()
{
    m_groupView->sortGroups();
}

void DatabaseWidget::sortGroupsDesc()
{
    m_groupView->sortGroups(true);
}

void DatabaseWidget::switchToDatabaseSecurity()
{
    switchToDatabaseSettings();
    m_databaseSettingDialog->showDatabaseKeySettings();
}

void DatabaseWidget::switchToRemoteSettings()
{
    switchToDatabaseSettings();
    m_databaseSettingDialog->showRemoteSettings();
}

#ifdef WITH_XC_BROWSER_PASSKEYS
void DatabaseWidget::switchToPasskeys()
{
    switchToDatabaseReports();
    m_reportsDialog->activatePasskeysPage();
}

void DatabaseWidget::showImportPasskeyDialog(bool isEntry)
{
    PasskeyImporter passkeyImporter(this);

    if (isEntry) {
        auto currentEntry = currentSelectedEntry();
        if (!currentEntry) {
            return;
        }

        passkeyImporter.importPasskey(m_db, currentEntry);
    } else {
        passkeyImporter.importPasskey(m_db);
    }
}

void DatabaseWidget::removePasskeyFromEntry()
{
    auto currentEntry = currentSelectedEntry();
    if (!currentEntry) {
        return;
    }

    auto result = MessageBox::question(this,
                                       tr("Remove passkey from entry"),
                                       tr("Do you want to remove the passkey from this entry?"),
                                       MessageBox::Remove | MessageBox::Cancel);
    if (result == MessageBox::Remove) {
        currentEntry->removePasskey();
    }
}
#endif

void DatabaseWidget::performUnlockDatabase(const QString& password, const QString& keyfile)
{
    if (password.isEmpty() && keyfile.isEmpty()) {
        return;
    }

    if (!m_db->isInitialized() || isLocked()) {
        switchToOpenDatabase();
        m_databaseOpenWidget->enterKey(password, keyfile);
    }
}

void DatabaseWidget::refreshSearch()
{
    if (isSearchActive()) {
        auto selectedEntry = m_entryView->currentEntry();
        search(m_lastSearchText);
        // Re-select the previous entry if it is still in the search
        m_entryView->setCurrentEntry(selectedEntry);
    }
}

void DatabaseWidget::search(const QString& searchtext)
{
    if (searchtext.isEmpty()) {
        endSearch();
        return;
    }

    auto searchGroup = m_db->rootGroup();
    if (m_searchLimitGroup && m_nextSearchLabelText.isEmpty()) {
        searchGroup = currentGroup();
    }

    auto results = m_entrySearcher->search(searchtext, searchGroup);

    // Display a label detailing our search results
    if (!m_nextSearchLabelText.isEmpty()) {
        // Custom searches don't display if there are no results
        if (results.isEmpty()) {
            endSearch();
            return;
        }
        m_searchingLabel->setText(m_nextSearchLabelText);
        m_nextSearchLabelText.clear();
    } else if (!results.isEmpty()) {
        m_searchingLabel->setText(tr("Search Results (%1)").arg(results.size()));
    } else {
        m_searchingLabel->setText(tr("No Results"));
    }

    emit searchModeAboutToActivate();

    m_entryView->displaySearch(results);
    m_lastSearchText = searchtext;

    m_searchingLabel->setVisible(true);
#ifdef WITH_XC_KEESHARE
    m_shareLabel->setVisible(false);
#endif

    emit searchModeActivated();
}

void DatabaseWidget::saveSearch(const QString& searchtext)
{
    if (!m_db->isInitialized()) {
        return;
    }

    // Pull the existing searches and prepend an empty string to allow
    // the user to input a new search name without seeing the first one
    QStringList searches(m_db->metadata()->savedSearches().keys());
    searches.prepend("");

    QInputDialog dialog(this);
    connect(this, &DatabaseWidget::databaseLockRequested, &dialog, &QInputDialog::reject);

    dialog.setComboBoxEditable(true);
    dialog.setComboBoxItems(searches);
    dialog.setOkButtonText(tr("Save"));
    dialog.setLabelText(tr("Enter a unique name or overwrite an existing search from the list:"));
    dialog.setWindowTitle(tr("Save Search"));
    dialog.exec();

    auto name = dialog.textValue();
    if (!name.isEmpty()) {
        m_db->metadata()->addSavedSearch(name, searchtext);
    }
}

void DatabaseWidget::deleteSearch(const QString& name)
{
    if (m_db->isInitialized()) {
        m_db->metadata()->deleteSavedSearch(name);
    }
}

void DatabaseWidget::setSearchCaseSensitive(bool state)
{
    m_entrySearcher->setCaseSensitive(state);
    refreshSearch();
}

void DatabaseWidget::setSearchLimitGroup(bool state)
{
    m_searchLimitGroup = state;
    refreshSearch();
}

void DatabaseWidget::onGroupChanged()
{
    auto group = m_groupView->currentGroup();

    // Intercept group changes if in search mode
    if (isSearchActive() && m_searchLimitGroup) {
        search(m_lastSearchText);
    } else {
        endSearch();
        m_entryView->displayGroup(group);
    }

    m_previewView->setGroup(group);

#ifdef WITH_XC_KEESHARE
    auto shareLabel = KeeShare::sharingLabel(group);
    if (!shareLabel.isEmpty()) {
        m_shareLabel->setRawText(shareLabel);
        m_shareLabel->setVisible(true);
    } else {
        m_shareLabel->setVisible(false);
    }
#endif

    emit groupChanged();
}

void DatabaseWidget::onDatabaseModified()
{
    refreshSearch();
    m_remoteSettings->loadSettings();
    int autosaveDelayMs = m_db->metadata()->autosaveDelayMin() * 60 * 1000; // min to msec for QTimer
    bool autosaveAfterEveryChangeConfig = config()->get(Config::AutoSaveAfterEveryChange).toBool();
    if (autosaveDelayMs > 0 && autosaveAfterEveryChangeConfig) {
        // reset delay when modified
        m_autosaveTimer->start(autosaveDelayMs);
        return;
    }
    if (!m_blockAutoSave && autosaveAfterEveryChangeConfig) {
        save();
    } else {
        // Only block once, then reset
        m_blockAutoSave = false;
    }
}

void DatabaseWidget::onAutosaveDelayTimeout()
{
    const bool isAutosaveDelayEnabled = m_db->metadata()->autosaveDelayMin() > 0;
    const bool autosaveAfterEveryChangeConfig = config()->get(Config::AutoSaveAfterEveryChange).toBool();
    if (!(isAutosaveDelayEnabled && autosaveAfterEveryChangeConfig)) {
        // User might disable the delay/autosave while the timer is running
        return;
    }
    if (!m_blockAutoSave) {
        save();
    } else {
        // Only block once, then reset
        m_blockAutoSave = false;
    }
}

void DatabaseWidget::triggerAutosaveTimer()
{
    m_autosaveTimer->stop();
    QMetaObject::invokeMethod(m_autosaveTimer, "timeout");
}

void DatabaseWidget::onDatabaseNonDataChanged()
{
    // Force mark the database modified if we are not auto-saving non-data changes
    if (!config()->get(Config::AutoSaveNonDataChanges).toBool()) {
        m_db->markAsModified();
    }
}

QString DatabaseWidget::getCurrentSearch()
{
    return m_lastSearchText;
}

void DatabaseWidget::endSearch()
{
    if (isSearchActive()) {
        // Show the normal entry view of the current group
        emit listModeAboutToActivate();
        m_entryView->displayGroup(currentGroup());
        emit listModeActivated();
        m_entryView->setFirstEntryActive();
        // Enforce preview view update (prevents stale information if focus group is empty)
        m_previewView->setEntry(currentSelectedEntry());
        // Reset selection on tag view
        m_tagView->selectionModel()->clearSelection();
    }

    m_searchingLabel->setVisible(false);
    m_searchingLabel->setText(tr("Searching…"));

    m_lastSearchText.clear();
    m_nextSearchLabelText.clear();

    // Tell the search widget to clear
    emit clearSearch();
}

void DatabaseWidget::emitGroupContextMenuRequested(const QPoint& pos)
{
    emit groupContextMenuRequested(m_groupView->viewport()->mapToGlobal(pos));
}

void DatabaseWidget::emitEntryContextMenuRequested(const QPoint& pos)
{
    emit entryContextMenuRequested(m_entryView->viewport()->mapToGlobal(pos));
}

void DatabaseWidget::onEntryChanged(Entry* entry)
{
    if (entry) {
        m_previewView->setEntry(entry);
    } else {
        m_previewView->setGroup(groupView()->currentGroup());
    }

    emit entrySelectionChanged();
}

bool DatabaseWidget::canCloneCurrentGroup() const
{
    auto currentGroup = m_groupView->currentGroup();
    return currentGroup != m_db->rootGroup() && currentGroup != m_db->metadata()->recycleBin();
}

bool DatabaseWidget::canDeleteCurrentGroup() const
{
    return currentGroup() != m_db->rootGroup();
}

Group* DatabaseWidget::currentGroup() const
{
    return m_groupView->currentGroup();
}

void DatabaseWidget::closeEvent(QCloseEvent* event)
{
    if (!lock() || m_databaseOpenWidget->unlockingDatabase()) {
        event->ignore();
        return;
    }

    m_databaseOpenWidget->resetQuickUnlock();
    event->accept();
}

void DatabaseWidget::showEvent(QShowEvent* event)
{
    if (!m_db->isInitialized() || isLocked()) {
        switchToOpenDatabase();
    }

    event->accept();
}

bool DatabaseWidget::focusNextPrevChild(bool next)
{
    // [parent] <-> GroupView <-> TagView <-> EntryView <-> EntryPreview <-> [parent]
    QList<QWidget*> sequence = {m_groupView, m_tagView, m_entryView, m_previewView};
    auto widget = qApp->focusWidget();
    if (!widget) {
        return QStackedWidget::focusNextPrevChild(next);
    }

    // Find the nearest parent widget in the sequence list
    int idx;
    do {
        idx = sequence.indexOf(widget);
        widget = widget->parentWidget();
    } while (idx == -1 && widget);

    // Determine next/previous or wrap around
    if (idx == -1) {
        idx = next ? 0 : sequence.size() - 1;
    } else {
        idx = next ? idx + 1 : idx - 1;
    }

    // Find the next visible element in the sequence and set the focus
    while (idx >= 0 && idx < sequence.size()) {
        widget = sequence[idx];
        if (widget && widget->isVisible() && widget->isEnabled() && widget->height() > 0 && widget->width() > 0) {
            widget->setFocus();
            return widget;
        }
        idx = next ? idx + 1 : idx - 1;
    }

    // Ran out of options, defer to the parent widget
    return QStackedWidget::focusNextPrevChild(next);
}

bool DatabaseWidget::lock()
{
    if (isLocked() || m_attemptingLock) {
        return isLocked();
    }

    // ignore when reloading
    if (m_reloading) {
        return false;
    }

    // Don't try to lock the database while saving, this will cause a deadlock
    if (m_db->isSaving()) {
        QTimer::singleShot(200, this, SLOT(lock()));
        return false;
    }

    m_attemptingLock = true;

    emit databaseLockRequested();

    // Force close any modal widgets associated with this widget
    auto modalWidget = QApplication::activeModalWidget();
    if (modalWidget) {
        auto parent = modalWidget->parentWidget();
        while (parent) {
            if (parent == this) {
                modalWidget->close();
                break;
            }
            parent = parent->parentWidget();
        }
    }

    clipboard()->clearCopiedText();

    if (isEditWidgetModified()) {
        auto result = MessageBox::question(this,
                                           tr("Lock Database?"),
                                           tr("You are editing an entry. Discard changes and lock anyway?"),
                                           MessageBox::Discard | MessageBox::Cancel,
                                           MessageBox::Cancel);
        if (result == MessageBox::Cancel) {
            m_attemptingLock = false;
            return false;
        }
    }

    if (m_db->isModified()) {
        bool saved = false;
        // Attempt to save on exit, but don't block locking if it fails
        if (config()->get(Config::AutoSaveOnExit).toBool()
            || config()->get(Config::AutoSaveAfterEveryChange).toBool()) {
            saved = save();

            if (!saved) {
                // detect if a reload was triggered
                bool reloadTriggered = false;
                auto connection =
                    connect(this, &DatabaseWidget::reloadBegin, [&reloadTriggered] { reloadTriggered = true; });
                QApplication::processEvents();
                disconnect(connection);
                if (reloadTriggered) {
                    return false;
                }
            }
        }

        if (!saved) {
            QString msg;
            if (!m_db->metadata()->name().toHtmlEscaped().isEmpty()) {
                msg = tr("\"%1\" was modified.\nSave changes?").arg(m_db->metadata()->name().toHtmlEscaped());
            } else {
                msg = tr("Database was modified.\nSave changes?");
            }
            auto result = MessageBox::question(this,
                                               tr("Save changes?"),
                                               msg,
                                               MessageBox::Save | MessageBox::Discard | MessageBox::Cancel,
                                               MessageBox::Save);
            if (result == MessageBox::Save) {
                if (!save()) {
                    m_attemptingLock = false;
                    return false;
                }
            } else if (result == MessageBox::Cancel) {
                m_attemptingLock = false;
                return false;
            }
        }
    } else if (m_db->hasNonDataChanges() && config()->get(Config::AutoSaveNonDataChanges).toBool()) {
        // Silently auto-save non-data changes, ignore errors
        QString errorMessage;
        performSave(errorMessage);
    }

    if (m_groupView->currentGroup()) {
        m_groupBeforeLock = m_groupView->currentGroup()->uuid();
    } else {
        m_groupBeforeLock = m_db->rootGroup()->uuid();
    }

    auto currentEntry = currentSelectedEntry();
    if (currentEntry) {
        m_entryBeforeLock = currentEntry->uuid();
    }

#ifdef WITH_XC_SSHAGENT
    sshAgent()->databaseLocked(m_db);
#endif

    endSearch();
    clearAllWidgets();
    switchToOpenDatabase(m_db->filePath());

    auto newDb = QSharedPointer<Database>::create(m_db->filePath());
    replaceDatabase(newDb);

    m_attemptingLock = false;
    emit databaseLocked();

    return true;
}

void DatabaseWidget::reloadDatabaseFile(bool triggeredBySave)
{
    if (triggeredBySave) {
        // not a failed save attempt due to file locking
        m_saveAttempts = 0;
    }
    // Ignore reload if we are locked, saving, reloading, or currently editing an entry or group
    if (!m_db || isLocked() || isEntryEditActive() || isGroupEditActive() || isSaving() || m_reloading) {
        return;
    }

    m_blockAutoSave = true;
    m_reloading = true;

    emit reloadBegin();

    if (!triggeredBySave && !config()->get(Config::AutoReloadOnChange).toBool()) {
        // Ask if we want to reload the db
        auto result = MessageBox::question(
            this,
            tr("File has changed"),
            QString("%1.\n%2").arg(tr("The database file \"%1\" was modified externally").arg(displayFileName()),
                                   tr("Do you want to load the changes?")),
            MessageBox::Yes | MessageBox::No);

        if (result == MessageBox::No) {
            // Notify everyone the database does not match the file
            m_db->markAsModified();
            m_reloading = false;

            emit reloadEnd();
            return;
        }
    }

    // notify
    showMessage(tr("Reloading database..."), MessageWidget::Information, false, MessageWidget::DisableAutoHide);

    // Lock out interactions
    m_entryView->setDisabled(true);
    m_groupView->setDisabled(true);
    m_tagView->setDisabled(true);
    QApplication::processEvents();

    auto reloadFinish = [this](bool hideMsg = true) {
        // Return control
        m_entryView->setDisabled(false);
        m_groupView->setDisabled(false);
        m_tagView->setDisabled(false);

        m_reloading = false;

        if (hideMsg) {
            hideMessage();
        }

        emit reloadEnd();
    };
    auto reloadAbort = [this, reloadFinish] {
        // Mark db as modified since existing data may differ from file or file was deleted
        m_db->markAsModified();

        showMessage(tr("Reload aborted"), MessageWidget::Warning);
        reloadFinish(false);
    };
    auto reloadContinue = [this, reloadFinish](QSharedPointer<Database> db, bool merge) {
        if (merge) {
            // Merge the old database into the new one
            Merger merger(m_db.data(), db.data());
            merger.merge();
        }

        QUuid groupBeforeReload = m_db->rootGroup()->uuid();
        if (m_groupView && m_groupView->currentGroup()) {
            groupBeforeReload = m_groupView->currentGroup()->uuid();
        }

        QUuid entryBeforeReload;
        if (m_entryView && m_entryView->currentEntry()) {
            entryBeforeReload = m_entryView->currentEntry()->uuid();
        }

        replaceDatabase(db);
        processAutoOpen();
        restoreGroupEntryFocus(groupBeforeReload, entryBeforeReload);
        m_blockAutoSave = false;

        showMessage(tr("Reload OK"), MessageWidget::Positive);
        reloadFinish(false);
    };

    auto db = QSharedPointer<Database>::create(m_db->filePath());
    bool openResult = db->open(database()->key());

    // skip if the db is unchanged, or the db file is gone or for sure not a kp-db
    if (bool sameHash = db->fileBlockHash() == m_db->fileBlockHash() || db->fileBlockHash().isEmpty()) {
        if (!sameHash) {
            // db file gone or invalid so mark modified
            m_db->markAsModified();
        }
        m_blockAutoSave = false;
        reloadFinish();
        return;
    }

    bool merge = false;
    QString changesActionStr;
    if (triggeredBySave || m_db->isModified() || m_db->hasNonDataChanges()) {
        // Ask how to proceed
        auto prefix = triggeredBySave ? tr("Cannot save because the") : tr("The");
        auto result =
            MessageBox::question(this,
                                 tr("Reload database"),
                                 QString("%1 %2.\n%3\n\n%4.\n%5.\n%6.")
                                     .arg(prefix,
                                          tr("database file \"%1\" was modified externally").arg(displayFileName()),
                                          tr("How to proceed with your unsaved changes?"),
                                          tr("Merge all changes together"),
                                          tr("Discard your changes"),
                                          tr("Ignore the changes in the file on disk")),
                                 MessageBox::Merge | MessageBox::Discard | MessageBox::Ignore | MessageBox::Cancel,
                                 MessageBox::Merge);

        if (result == MessageBox::Cancel) {
            reloadAbort();
            return;
        } else if (result == MessageBox::Ignore) {
            m_db->setIgnoreFileChangesUntilSaved(true);
            m_blockAutoSave = false;
            reloadFinish();
            return;
        } else if (result == MessageBox::Merge) {
            changesActionStr = tr("merged");
            merge = true;
        } else {
            changesActionStr = tr("discarded");
        }
    }

    if (openResult) {
        reloadContinue(std::move(db), merge);
        return;
    }

    // the user needs to provide credentials
    auto dbWidget = new DatabaseWidget(std::move(db));
    auto openDialog = new DatabaseOpenDialog(this);
    connect(openDialog, &QObject::destroyed, [=](QObject*) { dbWidget->deleteLater(); });
    connect(openDialog, &DatabaseOpenDialog::dialogFinished, this, [=](bool accepted, DatabaseWidget*) {
        if (accepted) {
            reloadContinue(openDialog->database(), merge);
        } else {
            reloadAbort();
        }
    });
    openDialog->setAttribute(Qt::WA_DeleteOnClose); // free the memory on close
    openDialog->addDatabaseTab(dbWidget);
    openDialog->setActiveDatabaseTab(dbWidget);
    if (!changesActionStr.isEmpty()) {
        changesActionStr = QString("   [%1]").arg(tr("Changes are %1").arg(changesActionStr));
    }
    openDialog->showMessage(
        QString("%1.\n%2%3")
            .arg(tr("Error: failed to open the database file for reload"), tr("Unlock to reload"), changesActionStr),
        MessageWidget::Error,
        MessageWidget::DisableAutoHide);

    // ensure the main window is visible for this
    getMainWindow()->bringToFront();
    // show the unlock dialog
    openDialog->show();
    openDialog->raise();
    openDialog->activateWindow();
}

int DatabaseWidget::numberOfSelectedEntries() const
{
    return m_entryView->numberOfSelectedEntries();
}

int DatabaseWidget::currentEntryIndex() const
{
    return m_entryView->currentEntryIndex();
}

QStringList DatabaseWidget::customEntryAttributes() const
{
    Entry* entry = m_entryView->currentEntry();
    if (!entry) {
        return {};
    }

    return entry->attributes()->customKeys();
}

/*
 * Restores the focus on the group and entry provided
 */
void DatabaseWidget::restoreGroupEntryFocus(const QUuid& groupUuid, const QUuid& entryUuid)
{
    auto group = m_db->rootGroup()->findGroupByUuid(groupUuid);
    if (group) {
        m_groupView->setCurrentGroup(group);
        auto entry = group->findEntryByUuid(entryUuid, false);
        if (entry) {
            m_entryView->setCurrentEntry(entry);
        }
    }
}

bool DatabaseWidget::isGroupSelected() const
{
    return m_groupView->currentGroup();
}

bool DatabaseWidget::currentEntryHasTitle()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->title().isEmpty();
}

bool DatabaseWidget::currentEntryHasUsername()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->resolveMultiplePlaceholders(currentEntry->username()).isEmpty();
}

bool DatabaseWidget::currentEntryHasPassword()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->resolveMultiplePlaceholders(currentEntry->password()).isEmpty();
}

bool DatabaseWidget::currentEntryHasUrl()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->resolveMultiplePlaceholders(currentEntry->url()).isEmpty();
}

bool DatabaseWidget::currentEntryHasTotp()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return currentEntry->hasTotp();
}

#ifdef WITH_XC_SSHAGENT
bool DatabaseWidget::currentEntryHasSshKey()
{
    Entry* currentEntry = m_entryView->currentEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }

    return KeeAgentSettings::inEntryAttachments(currentEntry->attachments());
}
#endif

#ifdef WITH_XC_BROWSER_PASSKEYS
bool DatabaseWidget::currentEntryHasPasskey()
{
    auto currentEntry = m_entryView->currentEntry();
    return currentEntry && currentEntry->hasPasskey();
}
#endif

bool DatabaseWidget::currentEntryHasNotes()
{
    auto currentEntry = currentSelectedEntry();
    Q_ASSERT(currentEntry);
    if (!currentEntry) {
        return false;
    }
    return !currentEntry->resolveMultiplePlaceholders(currentEntry->notes()).isEmpty();
}

bool DatabaseWidget::currentEntryHasAutoTypeEnabled()
{
    auto currentEntry = currentSelectedEntry();
    if (!currentEntry) {
        return false;
    }

    return currentEntry->autoTypeEnabled() && currentEntry->groupAutoTypeEnabled();
}

GroupView* DatabaseWidget::groupView()
{
    return m_groupView;
}

EntryView* DatabaseWidget::entryView()
{
    return m_entryView;
}

/**
 * Save the database to disk.
 *
 * This method will try to save several times in case of failure and
 * ask to disable safe saves if it is unable to save after the third attempt.
 * Set `attempt` to -1 to disable this behavior.
 *
 * @return true on success
 */
bool DatabaseWidget::save()
{
    // Never allow saving a locked database; it causes corruption
    Q_ASSERT(!isLocked());
    // Release build interlock
    if (isLocked()) {
        // We return true since a save is not required
        return true;
    }

    // Do no try to save if the database is being reloaded
    if (m_reloading) {
        return false;
    }

    // Read-only and new databases ask for filename
    if (m_db->filePath().isEmpty()) {
        return saveAs();
    }

    // Prevent recursions and infinite save loops
    m_blockAutoSave = true;
    ++m_saveAttempts;

    QString errorMessage;
    if (performSave(errorMessage)) {
        m_saveAttempts = 0;
        m_blockAutoSave = false;
        m_autosaveTimer->stop(); // stop autosave delay to avoid triggering another save
        return true;
    }

    if (m_saveAttempts > 2 && config()->get(Config::UseAtomicSaves).toBool()) {
        // Saving failed 3 times, issue a warning and attempt to resolve
        auto result = MessageBox::question(this,
                                           tr("Disable safe saves?"),
                                           tr("KeePassXC has failed to save the database multiple times. "
                                              "This is likely caused by file sync services holding a lock on "
                                              "the save file.\nDisable safe saves and try again?"),
                                           MessageBox::Disable | MessageBox::Cancel,
                                           MessageBox::Disable);
        if (result == MessageBox::Disable) {
            config()->set(Config::UseAtomicSaves, false);
            return save();
        }
    }

    showMessage(tr("Writing the database failed: %1").arg(errorMessage),
                MessageWidget::Error,
                true,
                MessageWidget::LongAutoHideTimeout);

    return false;
}

/**
 * Save database under a new user-selected filename.
 *
 * @return true on success
 */
bool DatabaseWidget::saveAs()
{
    // Never allow saving a locked database; it causes corruption
    Q_ASSERT(!isLocked());
    // Release build interlock
    if (isLocked()) {
        // We return true since a save is not required
        return true;
    }

    // Do no try to save if the database is being reloaded
    if (m_reloading) {
        return false;
    }

    QString oldFilePath = m_db->filePath();
    if (!QFileInfo::exists(oldFilePath)) {
        QString defaultFileName = config()->get(Config::DefaultDatabaseFileName).toString();
        oldFilePath =
            QDir::toNativeSeparators(FileDialog::getLastDir("db") + "/"
                                     + (defaultFileName.isEmpty() ? tr("Passwords").append(".kdbx") : defaultFileName));
    }
    const QString newFilePath = fileDialog()->getSaveFileName(
        this, tr("Save database as"), oldFilePath, tr("KeePass 2 Database").append(" (*.kdbx)"));

    bool ok = false;
    if (!newFilePath.isEmpty()) {
        QString errorMessage;
        if (!performSave(errorMessage, newFilePath)) {
            showMessage(tr("Writing the database failed: %1").arg(errorMessage),
                        MessageWidget::Error,
                        true,
                        MessageWidget::LongAutoHideTimeout);
        }
    }

    return ok;
}

bool DatabaseWidget::performSave(QString& errorMessage, const QString& fileName)
{
    QPointer<QWidget> focusWidget(qApp->focusWidget());

    // Lock out interactions
    auto mainWindow = getMainWindow();
    if (mainWindow) {
        mainWindow->setDisabled(true);
    }
    QApplication::processEvents();

    Database::SaveAction saveAction = Database::Atomic;
    if (!config()->get(Config::UseAtomicSaves).toBool()) {
        if (config()->get(Config::UseDirectWriteSaves).toBool()) {
            saveAction = Database::DirectWrite;
        } else {
            saveAction = Database::TempFile;
        }
    }

    QString backupFilePath;
    if (config()->get(Config::BackupBeforeSave).toBool()) {
        backupFilePath = config()->get(Config::BackupFilePathPattern).toString();
        // Fall back to default
        if (backupFilePath.isEmpty()) {
            backupFilePath = config()->getDefault(Config::BackupFilePathPattern).toString();
        }

        QFileInfo dbFileInfo(m_db->filePath());
        backupFilePath = Tools::substituteBackupFilePath(backupFilePath, dbFileInfo.canonicalFilePath());
        if (!backupFilePath.isNull()) {
            // Note that we cannot guarantee that backupFilePath is actually a valid filename. QT currently provides
            // no function for this. Moreover, we don't check if backupFilePath is a file and not a directory.
            // If this isn't the case, just let the backup fail.
            if (QDir::isRelativePath(backupFilePath)) {
                backupFilePath = QDir::cleanPath(dbFileInfo.absolutePath() + QDir::separator() + backupFilePath);
            }
        }
    }

    bool ok;
    if (fileName.isEmpty()) {
        ok = m_db->save(saveAction, backupFilePath, &errorMessage);
    } else {
        ok = m_db->saveAs(fileName, saveAction, backupFilePath, &errorMessage);
    }

    // Return control
    if (mainWindow) {
        mainWindow->setDisabled(false);
    }

    if (focusWidget && focusWidget->isVisible()) {
        focusWidget->setFocus();
    }

    return ok;
}

/**
 * Save copy of database under a new user-selected filename.
 *
 * @return true on success
 */
bool DatabaseWidget::saveBackup()
{
    while (true) {
        QString oldFilePath = m_db->filePath();
        if (!QFileInfo::exists(oldFilePath)) {
            QString defaultFileName = config()->get(Config::DefaultDatabaseFileName).toString();
            oldFilePath = QDir::toNativeSeparators(
                FileDialog::getLastDir("db") + "/"
                + (defaultFileName.isEmpty() ? tr("Passwords").append(".kdbx") : defaultFileName));
        }

        const QString newFilePath = fileDialog()->getSaveFileName(this,
                                                                  tr("Save database backup"),
                                                                  FileDialog::getLastDir("backup", oldFilePath),
                                                                  tr("KeePass 2 Database").append(" (*.kdbx)"));

        if (!newFilePath.isEmpty()) {
            // Ensure we don't recurse back into this function
            m_db->setFilePath(newFilePath);
            m_saveAttempts = 0;

            bool modified = m_db->isModified();

            if (!save()) {
                // Failed to save, try again
                m_db->setFilePath(oldFilePath);
                continue;
            }

            m_db->setFilePath(oldFilePath);
            if (modified) {
                // Source database is marked as clean when copy is saved, even if source has unsaved changes
                m_db->markAsModified();
            }
            FileDialog::saveLastDir("backup", newFilePath, true);
            return true;
        }

        // Canceled file selection
        return false;
    }
}

void DatabaseWidget::showMessage(const QString& text,
                                 MessageWidget::MessageType type,
                                 bool showClosebutton,
                                 int autoHideTimeout)
{
    m_messageWidget->setCloseButtonVisible(showClosebutton);
    m_messageWidget->showMessage(text, type, autoHideTimeout);
}

void DatabaseWidget::showErrorMessage(const QString& errorMessage)
{
    showMessage(errorMessage, MessageWidget::MessageType::Error);
}

void DatabaseWidget::hideMessage()
{
    if (m_messageWidget->isVisible()) {
        m_messageWidget->animatedHide();
    }
}

bool DatabaseWidget::isRecycleBinSelected() const
{
    auto group = currentGroup();
    auto entry = currentSelectedEntry();
    return (group && group->isRecycled()) || (entry && entry->isRecycled());
}

void DatabaseWidget::emptyRecycleBin()
{
    if (!isRecycleBinSelected()) {
        return;
    }

    auto result =
        MessageBox::question(this,
                             tr("Empty recycle bin?"),
                             tr("Are you sure you want to permanently delete everything from your recycle bin?"),
                             MessageBox::Empty | MessageBox::Cancel,
                             MessageBox::Cancel);

    if (result == MessageBox::Empty) {
        m_db->emptyRecycleBin();
    }
}

void DatabaseWidget::processAutoOpen()
{
    Q_ASSERT(m_db);

    auto* autoopenGroup = m_db->rootGroup()->findGroupByPath("/AutoOpen");
    if (!autoopenGroup) {
        return;
    }

    for (const auto* entry : autoopenGroup->entries()) {
        if (entry->url().isEmpty() || (entry->password().isEmpty() && entry->username().isEmpty())) {
            continue;
        }

        // Support ifDevice advanced entry, a comma separated list of computer names
        // that control whether to perform AutoOpen on this entry or not. Can be
        // negated using '!'
        auto ifDevice = entry->attribute("IfDevice");
        if (!ifDevice.isEmpty()) {
            bool loadDb = false;
            auto hostName = QHostInfo::localHostName();
            for (auto& device : ifDevice.split(",")) {
                device = device.trimmed();
                if (device.startsWith("!")) {
                    if (device.mid(1).compare(hostName, Qt::CaseInsensitive) == 0) {
                        // Machine name matched an exclusion, don't load this database
                        loadDb = false;
                        break;
                    } else {
                        // Not matching an exclusion allows loading on all machines
                        loadDb = true;
                    }
                } else if (device.compare(hostName, Qt::CaseInsensitive) == 0) {
                    // Explicitly named for loading
                    loadDb = true;
                }
            }
            if (!loadDb) {
                continue;
            }
        }

        openDatabaseFromEntry(entry);
    }
}

void DatabaseWidget::openDatabaseFromEntry(const Entry* entry, bool inBackground)
{
    auto keyFile = entry->resolveMultiplePlaceholders(entry->username());
    auto password = entry->resolveMultiplePlaceholders(entry->password());
    auto databaseUrl = entry->resolveMultiplePlaceholders(entry->url());
    if (databaseUrl.startsWith("kdbx://")) {
        databaseUrl = databaseUrl.mid(7);
    }

    QFileInfo dbFileInfo;
    if (databaseUrl.startsWith("file://")) {
        QUrl url(databaseUrl);
        dbFileInfo.setFile(url.toLocalFile());
    } else {
        dbFileInfo.setFile(databaseUrl);
        if (dbFileInfo.isRelative()) {
            QFileInfo currentpath(m_db->filePath());
            dbFileInfo.setFile(currentpath.absoluteDir(), databaseUrl);
        }
    }

    if (!dbFileInfo.isFile()) {
        showErrorMessage(tr("Could not find database file: %1").arg(databaseUrl));
        return;
    }

    QFileInfo keyFileInfo;
    if (!keyFile.isEmpty()) {
        if (keyFile.startsWith("file://")) {
            QUrl keyfileUrl(keyFile);
            keyFileInfo.setFile(keyfileUrl.toLocalFile());
        } else {
            keyFileInfo.setFile(keyFile);
            if (keyFileInfo.isRelative()) {
                QFileInfo currentpath(m_db->filePath());
                keyFileInfo.setFile(currentpath.absoluteDir(), keyFile);
            }
        }
    }

    // Request to open the database file in the background with a password and keyfile
    emit requestOpenDatabase(dbFileInfo.canonicalFilePath(), inBackground, password, keyFileInfo.canonicalFilePath());
}
