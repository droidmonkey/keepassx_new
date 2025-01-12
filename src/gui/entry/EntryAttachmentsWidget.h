/*
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
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

#ifndef ENTRYATTACHMENTSWIDGET_H
#define ENTRYATTACHMENTSWIDGET_H

#include <QPointer>
#include <QWidget>

namespace Ui
{
    class EntryAttachmentsWidget;
}

class QByteArray;
class EntryAttachments;
class EntryAttachmentsModel;

class EntryAttachmentsWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly NOTIFY readOnlyChanged)
    Q_PROPERTY(bool isButtonsVisible READ isButtonsVisible WRITE setButtonsVisible NOTIFY buttonsVisibleChanged)
public:
    explicit EntryAttachmentsWidget(QWidget* parent = nullptr);
    ~EntryAttachmentsWidget();

    const EntryAttachments* attachments() const;
    bool isReadOnly() const;
    bool isButtonsVisible() const;

public slots:
    void linkAttachments(EntryAttachments* attachments);
    void unlinkAttachments();
    void setReadOnly(bool readOnly);
    void setButtonsVisible(bool isButtonsVisible);

signals:
    void errorOccurred(const QString& error);
    void readOnlyChanged(bool readOnly);
    void buttonsVisibleChanged(bool isButtonsVisible);
    void widgetUpdated();

private slots:
    void insertAttachments();
    void newAttachments();
    void previewSelectedAttachment();
    void removeSelectedAttachments();
    void saveSelectedAttachments();
    void openAttachment(const QModelIndex& index);
    void openSelectedAttachments();
    void updateButtonsVisible();
    void updateButtonsEnabled();
    void attachmentModifiedExternally(const QString& key, const QString& filePath);

private:
    void updateSpacers();

    bool insertAttachments(const QStringList& fileNames, QString& errorMessage);

    QStringList confirmAttachmentSelection(const QStringList& filenames);

    bool eventFilter(QObject* watched, QEvent* event) override;

    QScopedPointer<Ui::EntryAttachmentsWidget> m_ui;
    QPointer<EntryAttachments> m_entryAttachments;
    QPointer<EntryAttachmentsModel> m_attachmentsModel;
    QStringList m_pendingChanges;
    bool m_readOnly;
    bool m_buttonsVisible;
};

#endif // ENTRYATTACHMENTSWIDGET_H
