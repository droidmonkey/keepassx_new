/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#include "DatabaseSettingsWidgetRemote.h"
#include "ui_DatabaseSettingsWidgetRemote.h"

#include "core/Global.h"
#include "core/Metadata.h"

#include "RemoteHandler.h"
#include "RemoteSettings.h"
#include "gui/MessageBox.h"

#include <QFile>

DatabaseSettingsWidgetRemote::DatabaseSettingsWidgetRemote(QWidget* parent)
    : DatabaseSettingsWidget(parent)
    , m_remoteSettings(new RemoteSettings(nullptr, this))
    , m_ui(new Ui::DatabaseSettingsWidgetRemote())
{
    m_ui->setupUi(this);
    m_ui->messageWidget->setHidden(true);

    connect(m_ui->saveSettingsButton, &QPushButton::clicked, this, &DatabaseSettingsWidgetRemote::saveCurrentSettings);
    connect(
        m_ui->removeSettingsButton, &QPushButton::clicked, this, &DatabaseSettingsWidgetRemote::removeCurrentSettings);
    connect(m_ui->settingsListWidget,
            &QListWidget::itemSelectionChanged,
            this,
            &DatabaseSettingsWidgetRemote::editCurrentSettings);
    connect(m_ui->testDownloadCommandButton, &QPushButton::clicked, this, &DatabaseSettingsWidgetRemote::testDownload);

    auto setModified = [this]() { m_modified = true; };
    connect(m_ui->nameLineEdit, &QLineEdit::textChanged, setModified);
    connect(m_ui->downloadCommand, &QLineEdit::textChanged, setModified);
    connect(m_ui->inputForDownload, &QPlainTextEdit::textChanged, setModified);
    connect(m_ui->downloadTimeoutSec, QOverload<int>::of(&QSpinBox::valueChanged), setModified);
    connect(m_ui->uploadCommand, &QLineEdit::textChanged, setModified);
    connect(m_ui->inputForUpload, &QPlainTextEdit::textChanged, setModified);
    connect(m_ui->uploadTimeoutSec, QOverload<int>::of(&QSpinBox::valueChanged), setModified);
}

DatabaseSettingsWidgetRemote::~DatabaseSettingsWidgetRemote() = default;

void DatabaseSettingsWidgetRemote::initialize()
{
    clearFields();
    m_remoteSettings->setDatabase(m_db);
    updateSettingsList();
    if (m_ui->settingsListWidget->count() > 0) {
        m_ui->settingsListWidget->setCurrentRow(0);
        m_ui->removeSettingsButton->setEnabled(true);
    } else {
        m_ui->removeSettingsButton->setDisabled(true);
    }
}

void DatabaseSettingsWidgetRemote::uninitialize()
{
}

bool DatabaseSettingsWidgetRemote::saveSettings()
{
    if (m_modified) {
        auto ans = MessageBox::question(this,
                                        tr("Save Remote Settings"),
                                        tr("You have unsaved changes. Do you want to save them?"),
                                        MessageBox::Save | MessageBox::Discard | MessageBox::Cancel,
                                        MessageBox::Save);
        if (ans == MessageBox::Save) {
            saveCurrentSettings();
        } else if (ans == MessageBox::Cancel) {
            return false;
        }
    }

    m_remoteSettings->saveSettings();
    return true;
}

void DatabaseSettingsWidgetRemote::saveCurrentSettings()
{
    QString name = m_ui->nameLineEdit->text();
    if (name.isEmpty()) {
        m_ui->messageWidget->showMessage(tr("Name cannot be empty."), MessageWidget::Warning);
        return;
    }

    auto* params = new RemoteParams();
    params->name = m_ui->nameLineEdit->text();
    params->downloadCommand = m_ui->downloadCommand->text();
    params->downloadInput = m_ui->inputForDownload->toPlainText();
    params->downloadTimeoutMsec = m_ui->downloadTimeoutSec->value() * 1000;
    params->uploadCommand = m_ui->uploadCommand->text();
    params->uploadInput = m_ui->inputForUpload->toPlainText();
    params->uploadTimeoutMsec = m_ui->uploadTimeoutSec->value() * 1000;

    m_remoteSettings->addRemoteParams(params);
    updateSettingsList();

    auto item = findItemByName(name);
    m_ui->settingsListWidget->setCurrentItem(item);
    m_ui->removeSettingsButton->setEnabled(true);
    m_modified = false;
}

