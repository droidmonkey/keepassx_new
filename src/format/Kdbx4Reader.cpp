/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "Kdbx4Reader.h"

#include <QBuffer>
#include <QJsonObject>

#include "core/AsyncTask.h"
#include "core/Endian.h"
#include "core/Group.h"
#include "crypto/CryptoHash.h"
#include "format/KdbxXmlReader.h"
#include "format/KeePass2RandomStream.h"
#include "streams/HmacBlockStream.h"
#include "streams/StoreDataStream.h"
#include "streams/SymmetricCipherStream.h"
#include "streams/qtiocompressor.h"

bool Kdbx4Reader::readDatabaseImpl(QIODevice* device,
                                   const QByteArray& headerData,
                                   QSharedPointer<const CompositeKey> key,
                                   Database* db)
{
    Q_ASSERT((db->formatVersion() & KeePass2::FILE_VERSION_CRITICAL_MASK) == KeePass2::FILE_VERSION_4);

    m_binaryPool.clear();

    if (hasError()) {
        return false;
    }

    // check if all required headers were present
    if (m_masterSeed.isEmpty() || m_encryptionIV.isEmpty() || db->cipher().isNull()) {
        raiseError(tr("missing database headers"));
        return false;
    }

    bool ok = AsyncTask::runAndWaitForFuture([&] { return db->setKey(key, false, false); });
    if (!ok) {
        raiseError(tr("Unable to calculate database key: %1").arg(db->keyError()));
        return false;
    }

    CryptoHash hash(CryptoHash::Sha256);
    hash.addData(m_masterSeed);
    hash.addData(db->transformedDatabaseKey());
    QByteArray finalKey = hash.result();

    QByteArray headerSha256 = device->read(32);
    QByteArray headerHmac = device->read(32);
    if (headerSha256.size() != 32 || headerHmac.size() != 32) {
        raiseError(tr("Invalid header checksum size"));
        return false;
    }
    if (headerSha256 != CryptoHash::hash(headerData, CryptoHash::Sha256)) {
        raiseError(tr("Header SHA256 mismatch"));
        return false;
    }

    // clang-format off
    QByteArray hmacKey = KeePass2::hmacKey(m_masterSeed, db->transformedDatabaseKey());
    if (headerHmac != CryptoHash::hmac(headerData, HmacBlockStream::getHmacKey(UINT64_MAX, hmacKey), CryptoHash::Sha256)) {
        raiseError(tr("Invalid credentials were provided, please try again.\n"
                      "If this reoccurs, then your database file may be corrupt.") + " " + tr("(HMAC mismatch)"));
        return false;
    }
    HmacBlockStream hmacStream(device, hmacKey);
    if (!hmacStream.open(QIODeviceBase::ReadOnly)) {
        raiseError(hmacStream.errorString());
        return false;
    }

    auto mode = SymmetricCipher::cipherUuidToMode(db->cipher());
    if (mode == SymmetricCipher::InvalidMode) {
        raiseError(tr("Unknown cipher"));
        return false;
    }
    SymmetricCipherStream cipherStream(&hmacStream);
    if (!cipherStream.init(mode, SymmetricCipher::Decrypt, finalKey, m_encryptionIV)) {
        raiseError(cipherStream.errorString());
        return false;
    }
    if (!cipherStream.open(QIODeviceBase::ReadOnly)) {
        raiseError(cipherStream.errorString());
        return false;
    }
    // clang-format on

    QIODevice* xmlDevice = nullptr;
    QScopedPointer<QtIOCompressor> ioCompressor;

    if (db->compressionAlgorithm() == Database::CompressionNone) {
        xmlDevice = &cipherStream;
    } else {
        ioCompressor.reset(new QtIOCompressor(&cipherStream));
        ioCompressor->setStreamFormat(QtIOCompressor::GzipFormat);
        if (!ioCompressor->open(QIODeviceBase::ReadOnly)) {
            raiseError(ioCompressor->errorString());
            return false;
        }
        xmlDevice = ioCompressor.data();
    }

    while (readInnerHeaderField(xmlDevice) && !hasError()) {
    }

    if (hasError()) {
        return false;
    }

    // TODO: Convert m_irsAlgo to Mode
    switch (m_irsAlgo) {
    case KeePass2::ProtectedStreamAlgo::Salsa20:
        mode = SymmetricCipher::Salsa20;
        break;
    case KeePass2::ProtectedStreamAlgo::ChaCha20:
        mode = SymmetricCipher::ChaCha20;
        break;
    default:
        mode = SymmetricCipher::InvalidMode;
    }

    KeePass2RandomStream randomStream;
    if (!randomStream.init(mode, m_protectedStreamKey)) {
        raiseError(randomStream.errorString());
        return false;
    }

    Q_ASSERT(xmlDevice);

    KdbxXmlReader xmlReader(KeePass2::FILE_VERSION_4, binaryPool());
    xmlReader.readDatabase(xmlDevice, db, &randomStream);

    if (xmlReader.hasError()) {
        raiseError(xmlReader.errorString());
        return false;
    }

    return true;
}

