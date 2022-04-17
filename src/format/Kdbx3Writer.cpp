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

#include "Kdbx3Writer.h"

#include <QBuffer>

#include "crypto/CryptoHash.h"
#include "crypto/Random.h"
#include "format/KdbxXmlWriter.h"
#include "format/KeePass2RandomStream.h"
#include "streams/HashedBlockStream.h"
#include "streams/SymmetricCipherStream.h"
#include "streams/qtiocompressor.h"

bool Kdbx3Writer::writeDatabase(QIODevice* device, Database* db)
{
    m_error = false;
    m_errorStr.clear();

    auto mode = SymmetricCipher::cipherUuidToMode(db->cipher());
    int ivSize = SymmetricCipher::defaultIvSize(mode);
    if (ivSize < 0) {
        raiseError(tr("Invalid symmetric cipher IV size.", "IV = Initialization Vector for symmetric cipher"));
        return false;
    }

    QByteArray masterSeed = randomGen()->randomArray(32);
    QByteArray encryptionIV = randomGen()->randomArray(ivSize);
    QByteArray protectedStreamKey = randomGen()->randomArray(32);
    QByteArray startBytes = randomGen()->randomArray(32);
    QByteArray endOfHeader = "\r\n\r\n";

    if (!db->challengeMasterSeed(masterSeed)) {
        raiseError(tr("Unable to issue challenge-response: %1").arg(db->keyError()));
        return false;
    }

    if (!db->setKey(db->key(), false, true)) {
        raiseError(tr("Unable to calculate database key"));
        return false;
    }

    // generate transformed database key
    CryptoHash hash(CryptoHash::Sha256);
    hash.addData(masterSeed);
    hash.addData(db->challengeResponseKey());
    Q_ASSERT(!db->transformedDatabaseKey().isEmpty());
    hash.addData(db->transformedDatabaseKey());
    QByteArray finalKey = hash.result();

    // write header
    QBuffer header;
    header.open(QIODeviceBase::WriteOnly);

    writeMagicNumbers(&header, KeePass2::SIGNATURE_1, KeePass2::SIGNATURE_2, db->formatVersion());

    CHECK_RETURN_FALSE(writeHeaderField<quint16>(&header, KeePass2::HeaderFieldID::CipherID, db->cipher().toRfc4122()));
    CHECK_RETURN_FALSE(
        writeHeaderField<quint16>(&header,
                                  KeePass2::HeaderFieldID::CompressionFlags,
                                  Endian::sizedIntToBytes<qint32>(db->compressionAlgorithm(), KeePass2::BYTEORDER)));
    auto kdf = db->kdf();
    CHECK_RETURN_FALSE(writeHeaderField<quint16>(&header, KeePass2::HeaderFieldID::MasterSeed, masterSeed));
    CHECK_RETURN_FALSE(writeHeaderField<quint16>(&header, KeePass2::HeaderFieldID::TransformSeed, kdf->seed()));
    CHECK_RETURN_FALSE(writeHeaderField<quint16>(&header,
                                                 KeePass2::HeaderFieldID::TransformRounds,
                                                 Endian::sizedIntToBytes<qint64>(kdf->rounds(), KeePass2::BYTEORDER)));
    CHECK_RETURN_FALSE(writeHeaderField<quint16>(&header, KeePass2::HeaderFieldID::EncryptionIV, encryptionIV));
    CHECK_RETURN_FALSE(
        writeHeaderField<quint16>(&header, KeePass2::HeaderFieldID::ProtectedStreamKey, protectedStreamKey));
    CHECK_RETURN_FALSE(writeHeaderField<quint16>(&header, KeePass2::HeaderFieldID::StreamStartBytes, startBytes));
    CHECK_RETURN_FALSE(writeHeaderField<quint16>(
        &header,
        KeePass2::HeaderFieldID::InnerRandomStreamID,
        Endian::sizedIntToBytes<qint32>(static_cast<qint32>(KeePass2::ProtectedStreamAlgo::Salsa20),
                                        KeePass2::BYTEORDER)));
    CHECK_RETURN_FALSE(writeHeaderField<quint16>(&header, KeePass2::HeaderFieldID::EndOfHeader, endOfHeader));
    header.close();

    // write header data
    CHECK_RETURN_FALSE(writeData(device, header.data()));

    // hash header
    const QByteArray headerHash = CryptoHash::hash(header.data(), CryptoHash::Sha256);

    // write cipher stream
    SymmetricCipherStream cipherStream(device);
    cipherStream.init(mode, SymmetricCipher::Encrypt, finalKey, encryptionIV);
    if (!cipherStream.open(QIODeviceBase::WriteOnly)) {
        raiseError(cipherStream.errorString());
        return false;
    }
    CHECK_RETURN_FALSE(writeData(&cipherStream, startBytes));

    HashedBlockStream hashedStream(&cipherStream);
    if (!hashedStream.open(QIODeviceBase::WriteOnly)) {
        raiseError(hashedStream.errorString());
        return false;
    }

    QIODevice* outputDevice = nullptr;
    QScopedPointer<QtIOCompressor> ioCompressor;

    if (db->compressionAlgorithm() == Database::CompressionNone) {
        outputDevice = &hashedStream;
    } else {
        ioCompressor.reset(new QtIOCompressor(&hashedStream));
        ioCompressor->setStreamFormat(QtIOCompressor::GzipFormat);
        if (!ioCompressor->open(QIODeviceBase::WriteOnly)) {
            raiseError(ioCompressor->errorString());
            return false;
        }
        outputDevice = ioCompressor.data();
    }

    Q_ASSERT(outputDevice);

    KeePass2RandomStream randomStream;
    if (!randomStream.init(SymmetricCipher::Salsa20, protectedStreamKey)) {
        raiseError(randomStream.errorString());
        return false;
    }

    KdbxXmlWriter xmlWriter(db->formatVersion());
    xmlWriter.writeDatabase(outputDevice, db, &randomStream, headerHash);

    // Explicitly close/reset streams so they are flushed and we can detect
    // errors. QIODeviceBase::close() resets errorString() etc.
    if (ioCompressor) {
        ioCompressor->close();
    }
    if (!hashedStream.reset()) {
        raiseError(hashedStream.errorString());
        return false;
    }
    if (!cipherStream.reset()) {
        raiseError(cipherStream.errorString());
        return false;
    }

    if (xmlWriter.hasError()) {
        raiseError(xmlWriter.errorString());
        return false;
    }

    return true;
}
