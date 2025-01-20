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

#include "NewEntryAttachmentsDialog.h"
#include "core/EntryAttachments.h"
#include "ui_EntryAttachmentsDialog.h"

#include <QMessageBox>
#include <QPushButton>

NewEntryAttachmentsDialog::NewEntryAttachmentsDialog(QPointer<EntryAttachments> attachments, QWidget* parent)
    : QDialog(parent)
    , m_attachments(std::move(attachments))
    , m_ui(new Ui::EntryAttachmentsDialog)
{
    Q_ASSERT(m_attachments);

    m_ui->setupUi(this);

    setWindowTitle(tr("New entry attachment"));

    m_ui->dialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(m_ui->dialogButtons, SIGNAL(accepted()), this, SLOT(saveAttachment()));
    connect(m_ui->dialogButtons, SIGNAL(rejected()), this, SLOT(reject()));
    connect(m_ui->titleEdit, SIGNAL(textChanged(const QString&)), this, SLOT(fileNameTextChanged(const QString&)));

    fileNameTextChanged(m_ui->titleEdit->text());
}

NewEntryAttachmentsDialog::~NewEntryAttachmentsDialog() = default;

bool NewEntryAttachmentsDialog::validateFileName(const QString& fileName, QString& error) const
{
    if (fileName.isEmpty()) {
        error = tr("Attachment name cannot be empty");
        return false;
    }

    if (m_attachments->hasKey(fileName)) {
        error = tr("Attachment with the same name already exists");
        return false;
    }

    return true;
}

void NewEntryAttachmentsDialog::saveAttachment()
{
    auto fileName = m_ui->titleEdit->text();
    auto text = m_ui->attachmentTextEdit->toPlainText().toUtf8();

    QString error;
    if (!validateFileName(fileName, error)) {
        QMessageBox::warning(this, tr("Save attachment"), error);
        return;
    }

    m_attachments->set(fileName, text);

    accept();
}

void NewEntryAttachmentsDialog::fileNameTextChanged(const QString& fileName)
{
    QString error;
    bool valid = validateFileName(fileName, error);

    m_ui->errorLabel->setText(error);
    m_ui->errorLabel->setVisible(!valid);

    auto okButton = m_ui->dialogButtons->button(QDialogButtonBox::Ok);
    if (okButton) {
        okButton->setDisabled(!valid);
    }
}