bool Kdbx4Reader::readHeaderField(StoreDataStream& device, Database* db)
{
    QByteArray fieldIDArray = device.read(1);
    if (fieldIDArray.size() != 1) {
        raiseError(tr("Invalid header id size"));
        return false;
    }
    char fieldID = fieldIDArray.at(0);

    bool ok;
    auto fieldLen = Endian::readSizedInt<quint32>(&device, KeePass2::BYTEORDER, &ok);
    if (!ok) {
        raiseError(tr("Invalid header field length: field %1").arg(fieldID));
        return false;
    }

    QByteArray fieldData;
    if (fieldLen != 0) {
        fieldData = device.read(fieldLen);
        if (static_cast<quint32>(fieldData.size()) != fieldLen) {
            raiseError(tr("Invalid header data length: field %1, %2 expected, %3 found")
                           .arg(static_cast<int>(fieldID))
                           .arg(fieldLen)
                           .arg(fieldData.size()));
            return false;
        }
    }

    switch (static_cast<KeePass2::HeaderFieldID>(fieldID)) {
    case KeePass2::HeaderFieldID::EndOfHeader:
        return false;

    case KeePass2::HeaderFieldID::CipherID:
        setCipher(fieldData);
        break;

    case KeePass2::HeaderFieldID::CompressionFlags:
        setCompressionFlags(fieldData);
        break;

    case KeePass2::HeaderFieldID::MasterSeed:
        setMasterSeed(fieldData);
        break;

    case KeePass2::HeaderFieldID::EncryptionIV:
        setEncryptionIV(fieldData);
        break;

    case KeePass2::HeaderFieldID::KdfParameters: {
        QBuffer bufIoDevice(&fieldData);
        if (!bufIoDevice.open(QIODeviceBase::ReadOnly)) {
            raiseError(tr("Failed to open buffer for KDF parameters in header"));
            return false;
        }
        QVariantMap kdfParams = readVariantMap(&bufIoDevice);
        QSharedPointer<Kdf> kdf = KeePass2::kdfFromParameters(kdfParams);
        if (!kdf) {
            raiseError(tr("Unsupported key derivation function (KDF) or invalid parameters"));
            return false;
        }
        db->setKdf(kdf);
        break;
    }

    case KeePass2::HeaderFieldID::PublicCustomData: {
        QBuffer variantBuffer;
        variantBuffer.setBuffer(&fieldData);
        variantBuffer.open(QBuffer::ReadOnly);
        QVariantMap data = readVariantMap(&variantBuffer);
        db->setPublicCustomData(data);
        break;
    }

    case KeePass2::HeaderFieldID::ProtectedStreamKey:
    case KeePass2::HeaderFieldID::TransformRounds:
    case KeePass2::HeaderFieldID::TransformSeed:
    case KeePass2::HeaderFieldID::StreamStartBytes:
    case KeePass2::HeaderFieldID::InnerRandomStreamID:
        raiseError(tr("Legacy header fields found in KDBX4 file."));
        return false;

    default:
        qWarning("Unknown header field read: id=%d", fieldID);
        break;
    }

    return true;
}

/**
 * Helper method for reading KDBX4 inner header fields.
 *
 * @param device input device
 * @return true if there are more inner header fields
 */
bool Kdbx4Reader::readInnerHeaderField(QIODevice* device)
{
    QByteArray fieldIDArray = device->read(1);
    if (fieldIDArray.size() != 1) {
        raiseError(tr("Invalid inner header id size"));
        return false;
    }
    auto fieldID = static_cast<KeePass2::InnerHeaderFieldID>(fieldIDArray.at(0));

    bool ok;
    auto fieldLen = Endian::readSizedInt<quint32>(device, KeePass2::BYTEORDER, &ok);
    if (!ok) {
        raiseError(tr("Invalid inner header field length: field %1").arg(static_cast<int>(fieldID)));
        return false;
    }

    QByteArray fieldData;
    if (fieldLen != 0) {
        fieldData = device->read(fieldLen);
        if (static_cast<quint32>(fieldData.size()) != fieldLen) {
            raiseError(tr("Invalid inner header data length: field %1, %2 expected, %3 found")
                           .arg(static_cast<int>(fieldID))
                           .arg(fieldLen)
                           .arg(fieldData.size()));
            return false;
        }
    }

    switch (fieldID) {
    case KeePass2::InnerHeaderFieldID::End:
        return false;

    case KeePass2::InnerHeaderFieldID::InnerRandomStreamID:
        setInnerRandomStreamID(fieldData);
        break;

    case KeePass2::InnerHeaderFieldID::InnerRandomStreamKey:
        setProtectedStreamKey(fieldData);
        break;

    case KeePass2::InnerHeaderFieldID::Binary: {
        if (fieldLen < 1) {
            raiseError(tr("Invalid inner header binary size"));
            return false;
        }
        auto data = fieldData.mid(1);
        m_binaryPool.insert(QString::number(m_binaryPool.size()), data);
        break;
    }
    }

    return true;
}

/**
 * Helper method for reading a serialized variant map.
 *
 * @param device input device
 * @return de-serialized variant map
 */
