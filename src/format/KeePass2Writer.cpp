/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include <QFile>

#include "core/Group.h"
#include "core/Metadata.h"
#include "format/Kdbx3Writer.h"
#include "format/Kdbx4Writer.h"
#include "format/KeePass2Writer.h"

/**
 * Write a database to a KDBX file.
 *
 * @param filename output filename
 * @param db source database
 * @return true on success
 */
bool KeePass2Writer::writeDatabase(const QString& filename, Database* db)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        raiseError(file.errorString());
        return false;
    }
    return writeDatabase(&file, db);
}

#define VERSION_MAX(a, b)                                                                                              \
    a = qMax(a, b);                                                                                                    \
    if (a >= KeePass2::FILE_VERSION_MAX) {                                                                             \
        return a;                                                                                                      \
    }

/**
 * Get the minimum KDBX version required for writing the database.
 */
quint32 KeePass2Writer::kdbxVersionRequired(Database const* db, bool ignoreCurrent, bool ignoreKdf)
{
    quint32 version = KeePass2::FILE_VERSION_3_1;
    if (!ignoreCurrent) {
        VERSION_MAX(version, db->formatVersion())
    }

    if (!ignoreKdf && !db->kdf().isNull() && !db->kdf()->uuid().isNull()
        && db->kdf()->uuid() != KeePass2::KDF_AES_KDBX3) {
        VERSION_MAX(version, KeePass2::FILE_VERSION_4)
    }

    if (!db->publicCustomData().isEmpty()) {
        VERSION_MAX(version, KeePass2::FILE_VERSION_4)
    }

    for (const auto* group : db->rootGroup()->groupsRecursive(true)) {
        if (group->customData() && !group->customData()->isEmpty()) {
            VERSION_MAX(version, KeePass2::FILE_VERSION_4)
        }
        if (!group->tags().isEmpty()) {
            VERSION_MAX(version, KeePass2::FILE_VERSION_4_1)
        }
        if (group->previousParentGroup()) {
            VERSION_MAX(version, KeePass2::FILE_VERSION_4_1)
        }

        for (const auto* entry : group->entries()) {

            if (entry->customData() && !entry->customData()->isEmpty()) {
                VERSION_MAX(version, KeePass2::FILE_VERSION_4)
            }
            if (entry->excludeFromReports()) {
                VERSION_MAX(version, KeePass2::FILE_VERSION_4_1)
            }
            if (entry->previousParentGroup()) {
                VERSION_MAX(version, KeePass2::FILE_VERSION_4_1)
            }

            for (const auto* historyItem : entry->historyItems()) {
                if (historyItem->customData() && !historyItem->customData()->isEmpty()) {
                    VERSION_MAX(version, KeePass2::FILE_VERSION_4)
                }
            }
        }
    }

    const QList<QUuid> customIconsOrder = db->metadata()->customIconsOrder();
    for (const QUuid& uuid : customIconsOrder) {
        const auto& icon = db->metadata()->customIcon(uuid);
        if (!icon.name.isEmpty() || icon.lastModified.isValid()) {
            VERSION_MAX(version, KeePass2::FILE_VERSION_4_1)
        }
    }

    if (db->compressionAlgorithm() == Database::CompressionZstd) {
        VERSION_MAX(version, KeePass2::FILE_VERSION_4_1)
    }

    return version;
}

/**
 * Write a database to a device in KDBX format.
 *
 * @param device output device
 * @param db source database
 * @return true on success
 */
bool KeePass2Writer::writeDatabase(QIODevice* device, Database* db)
{
    m_error = false;
    m_errorStr.clear();

    m_version = kdbxVersionRequired(db);
    if (db->kdf()->uuid() == KeePass2::KDF_AES_KDBX3 && m_version >= KeePass2::FILE_VERSION_4) {
        // We MUST re-transform the key, because challenge-response hashing has changed in KDBX 4.
        // If we forget to re-transform, the database will be saved WITHOUT a challenge-response key component!
        auto kdf = KeePass2::uuidToKdf(KeePass2::KDF_AES_KDBX4);
        kdf->setRounds(db->kdf()->rounds());
        db->changeKdf(kdf);
    }

    db->setFormatVersion(m_version);
    if (db->kdf()->uuid() == KeePass2::KDF_AES_KDBX3) {
        Q_ASSERT(m_version <= KeePass2::FILE_VERSION_3_1);
        m_writer.reset(new Kdbx3Writer());
    } else {
        Q_ASSERT(m_version >= KeePass2::FILE_VERSION_4);
        m_writer.reset(new Kdbx4Writer());
    }

    return m_writer->writeDatabase(device, db);
}

void KeePass2Writer::extractDatabase(Database* db, QByteArray& xmlOutput)
{
    m_error = false;
    m_errorStr.clear();

    m_version = kdbxVersionRequired(db);
    db->setFormatVersion(m_version);
    if (db->kdf()->uuid() == KeePass2::KDF_AES_KDBX3) {
        Q_ASSERT(m_version <= KeePass2::FILE_VERSION_3_1);
        m_writer.reset(new Kdbx3Writer());
    } else {
        Q_ASSERT(m_version >= KeePass2::FILE_VERSION_4);
        m_writer.reset(new Kdbx4Writer());
    }

    m_writer->extractDatabase(xmlOutput, db);
}

bool KeePass2Writer::hasError() const
{
    return m_error || (m_writer && m_writer->hasError());
}

QString KeePass2Writer::errorString() const
{
    return m_writer ? m_writer->errorString() : m_errorStr;
}

/**
 * Raise an error. Use in case of an unexpected write error.
 *
 * @param errorMessage error message
 */
void KeePass2Writer::raiseError(const QString& errorMessage)
{
    m_error = true;
    m_errorStr = errorMessage;
}

/**
 * @return KDBX writer used for writing the output file
 */
QSharedPointer<KdbxWriter> KeePass2Writer::writer() const
{
    return {};
}

/**
 * @return KDBX version used for writing the output file
 */
quint32 KeePass2Writer::version() const
{
    return m_version;
}
