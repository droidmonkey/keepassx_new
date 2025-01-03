/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

#include "ImportWizardPageSelect.h"
#include "ui_ImportWizardPageSelect.h"

#include "ImportWizard.h"

#include "gui/DatabaseWidget.h"
#include "gui/FileDialog.h"
#include "gui/Icons.h"
#include "gui/MainWindow.h"

ImportWizardPageSelect::ImportWizardPageSelect(QWidget* parent)
    : QWizardPage(parent)
    , m_ui(new Ui::ImportWizardPageSelect())
{
    m_ui->setupUi(this);

    new QListWidgetItem(icons()->icon("csv"), tr("Comma Separated Values (.csv)"), m_ui->importTypeList);
    new QListWidgetItem(icons()->icon("onepassword"), tr("1Password Export (.1pux)"), m_ui->importTypeList);
    new QListWidgetItem(icons()->icon("onepassword"), tr("1Password Vault (.opvault)"), m_ui->importTypeList);
    new QListWidgetItem(icons()->icon("bitwarden"), tr("Bitwarden (.json)"), m_ui->importTypeList);
    new QListWidgetItem(icons()->icon("proton"), tr("Proton Pass (.json)"), m_ui->importTypeList);
    new QListWidgetItem(icons()->icon("web"), tr("Remote Database (.kdbx)"), m_ui->importTypeList);
    new QListWidgetItem(icons()->icon("object-locked"), tr("KeePass 1 Database (.kdb)"), m_ui->importTypeList);

    m_ui->importTypeList->item(0)->setData(Qt::UserRole, ImportWizard::IMPORT_CSV);
    m_ui->importTypeList->item(1)->setData(Qt::UserRole, ImportWizard::IMPORT_OPUX);
    m_ui->importTypeList->item(2)->setData(Qt::UserRole, ImportWizard::IMPORT_OPVAULT);
    m_ui->importTypeList->item(3)->setData(Qt::UserRole, ImportWizard::IMPORT_BITWARDEN);
    m_ui->importTypeList->item(4)->setData(Qt::UserRole, ImportWizard::IMPORT_PROTONPASS);
    m_ui->importTypeList->item(6)->setData(Qt::UserRole, ImportWizard::IMPORT_KEEPASS1);

    connect(m_ui->importTypeList, &QListWidget::currentItemChanged, this, &ImportWizardPageSelect::itemSelected);
    m_ui->importTypeList->setCurrentRow(0);

    connect(m_ui->importFileButton, &QAbstractButton::clicked, this, &ImportWizardPageSelect::chooseImportFile);
    connect(m_ui->keyFileButton, &QAbstractButton::clicked, this, &ImportWizardPageSelect::chooseKeyFile);
    connect(m_ui->existingDatabaseRadio, &QRadioButton::toggled, this, [this](bool state) {
        m_ui->existingDatabaseChoice->setEnabled(state);
    });

    updateDatabaseChoices();

    registerField("ImportType", this);
    registerField("ImportFile*", m_ui->importFileEdit);
    registerField("ImportInto", m_ui->importIntoLabel);
    registerField("ImportPassword", m_ui->passwordEdit, "text", "textChanged");
    registerField("ImportKeyFile", m_ui->keyFileEdit);
}

ImportWizardPageSelect::~ImportWizardPageSelect()
{
}

void ImportWizardPageSelect::initializePage()
{
    setField("ImportType", m_ui->importTypeList->currentItem()->data(Qt::UserRole).toInt());
    adjustSize();
}

bool ImportWizardPageSelect::validatePage()
{
    if (m_ui->existingDatabaseRadio->isChecked()) {
        if (m_ui->existingDatabaseChoice->currentIndex() == -1) {
            return false;
        }
        setField("ImportInto", m_ui->existingDatabaseChoice->currentData());
    } else {
        setField("ImportInto", {});
    }

    return true;
}

void ImportWizardPageSelect::itemSelected(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED(previous)

    if (!current) {
        setCredentialState(false);
        return;
    }

    m_ui->importFileEdit->clear();
    m_ui->passwordEdit->clear();
    m_ui->keyFileEdit->clear();

    auto type = current->data(Qt::UserRole).toInt();
    setField("ImportType", type);
    switch (type) {
    // Unencrypted types
    case ImportWizard::IMPORT_CSV:
    case ImportWizard::IMPORT_OPUX:
    case ImportWizard::IMPORT_PROTONPASS:
        setCredentialState(false);
        break;
    // Password may be required
    case ImportWizard::IMPORT_BITWARDEN:
    case ImportWizard::IMPORT_OPVAULT:
        setCredentialState(true);
        break;
    // Password and/or Key File may be required
    case ImportWizard::IMPORT_KEEPASS1:
        setCredentialState(true, true);
        break;
    default:
        Q_ASSERT(false);
    }
}

