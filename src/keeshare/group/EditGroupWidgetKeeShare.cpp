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

#include "EditGroupWidgetKeeShare.h"
#include "ui_EditGroupWidgetKeeShare.h"

#include "KeeShareSettings.h"
#include "core/Group.h"
#include "gui/FileDialog.h"
#include "keeshare/KeeShare.h"

#include <QDir>
#include <QStandardPaths>

EditGroupWidgetKeeShare::EditGroupWidgetKeeShare(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::EditGroupWidgetKeeShare())
{
    m_ui->setupUi(this);

    m_ui->messageWidget->hide();
    m_ui->messageWidget->setCloseButtonVisible(false);
    m_ui->messageWidget->setAutoHideTimeout(-1);
    m_ui->messageWidget->setAnimate(false);

    m_ui->passwordEdit->enablePasswordGenerator();

    connect(m_ui->passwordEdit, SIGNAL(textChanged(QString)), SLOT(selectPassword()));
    connect(m_ui->pathEdit, SIGNAL(editingFinished()), SLOT(selectPath()));
    connect(m_ui->pathSelectionButton, SIGNAL(pressed()), SLOT(launchPathSelectionDialog()));
    connect(m_ui->typeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(selectType()));
    connect(m_ui->recurseIntoGroupsCheckbox, SIGNAL(toggled(bool)), SLOT(recurseIntoGroupsToggled(bool)));
    connect(m_ui->clearButton, SIGNAL(clicked(bool)), SLOT(clearInputs()));

    connect(KeeShare::instance(), SIGNAL(activeChanged()), SLOT(updateSharingState()));

    const auto types = QList<KeeShareSettings::Type>()
                       << KeeShareSettings::Inactive << KeeShareSettings::ImportFrom << KeeShareSettings::ExportTo
                       << KeeShareSettings::SynchronizeWith;
    for (const auto& type : types) {
        QString name;
        switch (type) {
        case KeeShareSettings::Inactive:
            name = tr("Inactive");
            break;
        case KeeShareSettings::ImportFrom:
            name = tr("Import");
            break;
        case KeeShareSettings::ExportTo:
            name = tr("Export");
            break;
        case KeeShareSettings::SynchronizeWith:
            name = tr("Synchronize");
            break;
        }
        m_ui->typeComboBox->insertItem(type, name, static_cast<int>(type));
    }
}

EditGroupWidgetKeeShare::~EditGroupWidgetKeeShare()
{
}

void EditGroupWidgetKeeShare::setGroup(Group* temporaryGroup, QSharedPointer<Database> database)
{
    if (m_temporaryGroup) {
        m_temporaryGroup->disconnect(this);
    }

    m_database = database;
    m_temporaryGroup = temporaryGroup;

    if (m_temporaryGroup) {
        connect(m_temporaryGroup, &Group::modified, this, &EditGroupWidgetKeeShare::update);
    }

    update();
}

void EditGroupWidgetKeeShare::updateSharingState()
{
    // Only enable controls if we are active
    bool isEnabled = m_ui->typeComboBox->currentData().toInt() > KeeShareSettings::Inactive;
    m_ui->pathEdit->setEnabled(isEnabled);
    m_ui->pathSelectionButton->setEnabled(isEnabled);
    m_ui->passwordEdit->setEnabled(isEnabled);
    m_ui->recurseIntoGroupsCheckbox->setEnabled(isEnabled);

    if (!m_temporaryGroup || !isEnabled) {
        m_ui->messageWidget->hideMessage();
        return;
    }

    QStringList supportedExtensions;
    supportedExtensions << KeeShare::unsignedContainerFileType() << KeeShare::signedContainerFileType();

    // Custom message for active KeeShare reference
    const auto reference = KeeShare::referenceOf(m_temporaryGroup);
    if (!reference.path.isEmpty()) {
        bool supported = false;
        for (const auto& extension : supportedExtensions) {
            if (reference.path.endsWith(extension, Qt::CaseInsensitive)) {
                supported = true;
                break;
            }
        }
        if (!supported) {
            m_ui->messageWidget->showMessage(tr("Your KeePassXC version does not support sharing this container type.\n"
                                                "Supported extensions are: %1.")
                                                 .arg(supportedExtensions.join(", ")),
                                             MessageWidget::Warning);
            return;
        }

        const auto groups = m_database->rootGroup()->groupsRecursive(true);
        bool conflictExport = false;
        bool multipleImport = false;
        bool cycleImportExport = false;
        for (const auto* group : groups) {
            if (group->uuid() == m_temporaryGroup->uuid()) {
                continue;
            }
            const auto other = KeeShare::referenceOf(group);
            if (other.path != reference.path) {
                continue;
            }
            multipleImport |= other.isImporting() && reference.isImporting();
            conflictExport |= other.isExporting() && reference.isExporting();
            cycleImportExport |=
                (other.isImporting() && reference.isExporting()) || (other.isExporting() && reference.isImporting());
        }
        if (conflictExport) {
            m_ui->messageWidget->showMessage(tr("%1 is already being exported by this database.").arg(reference.path),
                                             MessageWidget::Error);
            return;
        }
        if (multipleImport) {
            m_ui->messageWidget->showMessage(tr("%1 is already being imported by this database.").arg(reference.path),
                                             MessageWidget::Warning);
            return;
        }
        if (cycleImportExport) {
            m_ui->messageWidget->showMessage(
                tr("%1 is being imported and exported by different groups in this database.").arg(reference.path),
                MessageWidget::Warning);
            return;
        }
    }

    // Standard message for state of KeeShare service
    const auto active = KeeShare::active();
    if (!active.in && !active.out) {
        m_ui->messageWidget->showMessage(
            tr("KeeShare is currently disabled. You can enable import/export in the application settings.",
               "KeeShare is a proper noun"),
            MessageWidget::Information);
    } else if (active.in && !active.out) {
        m_ui->messageWidget->showMessage(tr("Database export is currently disabled by application settings."),
                                         MessageWidget::Information);
    } else if (!active.in && active.out) {
        m_ui->messageWidget->showMessage(tr("Database import is currently disabled by application settings."),
                                         MessageWidget::Information);
    } else {
        m_ui->messageWidget->hideMessage();
    }
}

