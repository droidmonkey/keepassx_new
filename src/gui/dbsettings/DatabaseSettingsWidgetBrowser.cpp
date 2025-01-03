/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2018 Sami Vänttinen <sami.vanttinen@protonmail.com>
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

#include "DatabaseSettingsWidgetBrowser.h"
#include "ui_DatabaseSettingsWidgetBrowser.h"

#include <QProgressDialog>

#include "browser/BrowserService.h"
#include "browser/BrowserSettings.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "gui/MessageBox.h"

DatabaseSettingsWidgetBrowser::DatabaseSettingsWidgetBrowser(QWidget* parent)
    : DatabaseSettingsWidget(parent)
    , m_ui(new Ui::DatabaseSettingsWidgetBrowser())
    , m_customData(new CustomData(this))
    , m_customDataModel(new QStandardItemModel(this))
{
    m_ui->setupUi(this);
    m_ui->removeCustomDataButton->setEnabled(false);
    m_ui->customDataTable->setModel(m_customDataModel);

    settingsWarning();

    // clang-format off
    connect(m_ui->customDataTable->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(toggleRemoveButton(QItemSelection)));
    connect(m_ui->customDataTable, SIGNAL(doubleClicked(QModelIndex)), SLOT(editIndex(QModelIndex)));
    connect(m_customDataModel, SIGNAL(itemChanged(QStandardItem*)), SLOT(editFinished(QStandardItem*)));
    // clang-format on

    connect(m_ui->removeCustomDataButton, SIGNAL(clicked()), SLOT(removeSelectedKey()));
    connect(m_ui->convertToCustomData, SIGNAL(clicked()), this, SLOT(convertAttributesToCustomData()));
    connect(m_ui->convertToCustomData, SIGNAL(clicked()), this, SLOT(updateSharedKeyList()));
    connect(m_ui->removeSharedEncryptionKeys, SIGNAL(clicked()), this, SLOT(removeSharedEncryptionKeys()));
    connect(m_ui->removeSharedEncryptionKeys, SIGNAL(clicked()), this, SLOT(updateSharedKeyList()));
    connect(m_ui->removeStoredPermissions, SIGNAL(clicked()), this, SLOT(removeStoredPermissions()));
    connect(m_ui->refreshDatabaseID, SIGNAL(clicked()), this, SLOT(refreshDatabaseID()));
}

DatabaseSettingsWidgetBrowser::~DatabaseSettingsWidgetBrowser()
{
}

CustomData* DatabaseSettingsWidgetBrowser::customData() const
{
    // Returns the current database customData from metadata. Otherwise return an empty customData member.
    if (m_db) {
        return m_db->metadata()->customData();
    }
    return m_customData;
}

void DatabaseSettingsWidgetBrowser::initialize()
{
    updateModel();
    settingsWarning();
}

void DatabaseSettingsWidgetBrowser::uninitialize()
{
}

void DatabaseSettingsWidgetBrowser::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
}

bool DatabaseSettingsWidgetBrowser::save()
{
    return true;
}

void DatabaseSettingsWidgetBrowser::removeSelectedKey()
{
    if (MessageBox::Yes
        != MessageBox::question(this,
                                tr("Delete the selected key?"),
                                tr("Do you really want to delete the selected key?\n"
                                   "This may prevent connection to the browser plugin."),
                                MessageBox::Yes | MessageBox::Cancel,
                                MessageBox::Cancel)) {
        return;
    }

    const QItemSelectionModel* itemSelectionModel = m_ui->customDataTable->selectionModel();
    if (itemSelectionModel) {
        for (const QModelIndex& index : itemSelectionModel->selectedRows(0)) {
            QString key = index.data().toString();
            key.insert(0, CustomData::BrowserKeyPrefix);
            customData()->remove(key);
        }
        updateModel();
    }
}

void DatabaseSettingsWidgetBrowser::toggleRemoveButton(const QItemSelection& selected)
{
    m_ui->removeCustomDataButton->setEnabled(!selected.isEmpty());
}

