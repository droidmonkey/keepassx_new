/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#include "Kdbx3Reader.h"

#include "core/AsyncTask.h"
#include "core/Endian.h"
#include "core/Group.h"
#include "crypto/CryptoHash.h"
#include "format/KdbxXmlReader.h"
#include "format/KeePass2RandomStream.h"
#include "streams/HashedBlockStream.h"
#include "streams/StoreDataStream.h"
#include "streams/SymmetricCipherStream.h"
#include "streams/qtiocompressor.h"

bool Kdbx3Reader::readDatabaseImpl(QIODevice* device,
                                   const QByteArray& headerData,
                                   QSharedPointer<const CompositeKey> key,
                                   Database* db)
{
    Q_ASSERT((db->formatVersion() & KeePass2::FILE_VERSION_CRITICAL_MASK) <= KeePass2::FILE_VERSION_3);

    if (hasError()) {
        return false;
    }

    // check if all required headers were present
    if (m_masterSeed.isEmpty() || m_encryptionIV.isEmpty() || m_streamStartBytes.isEmpty()
        || m_protectedStreamKey.isEmpty() || db->cipher().isNull()) {
        raiseError(tr("Missing database headers"));
        return false;
    }

    bool ok = AsyncTask::runAndWaitForFuture([&] { return db->setKey(key, false); });
    if (!ok) {
        raiseError(tr("Unable to calculate database key"));
        return false;
    }

    if (!db->challengeMasterSeed(m_masterSeed)) {
        raiseError(tr("Unable to issue challenge-response: %1").arg(db->keyError()));
        return false;
    }

    CryptoHash hash(CryptoHash::Sha256);
    hash.addData(m_masterSeed);
    hash.addData(db->challengeResponseKey());
    hash.addData(db->transformedDatabaseKey());
    QByteArray finalKey = hash.result();

    auto mode = SymmetricCipher::cipherUuidToMode(db->cipher());
    SymmetricCipherStream cipherStream(device);
    if (!cipherStream.init(mode, SymmetricCipher::Decrypt, finalKey, m_encryptionIV)) {
        raiseError(cipherStream.errorString());
        return false;
    }
    if (!cipherStream.open(QIODeviceBase::ReadOnly)) {
        raiseError(cipherStream.errorString());
        return false;
    }

    QByteArray realStart = cipherStream.read(32);

    if (realStart != m_streamStartBytes) {
        raiseError(tr("Invalid credentials were provided, please try again.\n"
                      "If this reoccurs, then your database file may be corrupt."));
        return false;
    }

    HashedBlockStream hashedStream(&cipherStream);
    if (!hashedStream.open(QIODeviceBase::ReadOnly)) {
        raiseError(hashedStream.errorString());
        return false;
    }

    QIODevice* xmlDevice = nullptr;
    QScopedPointer<QtIOCompressor> ioCompressor;

    if (db->compressionAlgorithm() == Database::CompressionNone) {
        xmlDevice = &hashedStream;
    } else {
        ioCompressor.reset(new QtIOCompressor(&hashedStream));
        ioCompressor->setStreamFormat(QtIOCompressor::GzipFormat);
        if (!ioCompressor->open(QIODeviceBase::ReadOnly)) {
            raiseError(ioCompressor->errorString());
            return false;
        }
        xmlDevice = ioCompressor.data();
    }

    KeePass2RandomStream randomStream;
    if (!randomStream.init(SymmetricCipher::Salsa20, m_protectedStreamKey)) {
        raiseError(randomStream.errorString());
        return false;
    }

    Q_ASSERT(xmlDevice);

    KdbxXmlReader xmlReader(KeePass2::FILE_VERSION_3_1);
    xmlReader.readDatabase(xmlDevice, db, &randomStream);

    if (xmlReader.hasError()) {
        raiseError(xmlReader.errorString());
        return false;
    }

    Q_ASSERT(!xmlReader.headerHash().isEmpty() || db->formatVersion() < KeePass2::FILE_VERSION_3_1);

    if (!xmlReader.headerHash().isEmpty()) {
        QByteArray headerHash = CryptoHash::hash(headerData, CryptoHash::Sha256);
        if (headerHash != xmlReader.headerHash()) {
            raiseError(tr("Header doesn't match hash"));
            return false;
        }
    }

    return true;
}

bool Kdbx3Reader::readHeaderField(StoreDataStream& headerStream, Database* db)
{
    Q_UNUSED(db);

    QByteArray fieldIDArray = headerStream.read(1);
    if (fieldIDArray.size() != 1) {
        raiseError(tr("Invalid header id size"));
        return false;
    }
    char fieldID = fieldIDArray.at(0);

    bool ok;
    auto fieldLen = Endian::readSizedInt<quint16>(&headerStream, KeePass2::BYTEORDER, &ok);
    if (!ok) {
        raiseError(tr("Invalid header field length: field %1").arg(fieldID));
        return false;
    }

    QByteArray fieldData;
    if (fieldLen != 0) {
        fieldData = headerStream.read(fieldLen);
        if (fieldData.size() != fieldLen) {
            raiseError(tr("Invalid header data length: field %1, %2 expected, %3 found")
                           .arg(fieldID)
                           .arg(fieldLen)
                           .arg(fieldData.size()));
            return false;
        }
    }

    bool headerEnd = false;
    switch (static_cast<KeePass2::HeaderFieldID>(fieldID)) {
    case KeePass2::HeaderFieldID::EndOfHeader:
        headerEnd = true;
        break;

    case KeePass2::HeaderFieldID::CipherID:
        setCipher(fieldData);
        break;

    case KeePass2::HeaderFieldID::CompressionFlags:
        setCompressionFlags(fieldData);
        break;

    case KeePass2::HeaderFieldID::MasterSeed:
        setMasterSeed(fieldData);
        break;

    case KeePass2::HeaderFieldID::TransformSeed:
        setTransformSeed(fieldData);
        break;

    case KeePass2::HeaderFieldID::TransformRounds:
        setTransformRounds(fieldData);
        break;

    case KeePass2::HeaderFieldID::EncryptionIV:
        setEncryptionIV(fieldData);
        break;

    case KeePass2::HeaderFieldID::ProtectedStreamKey:
        setProtectedStreamKey(fieldData);
        break;

    case KeePass2::HeaderFieldID::StreamStartBytes:
        setStreamStartBytes(fieldData);
        break;

    case KeePass2::HeaderFieldID::InnerRandomStreamID:
        setInnerRandomStreamID(fieldData);
        break;

    default:
        qWarning("Unknown header field read: id=%d", fieldID);
        break;
    }

    return !headerEnd;
}
