
/*
 * Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "KdbxReader.h"
#include "core/Database.h"
#include "core/Endian.h"
#include "crypto/SymmetricCipher.h"
#include "keys/MultiAuthenticationHeaderKey.h"
#include "streams/StoreDataStream.h"

#define UUID_LENGTH 16

/**
 * Read KDBX magic header numbers from a device.
 *
 * Passing a null key will only read in the unprotected headers.
 *
 * @param device input device
 * @param sig1 KDBX signature 1
 * @param sig2 KDBX signature 2
 * @param version KDBX version
 * @return true if magic numbers were read successfully
 */
bool KdbxReader::readMagicNumbers(QIODevice* device, quint32& sig1, quint32& sig2, quint32& version)
{
    bool ok;
    sig1 = Endian::readSizedInt<quint32>(device, KeePass2::BYTEORDER, &ok);
    if (!ok) {
        return false;
    }

    sig2 = Endian::readSizedInt<quint32>(device, KeePass2::BYTEORDER, &ok);
    if (!ok) {
        return false;
    }

    version = Endian::readSizedInt<quint32>(device, KeePass2::BYTEORDER, &ok);

    return ok;
}

/**
 * Read KDBX stream from device.
 * The device will automatically be reset to 0 before reading.
 *
 * Passing a null key will only read in the unprotected headers.
 *
 * @param device input device
 * @param key database encryption composite key
 * @param db database to read into
 * @return true on success
 */
bool KdbxReader::readDatabase(QIODevice* device, QSharedPointer<const CompositeKey> key, Database* db)
{
    device->seek(0);

    m_db = db;
    m_masterSeed.clear();
    m_encryptionIV.clear();
    m_streamStartBytes.clear();
    m_protectedStreamKey.clear();

    StoreDataStream headerStream(device);
    headerStream.open(QIODevice::ReadOnly);

    // read KDBX magic numbers
    quint32 sig1, sig2, version;
    if (!readMagicNumbers(&headerStream, sig1, sig2, version)) {
        return false;
    }
    m_kdbxSignature = qMakePair(sig1, sig2);
    m_db->setFormatVersion(version);

    // read header fields
    while (readHeaderField(headerStream, m_db) && !hasError()) {
    }

    headerStream.close();

    if (hasError()) {
        return false;
    }

    auto authenticationFactorInfo = m_db->authenticationFactorInfo();
    if (!authenticationFactorInfo.isNull()) {
        // Augment (or replace) the given composite key with factors from the header
        auto newCompositeKey = QSharedPointer<CompositeKey>::create();
        if (!authenticationFactorInfo->isComprehensive() && !key.isNull()) {
            // New composite should start with old key info
            for (const auto& keyPart : key->keys()) {
                newCompositeKey->addKey(keyPart);
            }
        }

        auto headerInfoKey = QSharedPointer<MultiAuthenticationHeaderKey>::create(authenticationFactorInfo, key);
        if (!headerInfoKey->process()) {
            m_error = true;
            m_errorStr = headerInfoKey->error();
            return false;
        }

        newCompositeKey->addKey(headerInfoKey);

        key = newCompositeKey;
    }

    // No key provided - don't proceed to load payload
    if (key.isNull()) {
        return true;
    }

    // read payload
    return readDatabaseImpl(device, headerStream.storedData(), std::move(key), db);
}

bool KdbxReader::hasError() const
{
    return m_error;
}

QString KdbxReader::errorString() const
{
    return m_errorStr;
}

KeePass2::ProtectedStreamAlgo KdbxReader::protectedStreamAlgo() const
{
    return m_irsAlgo;
}

/**
 * @param data stream cipher UUID as bytes
 */
void KdbxReader::setCipher(const QByteArray& data)
{
    if (data.size() != UUID_LENGTH) {
        raiseError(tr("Invalid cipher uuid length: %1 (length=%2)").arg(QString(data)).arg(data.size()));
        return;
    }

    QUuid uuid = QUuid::fromRfc4122(data);
    if (uuid.isNull()) {
        raiseError(tr("Unable to parse UUID: %1").arg(QString(data)));
        return;
    }

    if (SymmetricCipher::cipherUuidToMode(uuid) == SymmetricCipher::InvalidMode) {
        raiseError(tr("Unsupported cipher"));
        return;
    }
    m_db->setCipher(uuid);
}

/**
 * @param data compression flags as bytes
 */
void KdbxReader::setCompressionFlags(const QByteArray& data)
{
    if (data.size() != 4) {
        raiseError(tr("Invalid compression flags length"));
        return;
    }
    auto id = Endian::bytesToSizedInt<quint32>(data, KeePass2::BYTEORDER);

    if (id > Database::CompressionAlgorithmMax) {
        raiseError(tr("Unsupported compression algorithm"));
        return;
    }
    m_db->setCompressionAlgorithm(static_cast<Database::CompressionAlgorithm>(id));
}

/**
 * @param data master seed as bytes
 */
void KdbxReader::setMasterSeed(const QByteArray& data)
{
    if (data.size() != 32) {
        raiseError(tr("Invalid master seed size"));
        return;
    }
    m_masterSeed = data;
}

/**
 * @param data KDF seed as bytes
 */
void KdbxReader::setTransformSeed(const QByteArray& data)
{
    if (data.size() != 32) {
        raiseError(tr("Invalid transform seed size"));
        return;
    }

    auto kdf = m_db->kdf();
    if (!kdf.isNull()) {
        kdf->setSeed(data);
    }
}

/**
 * @param data KDF transform rounds as bytes
 */
void KdbxReader::setTransformRounds(const QByteArray& data)
{
    if (data.size() != 8) {
        raiseError(tr("Invalid transform rounds size"));
        return;
    }

    auto rounds = Endian::bytesToSizedInt<quint64>(data, KeePass2::BYTEORDER);
    auto kdf = m_db->kdf();
    if (!kdf.isNull()) {
        kdf->setRounds(static_cast<int>(rounds));
    }
}

/**
 * @param data cipher stream IV as bytes
 */
void KdbxReader::setEncryptionIV(const QByteArray& data)
{
    m_encryptionIV = data;
}

/**
 * @param data key for random (inner) stream as bytes
 */
void KdbxReader::setProtectedStreamKey(const QByteArray& data)
{
    m_protectedStreamKey = data;
}

/**
 * @param data start bytes for cipher stream
 */
void KdbxReader::setStreamStartBytes(const QByteArray& data)
{
    if (data.size() != 32) {
        raiseError(tr("Invalid start bytes size"));
        return;
    }
    m_streamStartBytes = data;
}

/**
 * @param data id of inner cipher stream algorithm
 */
void KdbxReader::setInnerRandomStreamID(const QByteArray& data)
{
    if (data.size() != 4) {
        raiseError(tr("Invalid random stream id size"));
        return;
    }
    auto id = Endian::bytesToSizedInt<quint32>(data, KeePass2::BYTEORDER);
    KeePass2::ProtectedStreamAlgo irsAlgo = KeePass2::idToProtectedStreamAlgo(id);
    if (irsAlgo == KeePass2::ProtectedStreamAlgo::InvalidProtectedStreamAlgo
        || irsAlgo == KeePass2::ProtectedStreamAlgo::ArcFourVariant) {
        raiseError(tr("Invalid inner random stream cipher"));
        return;
    }
    m_irsAlgo = irsAlgo;
}

/**
 * Raise an error. Use in case of an unexpected read error.
 *
 * @param errorMessage error message
 */
void KdbxReader::raiseError(const QString& errorMessage)
{
    m_error = true;
    m_errorStr = errorMessage;
}