void DatabaseSettingsWidgetBrowser::updateModel()
{
    m_customDataModel->clear();
    m_customDataModel->setHorizontalHeaderLabels({tr("Key"), tr("Value"), tr("Created")});

    for (const QString& key : customData()->keys()) {
        if (key.startsWith(CustomData::BrowserKeyPrefix)) {
            QString strippedKey = key;
            strippedKey.remove(CustomData::BrowserKeyPrefix);
            auto created = customData()->value(getKeyWithPrefix(CustomData::Created, strippedKey));
            auto createdItem = new QStandardItem(created);
            createdItem->setEditable(false);
            m_customDataModel->appendRow(QList<QStandardItem*>()
                                         << new QStandardItem(strippedKey)
                                         << new QStandardItem(customData()->value(key)) << createdItem);
        }
    }

    m_ui->removeCustomDataButton->setEnabled(false);
}

void DatabaseSettingsWidgetBrowser::settingsWarning()
{
    if (!browserSettings()->isEnabled()) {
        m_ui->convertToCustomData->setEnabled(false);
        m_ui->removeSharedEncryptionKeys->setEnabled(false);
        m_ui->removeStoredPermissions->setEnabled(false);
        m_ui->customDataTable->setEnabled(false);
        m_ui->warningWidget->showMessage(tr("Enable Browser Integration to access these settings."),
                                         MessageWidget::Warning);
        m_ui->warningWidget->setCloseButtonVisible(false);
        m_ui->warningWidget->setAutoHideTimeout(-1);
    } else {
        m_ui->convertToCustomData->setEnabled(true);
        m_ui->removeSharedEncryptionKeys->setEnabled(true);
        m_ui->removeStoredPermissions->setEnabled(true);
        m_ui->customDataTable->setEnabled(true);
        m_ui->warningWidget->hideMessage();
    }
}

void DatabaseSettingsWidgetBrowser::removeSharedEncryptionKeys()
{
    if (MessageBox::Yes
        != MessageBox::question(this,
                                tr("Disconnect all browsers"),
                                tr("Do you really want to disconnect all browsers?\n"
                                   "This may prevent connection to the browser plugin."),
                                MessageBox::Yes | MessageBox::Cancel,
                                MessageBox::Cancel)) {
        return;
    }

    QStringList keysToRemove;
    for (const QString& key : m_db->metadata()->customData()->keys()) {
        if (key.startsWith(CustomData::BrowserKeyPrefix)) {
            keysToRemove << key;
        }
    }

    if (keysToRemove.isEmpty()) {
        MessageBox::information(
            this, tr("No keys found"), tr("No shared encryption keys found in KeePassXC settings."), MessageBox::Ok);
        return;
    }

    for (const QString& key : keysToRemove) {
        m_db->metadata()->customData()->remove(key);
    }

    const int count = keysToRemove.count();
    MessageBox::information(this,
                            tr("Removed keys from database"),
                            tr("Successfully removed %n encryption key(s) from KeePassXC settings.", "", count),
                            MessageBox::Ok);
}

void DatabaseSettingsWidgetBrowser::removeStoredPermissions()
{
    if (MessageBox::Yes
        != MessageBox::question(this,
                                tr("Forget all site-specific settings on entries"),
                                tr("Do you really want forget all site-specific settings on every entry?\n"
                                   "Permissions to access entries will be revoked."),
                                MessageBox::Yes | MessageBox::Cancel,
                                MessageBox::Cancel)) {
        return;
    }

    QList<Entry*> entries = m_db->rootGroup()->entriesRecursive();

    QProgressDialog progress(tr("Removing stored permissions…"), tr("Abort"), 0, entries.count());
    progress.setWindowModality(Qt::WindowModal);

    uint counter = 0;
    for (Entry* entry : entries) {
        if (progress.wasCanceled()) {
            return;
        }

        if (entry->customData()->contains(BrowserService::KEEPASSXCBROWSER_NAME)) {
            browserService()->removePluginData(entry);
            ++counter;
        }
        progress.setValue(progress.value() + 1);
    }
    progress.reset();

    if (counter > 0) {
        MessageBox::information(this,
                                tr("Removed permissions"),
                                tr("Successfully removed permissions from %n entry(s).", "", counter),
                                MessageBox::Ok);
    } else {
        MessageBox::information(this,
                                tr("No entry with permissions found!"),
                                tr("The active database does not contain an entry with permissions."),
                                MessageBox::Ok);
    }
}

