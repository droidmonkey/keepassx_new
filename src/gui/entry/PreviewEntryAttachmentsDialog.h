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

#pragma once

#include "core/Tools.h"

#include <QDialog>
#include <QPointer>

namespace Ui
{
    class EntryAttachmentsDialog;
}

class PreviewEntryAttachmentsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreviewEntryAttachmentsDialog(QWidget* parent = nullptr);
    ~PreviewEntryAttachmentsDialog() override;

    void setAttachment(const QString& name, const QByteArray& data);

signals:
    void openAttachment(const QString& name);
    void saveAttachment(const QString& name);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    Tools::MimeType attachmentType(const QByteArray& data) const;

    void update();
    void updateTextAttachment(const QByteArray& data);
    void updateImageAttachment(const QByteArray& data);
    void updatePdfAttachment(const QByteArray& data);

    QScopedPointer<Ui::EntryAttachmentsDialog> m_ui;

    QString m_name;
    QByteArray m_data;
    Tools::MimeType m_type{Tools::MimeType::Unknown};
};
