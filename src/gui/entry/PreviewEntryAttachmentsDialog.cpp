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

#include "PreviewEntryAttachmentsDialog.h"
#include "ui_EntryAttachmentsDialog.h"

#include <QDialogButtonBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QTextCursor>
#include <QtDebug>

PreviewEntryAttachmentsDialog::PreviewEntryAttachmentsDialog(QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::EntryAttachmentsDialog)
{
    m_ui->setupUi(this);

    setWindowTitle(tr("Preview entry attachment"));
    // Disable the help button in the title bar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // Set to read-only
    m_ui->titleEdit->setReadOnly(true);
    m_ui->attachmentTextEdit->setReadOnly(true);
    m_ui->errorLabel->setVisible(false);

    // Initialize dialog buttons
    m_ui->dialogButtons->setStandardButtons(QDialogButtonBox::Close | QDialogButtonBox::Open | QDialogButtonBox::Save);
    auto closeButton = m_ui->dialogButtons->button(QDialogButtonBox::Close);
    closeButton->setDefault(true);

    connect(m_ui->dialogButtons, SIGNAL(rejected()), this, SLOT(reject()));
    connect(m_ui->dialogButtons, &QDialogButtonBox::clicked, [this](QAbstractButton* button) {
        auto pressedButton = m_ui->dialogButtons->standardButton(button);
        if (pressedButton == QDialogButtonBox::Open) {
            emit openAttachment(m_name);
        } else if (pressedButton == QDialogButtonBox::Save) {
            emit saveAttachment(m_name);
        }
    });
}

PreviewEntryAttachmentsDialog::~PreviewEntryAttachmentsDialog() = default;

void PreviewEntryAttachmentsDialog::setAttachment(const QString& name, const QByteArray& data)
{
    m_name = name;
    m_ui->titleEdit->setText(m_name);

    m_type = attachmentType(data);
    m_data = data;

    update();
}

void PreviewEntryAttachmentsDialog::update()
{
    if (m_type == Tools::MimeType::Unknown) {
        updateTextAttachment(tr("No preview available").toUtf8());
    } else if (m_type == Tools::MimeType::Image) {
        updateImageAttachment(m_data);
    } else if (m_type == Tools::MimeType::PlainText) {
        updateTextAttachment(m_data);
    } else if (m_type == Tools::MimeType::Pdf) {
        updatePdfAttachment(m_data);
    }
}

void PreviewEntryAttachmentsDialog::updatePdfAttachment(const QByteArray& data)
{
    // To preview a PDF as an image, you need to install the qt5-image-formats-plugin-pdf
    updateImageAttachment(data);
}

void PreviewEntryAttachmentsDialog::updateTextAttachment(const QByteArray& data)
{
    m_ui->attachmentTextEdit->setPlainText(QString::fromUtf8(data));
}

void PreviewEntryAttachmentsDialog::updateImageAttachment(const QByteArray& data)
{
    QImage image{};
    if (!image.loadFromData(data)) {
        updateTextAttachment(tr("Image format not supported").toUtf8());
        return;
    }

    m_ui->attachmentTextEdit->clear();
    auto cursor = m_ui->attachmentTextEdit->textCursor();

    // Scale the image to the contents rect minus another set of margins to avoid scrollbars
    auto margins = m_ui->attachmentTextEdit->contentsMargins();
    auto size = m_ui->attachmentTextEdit->contentsRect().size();
    size.setWidth(size.width() - margins.left() - margins.right());
    size.setHeight(size.height() - margins.top() - margins.bottom());
    image = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    cursor.insertImage(image);
}

Tools::MimeType PreviewEntryAttachmentsDialog::attachmentType(const QByteArray& data) const
{
    QMimeDatabase mimeDb{};
    const auto mime = mimeDb.mimeTypeForData(data);

    return Tools::toMimeType(mime.name());
}

void PreviewEntryAttachmentsDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);

    update();
}