void DatabaseSettingsWidgetBrowser::convertAttributesToCustomData()
{
    if (MessageBox::Yes
        != MessageBox::question(
            this,
            tr("Move KeePassHTTP attributes to custom data"),
            tr("Do you really want to convert all legacy browser integration data to the latest standard?\n"
               "This is necessary to maintain compatibility with the browser plugin."),
            MessageBox::Yes | MessageBox::Cancel,
            MessageBox::Cancel)) {
        return;
    }

    BrowserService::convertAttributesToCustomData(m_db);
}

void DatabaseSettingsWidgetBrowser::refreshDatabaseID()
{
    if (MessageBox::Yes
        != MessageBox::question(this,
                                tr("Refresh database ID"),
                                tr("Do you really want refresh the database ID?\n"
                                   "This is only necessary if your database is a copy of another and the "
                                   "browser extension cannot connect."),
                                MessageBox::Yes | MessageBox::Cancel,
                                MessageBox::Cancel)) {
        return;
    }

    m_db->rootGroup()->setUuid(QUuid::createUuid());
}

void DatabaseSettingsWidgetBrowser::editIndex(const QModelIndex& index)
{
    Q_ASSERT(index.isValid());
    if (!index.isValid()) {
        return;
    }

    m_valueInEdit = index.data().toString();
    m_ui->customDataTable->edit(index);
}

void DatabaseSettingsWidgetBrowser::editFinished(QStandardItem* item)
{
    const QItemSelectionModel* itemSelectionModel = m_ui->customDataTable->selectionModel();

    if (itemSelectionModel) {
        auto indexList = itemSelectionModel->selectedRows(item->column());
        if (!indexList.isEmpty()) {
            auto newValue = item->index().data().toString();

            // The key is edited
            if (item->column() == 0) {
                // Update created timestamp with the new key
                replaceKey(CustomData::Created, m_valueInEdit, newValue);

                // Get the old key/value pair, remove it and replace it
                replaceKey(CustomData::BrowserKeyPrefix, m_valueInEdit, newValue);
            } else {
                // Replace just the value
                for (const QString& key : m_db->metadata()->customData()->keys()) {
                    if (key.startsWith(CustomData::BrowserKeyPrefix)) {
                        if (m_valueInEdit == m_db->metadata()->customData()->value(key)) {
                            m_db->metadata()->customData()->set(key, newValue);
                            break;
                        }
                    }
                }
            }

            updateModel();
        }
    }
}

// Updates the shared key list after the list is cleared
void DatabaseSettingsWidgetBrowser::updateSharedKeyList()
{
    updateModel();
}

// Replaces a key and the created timestamp for it
void DatabaseSettingsWidgetBrowser::replaceKey(const QString& prefix,
                                               const QString& oldName,
                                               const QString& newName) const
{
    const auto oldKey = getKeyWithPrefix(prefix, oldName);
    const auto newKey = getKeyWithPrefix(prefix, newName);
    const auto tempValue = customData()->value(oldKey);
    m_db->metadata()->customData()->remove(oldKey);
    m_db->metadata()->customData()->set(newKey, tempValue);
}

QString DatabaseSettingsWidgetBrowser::getKeyWithPrefix(const QString& prefix, const QString& key) const
{
    return QString("%1%2").arg(prefix, key);
}
