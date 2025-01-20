/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
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

#include "ShareExport.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "crypto/Random.h"
#include "format/KeePass2Writer.h"
#include "gui/Icons.h"
#include "gui/MessageBox.h"
#include "keeshare/KeeShare.h"
#include "keys/PasswordKey.h"

#include <QBuffer>
#include <botan/pubkey.h>
#include <minizip/zip.h>

// Compatibility with minizip-ng
#ifdef MZ_VERSION_BUILD
#undef Z_BEST_COMPRESSION
#define Z_BEST_COMPRESSION MZ_COMPRESS_LEVEL_BEST
#define zipOpenNewFileInZip64 zipOpenNewFileInZip_64
#endif

namespace
{
    void resolveReferenceAttributes(Entry* targetEntry, const Database* sourceDb)
    {
        for (const auto& attribute : EntryAttributes::DefaultAttributes) {
            const auto standardValue = targetEntry->attributes()->value(attribute);
            const auto type = targetEntry->placeholderType(standardValue);
            if (type != Entry::PlaceholderType::Reference) {
                // No reference to resolve
                continue;
            }
            const auto* referencedTargetEntry = targetEntry->resolveReference(standardValue);
            if (referencedTargetEntry) {
                // References is within scope, no resolving needed
                continue;
            }
            // We could do more sophisticated **** trying to point the reference to the next in-scope reference
            // but those cases with high probability constructed examples and very rare in real usage
            const auto* sourceReference = sourceDb->rootGroup()->findEntryByUuid(targetEntry->uuid());
            const auto resolvedValue = sourceReference->resolveMultiplePlaceholders(standardValue);
            targetEntry->setUpdateTimeinfo(false);
            targetEntry->attributes()->set(attribute, resolvedValue, targetEntry->attributes()->isProtected(attribute));
            targetEntry->setUpdateTimeinfo(true);
        }
    }

    void cloneIcon(Metadata* targetMetadata, const Database* sourceDb, const QUuid& iconUuid)
    {
        if (!iconUuid.isNull() && !targetMetadata->hasCustomIcon(iconUuid)) {
            targetMetadata->addCustomIcon(iconUuid, sourceDb->metadata()->customIcon(iconUuid));
        }
    }

    void cloneEntries(Metadata* targetMetadata, const Group* sourceGroup, Group* targetGroup)
    {
        for (const Entry* sourceEntry : sourceGroup->entries()) {
            auto* targetEntry = sourceEntry->clone(Entry::CloneIncludeHistory);
            const bool updateTimeinfoEntry = targetEntry->canUpdateTimeinfo();
            targetEntry->setUpdateTimeinfo(false);
            targetEntry->setGroup(targetGroup);
            targetEntry->setUpdateTimeinfo(updateTimeinfoEntry);
            cloneIcon(targetMetadata, sourceEntry->database(), targetEntry->iconUuid());
        }
    }

    void cloneChildren(Metadata* targetMetadata, const Group* sourceRoot, Group* targetRoot)
    {
        for (const Group* sourceGroup : sourceRoot->children()) {
            auto* targetGroup = sourceGroup->clone(Entry::CloneNoFlags, Group::CloneNoFlags);
            const bool updateTimeinfo = targetGroup->canUpdateTimeinfo();
            targetGroup->setUpdateTimeinfo(false);
            targetGroup->setParent(targetRoot);
            targetGroup->setUpdateTimeinfo(updateTimeinfo);
            cloneIcon(targetMetadata, sourceRoot->database(), targetGroup->iconUuid());
            cloneEntries(targetMetadata, sourceGroup, targetGroup);
            cloneChildren(targetMetadata, sourceGroup, targetGroup);
        }
    }