void EditGroupWidgetKeeShare::update()
{
    if (!m_temporaryGroup) {
        m_ui->passwordEdit->clear();
        m_ui->pathEdit->clear();
    } else {
        const auto reference = KeeShare::referenceOf(m_temporaryGroup);

        m_ui->typeComboBox->setCurrentIndex(reference.type);
        m_ui->passwordEdit->setText(reference.password);
        m_ui->pathEdit->setText(reference.path);
        m_ui->recurseIntoGroupsCheckbox->setChecked(reference.recurse);
    }

    updateSharingState();
}

void EditGroupWidgetKeeShare::clearInputs()
{
    if (m_temporaryGroup) {
        KeeShare::setReferenceTo(m_temporaryGroup, KeeShareSettings::Reference());
    }
    m_ui->passwordEdit->clear();
    m_ui->pathEdit->clear();
    m_ui->typeComboBox->setCurrentIndex(KeeShareSettings::Inactive);
    updateSharingState();
}

void EditGroupWidgetKeeShare::selectPath()
{
    if (!m_temporaryGroup) {
        return;
    }
    auto reference = KeeShare::referenceOf(m_temporaryGroup);
    reference.path = m_ui->pathEdit->text();
    KeeShare::setReferenceTo(m_temporaryGroup, reference);
}

void EditGroupWidgetKeeShare::launchPathSelectionDialog()
{
    if (!m_temporaryGroup) {
        return;
    }
    auto reference = KeeShare::referenceOf(m_temporaryGroup);
    QString defaultFiletype = KeeShare::unsignedContainerFileType();

    QStringList supportedExtensions;
    supportedExtensions << KeeShare::unsignedContainerFileType() << KeeShare::signedContainerFileType();

    QStringList knownFilters;
    knownFilters << QString("%1 (*.%2)").arg(tr("KeeShare container"), KeeShare::unsignedContainerFileType());
    knownFilters << QString("%1 (*.%2)").arg(tr("KeeShare signed container"), KeeShare::signedContainerFileType());
    knownFilters << QString("%1 (*)").arg("All files");

    const auto filters = knownFilters.join(";;");
    auto defaultDirPath = FileDialog::getLastDir("keeshare");
    auto filename = reference.path;
    if (filename.isEmpty()) {
        filename = m_temporaryGroup->name();
    }
    switch (reference.type) {
    case KeeShareSettings::ImportFrom:
        filename = fileDialog()->getOpenFileName(this, tr("Select import source"), defaultDirPath, filters);
        break;
    case KeeShareSettings::ExportTo:
        filename = fileDialog()->getSaveFileName(this, tr("Select export target"), defaultDirPath, filters);
        break;
    case KeeShareSettings::SynchronizeWith:
    case KeeShareSettings::Inactive:
        filename = fileDialog()->getSaveFileName(this, tr("Select import/export file"), defaultDirPath, filters);
        break;
    }

    if (filename.isEmpty()) {
        return;
    }
    bool validFilename = false;
    for (const auto& extension : supportedExtensions) {
        if (filename.endsWith(extension, Qt::CaseInsensitive)) {
            validFilename = true;
            break;
        }
    }
    if (!validFilename) {
        filename += (!filename.endsWith(".") ? "." : "") + defaultFiletype;
    }

    m_ui->pathEdit->setText(filename);
    selectPath();
    FileDialog::saveLastDir("keeshare", filename);

    updateSharingState();
}

void EditGroupWidgetKeeShare::selectPassword()
{
    if (!m_temporaryGroup) {
        return;
    }
    auto reference = KeeShare::referenceOf(m_temporaryGroup);
    reference.password = m_ui->passwordEdit->text();
    KeeShare::setReferenceTo(m_temporaryGroup, reference);
}

void EditGroupWidgetKeeShare::selectType()
{
    if (!m_temporaryGroup) {
        return;
    }
    auto reference = KeeShare::referenceOf(m_temporaryGroup);
    reference.type = static_cast<KeeShareSettings::Type>(m_ui->typeComboBox->currentData().toInt());
    KeeShare::setReferenceTo(m_temporaryGroup, reference);

    updateSharingState();
}

void EditGroupWidgetKeeShare::recurseIntoGroupsToggled(bool toggled)
{
    if (!m_temporaryGroup) {
        return;
    }
    auto reference = KeeShare::referenceOf(m_temporaryGroup);
    reference.recurse = toggled;
    KeeShare::setReferenceTo(m_temporaryGroup, reference);
}
