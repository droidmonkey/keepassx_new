/*
 *  Copyright (C) 2025 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_ENTRYATTACHMENTS_H
#define KEEPASSX_ENTRYATTACHMENTS_H

#include "core/FileWatcher.h"
#include "core/ModifiableObject.h"

#include <QHash>
#include <QMap>
#include <QObject>
#include <QSharedPointer>
#include <QStringList>

class EntryAttachments : public ModifiableObject
{
    Q_OBJECT

public:
    explicit EntryAttachments(QObject* parent = nullptr);
    ~EntryAttachments() override;
    QStringList keys() const;
    bool hasKey(const QString& key) const;
    QSet<QByteArray> values() const;
    QByteArray value(const QString& key) const;
    void set(const QString& key, const QByteArray& value);
    void remove(const QString& key);
    void remove(const QStringList& keys);
    void rename(const QString& key, const QString& newKey);
    bool isEmpty() const;
    void clear();
    void copyDataFrom(const EntryAttachments* other);
    bool operator==(const EntryAttachments& other) const;
    bool operator!=(const EntryAttachments& other) const;
    int attachmentsSize() const;
    bool openAttachment(const QString& key, QString* errorMessage = nullptr);

signals:
    void keyModified(const QString& key);
    void valueModifiedExternally(const QString& key, const QString& path);
    void aboutToBeAdded(const QString& key);
    void added(const QString& key);
    void aboutToBeRemoved(const QString& key);
    void removed(const QString& key);
    void aboutToBeReset();
    void reset();

private slots:
    void attachmentFileModified(const QString& path);

private:
    void disconnectAndEraseExternalFile(const QString& path);

    QMap<QString, QByteArray> m_attachments;
    QHash<QString, QString> m_openedAttachments;
    QHash<QString, QString> m_openedAttachmentsInverse;
    QHash<QString, QSharedPointer<FileWatcher>> m_attachmentFileWatchers;
};

#endif // KEEPASSX_ENTRYATTACHMENTS_H