QListWidgetItem* DatabaseSettingsWidgetRemote::findItemByName(const QString& name)
{
    return m_ui->settingsListWidget->findItems(name, Qt::MatchExactly).first();
}

void DatabaseSettingsWidgetRemote::removeCurrentSettings()
{
    m_remoteSettings->removeRemoteParams(m_ui->nameLineEdit->text());
    updateSettingsList();
    if (!m_remoteSettings->getAllRemoteParams().empty()) {
        m_ui->settingsListWidget->setCurrentRow(0);
        m_ui->removeSettingsButton->setEnabled(true);
    } else {
        clearFields();
        m_ui->removeSettingsButton->setDisabled(true);
    }
}

void DatabaseSettingsWidgetRemote::editCurrentSettings()
{
    if (!m_ui->settingsListWidget->currentItem()) {
        return;
    }

    QString name = m_ui->settingsListWidget->currentItem()->text();
    auto* params = m_remoteSettings->getRemoteParams(name);
    if (!params) {
        return;
    }

    m_ui->nameLineEdit->setText(params->name);
    m_ui->downloadCommand->setText(params->downloadCommand);
    m_ui->inputForDownload->setPlainText(params->downloadInput);
    m_ui->downloadTimeoutSec->setValue(params->downloadTimeoutMsec / 1000);
    m_ui->uploadCommand->setText(params->uploadCommand);
    m_ui->inputForUpload->setPlainText(params->uploadInput);
    m_ui->uploadTimeoutSec->setValue(params->uploadTimeoutMsec / 1000);
    m_modified = false;
}

void DatabaseSettingsWidgetRemote::updateSettingsList()
{
    m_ui->settingsListWidget->clear();
    for (auto params : m_remoteSettings->getAllRemoteParams()) {
        auto* item = new QListWidgetItem(m_ui->settingsListWidget);
        item->setText(params->name);
        m_ui->settingsListWidget->addItem(item);
    }
}

void DatabaseSettingsWidgetRemote::clearFields()
{
    m_ui->nameLineEdit->setText("");
    m_ui->downloadCommand->setText("");
    m_ui->inputForDownload->setPlainText("");
    m_ui->downloadTimeoutSec->setValue(10);
    m_ui->uploadCommand->setText("");
    m_ui->inputForUpload->setPlainText("");
    m_ui->uploadTimeoutSec->setValue(10);
    m_modified = false;
}

void DatabaseSettingsWidgetRemote::testDownload()
{
    auto* params = new RemoteParams();
    params->name = m_ui->nameLineEdit->text();
    params->downloadCommand = m_ui->downloadCommand->text();
    params->downloadInput = m_ui->inputForDownload->toPlainText();
    params->downloadTimeoutMsec = m_ui->downloadTimeoutSec->value() * 1000;

    QScopedPointer<RemoteHandler> remoteHandler(new RemoteHandler(this));
    if (params->downloadCommand.isEmpty()) {
        m_ui->messageWidget->showMessage(tr("Download command cannot be empty."), MessageWidget::Warning);
        return;
    }

    RemoteHandler::RemoteResult result = remoteHandler->download(params);
    if (!result.success) {
        m_ui->messageWidget->showMessage(tr("Download failed with error: %1").arg(result.errorMessage),
                                         MessageWidget::Error);
        return;
    }

    if (!QFile::exists(result.filePath)) {
        m_ui->messageWidget->showMessage(tr("Download finished, but file %1 could not be found.").arg(result.filePath),
                                         MessageWidget::Error);
        return;
    }

    m_ui->messageWidget->showMessage(tr("Download successful."), MessageWidget::Positive);
}