QVariantMap Kdbx4Reader::readVariantMap(QIODevice* device)
{
    bool ok;
    quint16 version =
        Endian::readSizedInt<quint16>(device, KeePass2::BYTEORDER, &ok) & KeePass2::VARIANTMAP_CRITICAL_MASK;
    quint16 maxVersion = KeePass2::VARIANTMAP_VERSION & KeePass2::VARIANTMAP_CRITICAL_MASK;
    if (!ok || (version > maxVersion)) {
        //: Translation: variant map = data structure for storing meta data
        raiseError(tr("Unsupported KeePass variant map version."));
        return {};
    }

    QVariantMap vm;
    QByteArray fieldTypeArray;
    KeePass2::VariantMapFieldType fieldType = KeePass2::VariantMapFieldType::End;
    while (((fieldTypeArray = device->read(1)).size() == 1)
           && ((fieldType = static_cast<KeePass2::VariantMapFieldType>(fieldTypeArray.at(0)))
               != KeePass2::VariantMapFieldType::End)) {
        auto nameLen = Endian::readSizedInt<quint32>(device, KeePass2::BYTEORDER, &ok);
        if (!ok) {
            //: Translation: variant map = data structure for storing meta data
            raiseError(tr("Invalid variant map entry name length"));
            return {};
        }
        QByteArray nameBytes;
        if (nameLen != 0) {
            nameBytes = device->read(nameLen);
            if (static_cast<quint32>(nameBytes.size()) != nameLen) {
                //: Translation: variant map = data structure for storing meta data
                raiseError(tr("Invalid variant map entry name data"));
                return {};
            }
        }
        QString name = QString::fromUtf8(nameBytes.toStdString().c_str());

        auto valueLen = Endian::readSizedInt<quint32>(device, KeePass2::BYTEORDER, &ok);
        if (!ok) {
            //: Translation: variant map = data structure for storing meta data
            raiseError(tr("Invalid variant map entry value length"));
            return {};
        }
        QByteArray valueBytes;
        if (valueLen != 0) {
            valueBytes = device->read(valueLen);
            if (static_cast<quint32>(valueBytes.size()) != valueLen) {
                //: Translation comment: variant map = data structure for storing meta data
                raiseError(tr("Invalid variant map entry value data"));
                return {};
            }
        }

        switch (fieldType) {
        case KeePass2::VariantMapFieldType::Bool:
            if (valueLen == 1) {
                vm.insert(name, QVariant(valueBytes.at(0) != 0));
            } else {
                //: Translation: variant map = data structure for storing meta data
                raiseError(tr("Invalid variant map Bool entry value length"));
                return {};
            }
            break;

        case KeePass2::VariantMapFieldType::Int32:
            if (valueLen == 4) {
                vm.insert(name, QVariant(Endian::bytesToSizedInt<qint32>(valueBytes, KeePass2::BYTEORDER)));
            } else {
                //: Translation: variant map = data structure for storing meta data
                raiseError(tr("Invalid variant map Int32 entry value length"));
                return {};
            }
            break;

        case KeePass2::VariantMapFieldType::UInt32:
            if (valueLen == 4) {
                vm.insert(name, QVariant(Endian::bytesToSizedInt<quint32>(valueBytes, KeePass2::BYTEORDER)));
            } else {
                //: Translation: variant map = data structure for storing meta data
                raiseError(tr("Invalid variant map UInt32 entry value length"));
                return {};
            }
            break;

        case KeePass2::VariantMapFieldType::Int64:
            if (valueLen == 8) {
                vm.insert(name, QVariant(Endian::bytesToSizedInt<qint64>(valueBytes, KeePass2::BYTEORDER)));
            } else {
                //: Translation: variant map = data structure for storing meta data
                raiseError(tr("Invalid variant map Int64 entry value length"));
                return {};
            }
            break;

        case KeePass2::VariantMapFieldType::UInt64:
            if (valueLen == 8) {
                vm.insert(name, QVariant(Endian::bytesToSizedInt<quint64>(valueBytes, KeePass2::BYTEORDER)));
            } else {
                //: Translation: variant map = data structure for storing meta data
                raiseError(tr("Invalid variant map UInt64 entry value length"));
                return {};
            }
            break;

        case KeePass2::VariantMapFieldType::String:
            vm.insert(name, QVariant(QString::fromUtf8(valueBytes.toStdString().c_str())));
            break;

        case KeePass2::VariantMapFieldType::ByteArray:
            vm.insert(name, QVariant(valueBytes));
            break;

        default:
            //: Translation: variant map = data structure for storing meta data
            raiseError(tr("Invalid variant map entry type"));
            return {};
        }
    }

    if (fieldTypeArray.size() != 1) {
        //: Translation: variant map = data structure for storing meta data
        raiseError(tr("Invalid variant map field type size"));
        return {};
    }

    return vm;
}

/**
 * @return mapping from attachment keys to binary data
 */
QHash<QString, QByteArray> Kdbx4Reader::binaryPool() const
{
    return m_binaryPool;
}