    Database* extractIntoDatabase(const KeeShareSettings::Reference& reference, const Group* sourceRoot)
    {
        const auto* sourceDb = sourceRoot->database();
        auto* targetDb = new Database();
        auto* targetMetadata = targetDb->metadata();
        targetMetadata->setRecycleBinEnabled(false);

        // Copy the source root as the root of the export database, memory manage the old root node
        auto* targetRoot = sourceRoot->clone(Entry::CloneNoFlags, Group::CloneNoFlags);
        const bool updateTimeinfo = targetRoot->canUpdateTimeinfo();
        targetRoot->setUpdateTimeinfo(false);
        KeeShare::setReferenceTo(targetRoot, KeeShareSettings::Reference());
        targetRoot->setUpdateTimeinfo(updateTimeinfo);
        cloneIcon(targetMetadata, sourceRoot->database(), targetRoot->iconUuid());
        cloneEntries(targetMetadata, sourceRoot, targetRoot);
        if (reference.recurse) {
            cloneChildren(targetMetadata, sourceRoot, targetRoot);
        }

        auto key = QSharedPointer<CompositeKey>::create();
        key->addKey(QSharedPointer<PasswordKey>::create(reference.password));
        targetDb->setKey(key);

        auto obsoleteRoot = targetDb->setRootGroup(targetRoot);
        delete obsoleteRoot;

        targetDb->metadata()->setName(sourceRoot->name());

        // Push all deletions of the source database to the target
        // simple moving out of a share group will not trigger a deletion in the
        // target - a more elaborate mechanism may need the use of another custom
        // attribute to share unshared entries from the target db
        for (const auto& object : sourceDb->deletedObjects()) {
            targetDb->addDeletedObject(object);
        }
        for (auto* targetEntry : targetRoot->entriesRecursive(false)) {
            if (targetEntry->hasReferences()) {
                resolveReferenceAttributes(targetEntry, sourceDb);
            }
        }
        return targetDb;
    }

    bool writeZipFile(void* zf, const QString& fileName, const QByteArray& data)
    {
        zipOpenNewFileInZip64(zf,
                              fileName.toLatin1().data(),
                              nullptr,
                              nullptr,
                              0,
                              nullptr,
                              0,
                              nullptr,
                              Z_DEFLATED,
                              Z_BEST_COMPRESSION,
                              1);
        int pos = 0;
        do {
            auto len = qMin(data.size() - pos, 8192);
            zipWriteInFileInZip(zf, data.data() + pos, len);
            pos += len;
        } while (pos < data.size());

        zipCloseFileInZip(zf);
        return true;
    }

    bool signData(const QByteArray& data, const KeeShareSettings::Key& key, QString& signature)
    {
        if (key.key->algo_name() == "RSA") {
            try {
                Botan::PK_Signer signer(*key.key, *randomGen()->getRng(), "EMSA3(SHA-256)");
                signer.update(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
                auto s = signer.signature(*randomGen()->getRng());

                auto hex = QByteArray(reinterpret_cast<char*>(s.data()), s.size()).toHex();
                signature = QString("rsa|%1").arg(QString::fromLatin1(hex));
                return true;
            } catch (std::exception& e) {
                qWarning("KeeShare: Failed to sign data: %s", e.what());
                return false;
            }
        }
        qWarning("Unsupported Public/Private key format");
        return false;
    }
} // namespace

ShareObserver::Result ShareExport::intoContainer(const QString& resolvedPath,
                                                 const KeeShareSettings::Reference& reference,
                                                 const Group* group)
{
    QScopedPointer<Database> targetDb(extractIntoDatabase(reference, group));
    if (resolvedPath.endsWith(".kdbx.share")) {
        // Write database to memory and sign it
        QByteArray dbData, signatureData;
        QBuffer buffer;

        buffer.setBuffer(&dbData);
        buffer.open(QIODevice::WriteOnly);

        KeePass2Writer writer;
        if (!writer.writeDatabase(&buffer, targetDb.data())) {
            qWarning("Serializing export database failed: %s.", writer.errorString().toLatin1().data());
            return {reference.path, ShareObserver::Result::Error, writer.errorString()};
        }

        buffer.close();

        // Get Own Certificate for signing
        const auto own = KeeShare::own();
        Q_ASSERT(!own.isNull());

        // Sign the database data
        KeeShareSettings::Sign sign;
        sign.certificate = own.certificate;
        signData(dbData, own.key, sign.signature);

        signatureData = KeeShareSettings::Sign::serialize(sign).toLatin1();

        auto zf = zipOpen64(resolvedPath.toLatin1().data(), 0);
        if (!zf) {
            return {reference.path, ShareObserver::Result::Error, ShareExport::tr("Could not write export container.")};
        }

        writeZipFile(zf, KeeShare::signatureFileName().toLatin1().data(), signatureData);
        writeZipFile(zf, KeeShare::containerFileName().toLatin1().data(), dbData);

        zipClose(zf, nullptr);
    } else {
        QString error;
        if (!targetDb->saveAs(resolvedPath, Database::Atomic, {}, &error)) {
            qWarning("Exporting database failed: %s.", error.toLatin1().data());
            return {resolvedPath, ShareObserver::Result::Error, error};
        }
    }

    return {resolvedPath};
}
