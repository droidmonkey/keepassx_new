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

#include <memory>

#include <poppler-document.h>
#include <poppler-page-renderer.h>
#include <poppler-page.h>

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
    poppler::byte_array array{std::cbegin(data), std::cend(data)};

    auto doc = std::unique_ptr<poppler::document>(poppler::document::load_from_data(&array));
    if (!doc) {
        updateTextAttachment(tr("Failed to load the PDF").toUtf8());
        return;
    }

    // Locked PDF files are not supported
    if (doc->is_locked()) {
        updateTextAttachment(tr("Failed to read the PDF: The file is locked").toUtf8());
        return;
    }

    if (!doc->pages()) {
        updateTextAttachment(tr("Failed to read the PDF: No pages found").toUtf8());
        return;
    }

    // Preview the first page of the document.
    auto page = std::unique_ptr<poppler::page>(doc->create_page(0));
    if (!page) {
        updateTextAttachment(tr("Failed to read the PDF: Unable to create a page").toUtf8());
        return;
    }

    poppler::page_renderer renderer{};
    auto popplerImage = renderer.render_page(page.get());

    QImage image(reinterpret_cast<const uchar*>(popplerImage.const_data()),
                 popplerImage.width(),
                 popplerImage.height(),
                 popplerImage.bytes_per_row(),
                 QImage::Format_ARGB32);

    if (image.isNull()) {
        updateTextAttachment(tr("Failed to render the PDF page").toUtf8());
        return;
    }

    updateImageAttachment(image);
}

QSize PreviewEntryAttachmentsDialog::calcucateImageSize()
{
    // Scale the image to the contents rect minus another set of margins to avoid scrollbars
    auto margins = m_ui->attachmentTextEdit->contentsMargins();
    auto size = m_ui->attachmentTextEdit->contentsRect().size();
    size.setWidth(size.width() - margins.left() - margins.right());
    size.setHeight(size.height() - margins.top() - margins.bottom());

    return size;
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

    updateImageAttachment(image);
}

void PreviewEntryAttachmentsDialog::updateImageAttachment(const QImage& image)
{
    m_ui->attachmentTextEdit->clear();
    auto cursor = m_ui->attachmentTextEdit->textCursor();

    cursor.insertImage(image.scaled(calcucateImageSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