void ImportWizardPageSelect::updateDatabaseChoices() const
{
    m_ui->existingDatabaseChoice->clear();
    auto mainWindow = getMainWindow();
    if (mainWindow) {
        for (auto dbWidget : mainWindow->getOpenDatabases()) {
            // Remove all connections
            disconnect(dbWidget, nullptr, this, nullptr);

            // Skip over locked databases
            if (dbWidget->isLocked()) {
                continue;
            }

            connect(dbWidget, &DatabaseWidget::databaseLocked, this, &ImportWizardPageSelect::updateDatabaseChoices);
            connect(dbWidget, &DatabaseWidget::databaseModified, this, &ImportWizardPageSelect::updateDatabaseChoices);

            // Enable the selection of an existing database
            m_ui->existingDatabaseRadio->setEnabled(true);
            m_ui->existingDatabaseRadio->setToolTip("");

            // Add a separator between databases
            if (m_ui->existingDatabaseChoice->count() > 0) {
                m_ui->existingDatabaseChoice->insertSeparator(m_ui->existingDatabaseChoice->count());
            }

            // Add the root group as a special line item
            auto db = dbWidget->database();
            m_ui->existingDatabaseChoice->addItem(
                QString("%1 (%2)").arg(dbWidget->displayName(), db->rootGroup()->name()),
                QList<QVariant>() << db->uuid() << db->rootGroup()->uuid());

            if (dbWidget->isVisible()) {
                m_ui->existingDatabaseChoice->setCurrentIndex(m_ui->existingDatabaseChoice->count() - 1);
            }

            // Add remaining groups
            for (const auto& group : db->rootGroup()->groupsRecursive(false)) {
                if (!group->isRecycled()) {
                    auto path = group->hierarchy();
                    path.removeFirst();
                    m_ui->existingDatabaseChoice->addItem(QString("  / %1").arg(path.join(" / ")),
                                                          QList<QVariant>() << db->uuid() << group->uuid());
                }
            }
        }
    }

    if (m_ui->existingDatabaseChoice->count() == 0) {
        m_ui->existingDatabaseRadio->setEnabled(false);
        m_ui->newDatabaseRadio->setChecked(true);
    }
}

void ImportWizardPageSelect::chooseImportFile()
{
    QString file;
#ifndef Q_OS_MACOS
    // OPVault is a folder except on macOS
    if (field("ImportType").toInt() == ImportWizard::IMPORT_OPVAULT) {
        file = fileDialog()->getExistingDirectory(this, tr("Open OPVault"), QDir::homePath());
    } else {
#endif
        file = fileDialog()->getOpenFileName(this, tr("Select import file"), QDir::homePath(), importFileFilter());
#ifndef Q_OS_MACOS
    }
#endif

    if (!file.isEmpty()) {
        m_ui->importFileEdit->setText(file);
    }
}

void ImportWizardPageSelect::chooseKeyFile()
{
    auto filter = QString("%1 (*);;%2 (*.keyx; *.key)").arg(tr("All files"), tr("Key files"));
    auto file = fileDialog()->getOpenFileName(this, tr("Select key file"), QDir::homePath(), filter);
    if (!file.isEmpty()) {
        m_ui->keyFileEdit->setText(file);
    }
}

void ImportWizardPageSelect::setCredentialState(bool passwordEnabled, bool keyFileEnable)
{
    bool passwordStateChanged = m_ui->passwordLabel->isVisible() != passwordEnabled;
    m_ui->passwordLabel->setVisible(passwordEnabled);
    m_ui->passwordEdit->setVisible(passwordEnabled);

    bool keyFileStateChanged = m_ui->keyFileLabel->isVisible() != keyFileEnable;
    m_ui->keyFileLabel->setVisible(keyFileEnable);
    m_ui->keyFileEdit->setVisible(keyFileEnable);
    m_ui->keyFileButton->setVisible(keyFileEnable);

    // Workaround Qt bug where the wizard window is not updated when the internal layout changes
    if (window()) {
        int height = window()->height();
        if (passwordStateChanged) {
            auto diff = m_ui->passwordEdit->height() + m_ui->inputFields->layout()->spacing();
            height += passwordEnabled ? diff : -diff;
        }
        if (keyFileStateChanged) {
            auto diff = m_ui->keyFileEdit->height() + m_ui->inputFields->layout()->spacing();
            height += keyFileEnable ? diff : -diff;
        }
        window()->resize(window()->width(), height);
    }
}

QString ImportWizardPageSelect::importFileFilter()
{
    switch (field("ImportType").toInt()) {
    case ImportWizard::IMPORT_CSV:
        return QString("%1 (*.csv);;%2 (*)").arg(tr("Comma Separated Values"), tr("All files"));
    case ImportWizard::IMPORT_OPUX:
        return QString("%1 (*.1pux)").arg(tr("1Password Export"));
    case ImportWizard::IMPORT_BITWARDEN:
        return QString("%1 (*.json)").arg(tr("Bitwarden JSON Export"));
    case ImportWizard::IMPORT_PROTONPASS:
        return QString("%1 (*.json)").arg(tr("Proton Pass JSON Export"));
    case ImportWizard::IMPORT_OPVAULT:
        return QString("%1 (*.opvault)").arg(tr("1Password Vault"));
    case ImportWizard::IMPORT_KEEPASS1:
        return QString("%1 (*.kdb)").arg(tr("KeePass1 Database"));
    default:
        return {};
    }
}
