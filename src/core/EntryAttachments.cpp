/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "EntryAttachments.h"

#include "config-keepassx.h"
#include "core/Global.h"
#include "core/Tools.h"
#include "crypto/Random.h"

#include <QDesktopServices>
#include <QDir>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QSet>
#include <QTemporaryFile>
#include <QUrl>

EntryAttachments::EntryAttachments(QObject* parent)
    : ModifiableObject(parent)
{
}

EntryAttachments::~EntryAttachments()
{
    clear();
}

QList<QString> EntryAttachments::keys() const
{
    return m_attachments.keys();
}

bool EntryAttachments::hasKey(const QString& key) const
{
    return m_attachments.contains(key);
}

QSet<QByteArray> EntryAttachments::values() const
{
    return Tools::asSet(m_attachments.values());
}

QByteArray EntryAttachments::value(const QString& key) const
{
    return m_attachments.value(key);
}

void EntryAttachments::set(const QString& key, const QByteArray& value)
{
    bool shouldEmitModified = false;
    bool addAttachment = !m_attachments.contains(key);

    if (addAttachment) {
        emit aboutToBeAdded(key);
    }

    if (addAttachment || m_attachments.value(key) != value) {
        m_attachments.insert(key, value);
        shouldEmitModified = true;
    }

    if (addAttachment) {
        emit added(key);
    } else {
        emit keyModified(key);
    }

    if (shouldEmitModified) {
        emitModified();
    }
}

void EntryAttachments::remove(const QString& key)
{
    if (!m_attachments.contains(key)) {
        Q_ASSERT_X(false, "EntryAttachments::remove", qPrintable(QString("Can't find attachment for key %1").arg(key)));
        return;
    }

    emit aboutToBeRemoved(key);

    m_attachments.remove(key);

    if (m_openedAttachments.contains(key)) {
        disconnectAndEraseExternalFile(m_openedAttachments.value(key));
    }

    emit removed(key);
    emitModified();
}

void EntryAttachments::remove(const QStringList& keys)
{
    if (keys.isEmpty()) {
        return;
    }

    bool emitStatus = modifiedSignalEnabled();
    setEmitModified(false);

    bool isModified = false;
    for (const QString& key : keys) {
        isModified |= m_attachments.contains(key);
        remove(key);
    }

    setEmitModified(emitStatus);

    if (isModified) {
        emitModified();
    }
}

void EntryAttachments::rename(const QString& key, const QString& newKey)
{
    const QByteArray val = value(key);
    remove(key);
    set(newKey, val);
}

bool EntryAttachments::isEmpty() const
{
    return m_attachments.isEmpty();
}

void EntryAttachments::clear()
{
    if (m_attachments.isEmpty()) {
        return;
    }

    emit aboutToBeReset();

    m_attachments.clear();

    const auto externalPath = m_openedAttachments.values();
    for (auto& path : externalPath) {
        disconnectAndEraseExternalFile(path);
    }

    emit reset();
    emitModified();
}

void EntryAttachments::disconnectAndEraseExternalFile(const QString& path)
{
    if (m_openedAttachmentsInverse.contains(path)) {
        m_attachmentFileWatchers.value(path)->stop();
        m_attachmentFileWatchers.remove(path);

        m_openedAttachments.remove(m_openedAttachmentsInverse.value(path));
        m_openedAttachmentsInverse.remove(path);
    }

    QFile f(path);
    if (f.open(QFile::ReadWrite)) {
        qint64 blocks = f.size() / 128 + 1;
        for (qint64 i = 0; i < blocks; ++i) {
            f.write(randomGen()->randomArray(128));
        }
        f.close();
    }
    f.remove();
}

void EntryAttachments::copyDataFrom(const EntryAttachments* other)
{
    if (*this != *other) {
        emit aboutToBeReset();

        // Reset all externally opened files
        const auto externalPath = m_openedAttachments.values();
        for (auto& path : externalPath) {
            disconnectAndEraseExternalFile(path);
        }

        m_attachments = other->m_attachments;

        emit reset();
        emitModified();
    }
}

bool EntryAttachments::operator==(const EntryAttachments& other) const
{
    return m_attachments == other.m_attachments;
}

bool EntryAttachments::operator!=(const EntryAttachments& other) const
{
    return m_attachments != other.m_attachments;
}

int EntryAttachments::attachmentsSize() const
{
    int size = 0;
    for (auto it = m_attachments.constBegin(); it != m_attachments.constEnd(); ++it) {
        size += it.key().toUtf8().size() + it.value().size();
    }
    return size;
}

bool EntryAttachments::openAttachment(const QString& key, QString* errorMessage)
{
    if (!m_openedAttachments.contains(key)) {
        const QByteArray attachmentData = value(key);
        auto ext = key.contains(".") ? "." + key.split(".").last() : "";

#if defined(KEEPASSXC_DIST_SNAP)
        const QString tmpFileTemplate =
            QString("%1/XXXXXXXXXXXX%2").arg(QProcessEnvironment::systemEnvironment().value("SNAP_USER_DATA"), ext);
#elif defined(KEEPASSXC_DIST_FLATPAK)
        const QString tmpFileTemplate =
            QString("%1/app/%2/XXXXXX.%3")
                .arg(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation), "org.keepassxc.KeePassXC", ext);
#else
        const QString tmpFileTemplate = QDir::temp().absoluteFilePath(QString("XXXXXXXXXXXX").append(ext));
#endif

        QTemporaryFile tmpFile(tmpFileTemplate);

        const bool saveOk = tmpFile.open() && tmpFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
                            && tmpFile.write(attachmentData) == attachmentData.size() && tmpFile.flush();

        if (!saveOk && errorMessage) {
            *errorMessage = QString("%1 - %2").arg(key, tmpFile.errorString());
            return false;
        }

        tmpFile.close();
        tmpFile.setAutoRemove(false);
        m_openedAttachments.insert(key, tmpFile.fileName());
        m_openedAttachmentsInverse.insert(tmpFile.fileName(), key);

        auto watcher = QSharedPointer<FileWatcher>::create();
        watcher->start(tmpFile.fileName(), 5);
        connect(watcher.data(), &FileWatcher::fileChanged, this, &EntryAttachments::attachmentFileModified);
        m_attachmentFileWatchers.insert(tmpFile.fileName(), watcher);
    } else if (auto path = m_openedAttachments.value(key); m_attachmentFileWatchers.contains(path)) {
        auto watcher = m_attachmentFileWatchers.value(path);
        watcher->stop();

        QFile file(path);
        auto finally = qScopeGuard([&file, &watcher, &path] {
            file.close();
            watcher->start(path, 5);
        });

        const auto attachmentData = value(key);
        const bool saveOk = file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
                            && file.write(attachmentData) == attachmentData.size() && file.flush();

        if (!saveOk) {
            *errorMessage = QString("%1 - %2").arg(key, file.errorString());
            return false;
        }
    }

    const bool openOk = QDesktopServices::openUrl(QUrl::fromLocalFile(m_openedAttachments.value(key)));
    if (!openOk && errorMessage) {
        *errorMessage = tr("Cannot open file \"%1\"").arg(key);
        return false;
    }

    return true;
}

void EntryAttachments::attachmentFileModified(const QString& path)
{
    auto it = m_openedAttachmentsInverse.find(path);
    if (it != m_openedAttachmentsInverse.end()) {
        emit valueModifiedExternally(it.value(), path);
    }
}
