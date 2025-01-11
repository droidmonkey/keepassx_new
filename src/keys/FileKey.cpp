/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
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

#include "FileKey.h"

#include "core/Tools.h"
#include "crypto/CryptoHash.h"
#include "crypto/Random.h"

#include <QDataStream>
#include <QFile>
#include <QXmlStreamReader>

QUuid FileKey::UUID("a584cbc4-c9b4-437e-81bb-362ca9709273");

constexpr int FileKey::SHA256_SIZE;

FileKey::FileKey()
    : Key(UUID)
    , m_key(SHA256_SIZE)
{
}

/**
 * Read key file from device while trying to detect its file format.
 *
 * If no legacy key file format was detected, the SHA-256 hash of the
 * key file will be used, allowing usage of arbitrary files as key files.
 * In case of a detected legacy key file format, the raw byte contents
 * will be extracted from the file.
 *
 * Supported legacy formats are:
 *  - KeePass 2 XML key file
 *  - Fixed 32 byte binary
 *  - Fixed 32 byte ASCII hex-encoded binary
 *
 * Usage of legacy formats is discouraged and support for them may be
 * removed in a future version.
 *
 * @param device input device
 * @param errorMsg error message in case of fatal failure
 * @return true if key file was loaded successfully
 */
bool FileKey::load(QIODevice* device, QString* errorMsg)
{
    m_type = None;

    // we may need to read the file multiple times
    if (device->isSequential()) {
        return false;
    }

    if (device->size() == 0 || !device->reset()) {
        return false;
    }

    // load XML key file v1 or v2
    QString xmlError;
    if (loadXml(device, &xmlError)) {
        return true;
    }

    if (!device->reset() || !xmlError.isEmpty()) {
        if (errorMsg) {
            *errorMsg = xmlError;
        }
        return false;
    }

    // try legacy key file formats
    if (loadBinary(device)) {
        return true;
    }

    if (!device->reset()) {
        return false;
    }

    if (loadHex(device)) {
        return true;
    }

    // if no legacy format was detected, generate SHA-256 hash of key file
    if (!device->reset()) {
        return false;
    }
    if (loadHashed(device)) {
        return true;
    }

    return false;
}

/**
 * Load key file from path while trying to detect its file format.
 *
 * If no legacy key file format was detected, the SHA-256 hash of the
 * key file will be used, allowing usage of arbitrary files as key files.
 * In case of a detected legacy key file format, the raw byte contents
 * will be extracted from the file.
 *
 * Supported legacy formats are:
 *  - KeePass 2 XML key file
 *  - Fixed 32 byte binary
 *  - Fixed 32 byte ASCII hex-encoded binary
 *
 * Usage of legacy formats is discouraged and support for them may be
 * removed in a future version.
 *
 * @param fileName input file name
 * @param errorMsg error message if loading failed
 * @return true if key file was loaded successfully
 */
bool FileKey::load(const QString& fileName, QString* errorMsg)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly)) {
        if (errorMsg) {
            *errorMsg = file.errorString();
        }
        return false;
    }

    bool result = load(&file, errorMsg);
    file.close();

    if (errorMsg && !errorMsg->isEmpty()) {
        return false;
    }

    if (file.error()) {
        result = false;
        if (errorMsg) {
            *errorMsg = file.errorString();
        }
    } else {
        // Store the file path for serialization
        m_file = fileName;
    }

    return result;
}

/**
 * @return key data as bytes
 */
QByteArray FileKey::rawKey() const
{
    return {m_key.data(), int(m_key.size())};
}

void FileKey::setRawKey(const QByteArray& data)
{
    Q_ASSERT(data.size() == SHA256_SIZE);
    m_key.assign(data.begin(), data.end());
}

QByteArray FileKey::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << uuid().toRfc4122() << rawKey() << static_cast<qint32>(m_type) << m_file;
    return data;
}

void FileKey::deserialize(const QByteArray& data)
{
    QDataStream stream(data);
    QByteArray uuidData;
    stream >> uuidData;
    if (uuid().toRfc4122() == uuidData) {
        QByteArray key;
        qint32 type;
        stream >> key >> type >> m_file;

        setRawKey(key);
        m_type = static_cast<Type>(type);
    }
}

/**
 * Generate a new key file with random bytes.
 *
 * @param device output device
 * @param number of random bytes to generate
 */
void FileKey::createRandom(QIODevice* device, int size)
{
    device->write(randomGen()->randomArray(size));
}

/**
 * Generate a new key file in the KeePass2 XML format v2.
 *
 * @param device output device
 * @param number of random bytes to generate
 */
void FileKey::createXMLv2(QIODevice* device, int size)
{
    QXmlStreamWriter w(device);
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(4);
    w.writeStartDocument();

    w.writeStartElement("KeyFile");

    w.writeStartElement("Meta");
    w.writeTextElement("Version", "2.0");
    w.writeEndElement();

    w.writeStartElement("Key");
    w.writeStartElement("Data");

    QByteArray key = randomGen()->randomArray(size);
    CryptoHash hash(CryptoHash::Sha256);
    hash.addData(key);
    QByteArray result = hash.result().left(4);
    key = key.toHex().toUpper();

    w.writeAttribute("Hash", result.toHex().toUpper());
    w.writeCharacters("\n            ");
    for (int i = 0; i < key.size(); ++i) {
        // Pretty-print hex value (not strictly necessary, but nicer to read and KeePass2 does it)
        if (i != 0 && i % 32 == 0) {
            w.writeCharacters("\n            ");
        } else if (i != 0 && i % 8 == 0) {
            w.writeCharacters(" ");
        }
        w.writeCharacters(QChar(key[i]));
    }
    Botan::secure_scrub_memory(key.data(), static_cast<std::size_t>(key.capacity()));
    w.writeCharacters("\n        ");

    w.writeEndElement();
    w.writeEndElement();

    w.writeEndDocument();
}

/**
 * Create a new key file from random bytes.
 *
 * @param fileName output file name
 * @param errorMsg error message if generation failed
 * @param number of random bytes to generate
 * @return true on successful creation
 */
bool FileKey::create(const QString& fileName, QString* errorMsg)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly)) {
        if (errorMsg) {
            *errorMsg = file.errorString();
        }
        return false;
    }
    if (fileName.endsWith(".keyx")) {
        createXMLv2(&file);
    } else {
        createRandom(&file);
    }
    file.close();
    file.setPermissions(QFile::ReadUser);

    if (file.error()) {
        if (errorMsg) {
            *errorMsg = file.errorString();
        }

        return false;
    }

    return true;
}

/**
 * Load key file in legacy KeePass 2 XML format.
 *
 * @param device input device
 * @return true on success
 */
bool FileKey::loadXml(QIODevice* device, QString* errorMsg)
{
    QXmlStreamReader xmlReader(device);

    if (xmlReader.error()) {
        return false;
    }
    if (xmlReader.readNextStartElement() && xmlReader.name().toString() != "KeyFile") {
        return false;
    }

    struct
    {
        QString version;
        QByteArray hash;
        QByteArray data;
    } keyFileData;

    while (!xmlReader.error() && xmlReader.readNextStartElement()) {
        if (xmlReader.name().toString() == "Meta") {
            while (!xmlReader.error() && xmlReader.readNextStartElement()) {
                if (xmlReader.name().toString() == "Version") {
                    keyFileData.version = xmlReader.readElementText();
                    if (keyFileData.version.startsWith("1.0")) {
                        m_type = KeePass2XML;
                    } else if (keyFileData.version == "2.0") {
                        m_type = KeePass2XMLv2;
                    } else {
                        if (errorMsg) {
                            *errorMsg = QObject::tr("Unsupported key file version: %1").arg(keyFileData.version);
                        }
                        return false;
                    }
                }
            }
        } else if (xmlReader.name().toString() == "Key") {
            while (!xmlReader.error() && xmlReader.readNextStartElement()) {
                if (xmlReader.name().toString() == "Data") {
                    keyFileData.hash = QByteArray::fromHex(xmlReader.attributes().value("Hash").toLatin1());
                    keyFileData.data = xmlReader.readElementText().simplified().replace(" ", "").toLatin1();

                    if (keyFileData.version.startsWith("1.0") && Tools::isBase64(keyFileData.data)) {
                        keyFileData.data = QByteArray::fromBase64(keyFileData.data);
                    } else if (keyFileData.version == "2.0" && Tools::isHex(keyFileData.data)) {
                        keyFileData.data = QByteArray::fromHex(keyFileData.data);

                        CryptoHash hash(CryptoHash::Sha256);
                        hash.addData(keyFileData.data);
                        QByteArray result = hash.result().left(4);
                        if (keyFileData.hash != result) {
                            if (errorMsg) {
                                *errorMsg = QObject::tr("Checksum mismatch! Key file may be corrupt.");
                            }
                            return false;
                        }
                    } else {
                        if (errorMsg) {
                            *errorMsg = QObject::tr("Unexpected key file data! Key file may be corrupt.");
                        }
                        return false;
                    }
                }
            }
        }
    }

    bool ok = false;
    if (!xmlReader.error() && !keyFileData.data.isEmpty()) {
        std::memcpy(m_key.data(), keyFileData.data.data(), qMin(SHA256_SIZE, keyFileData.data.size()));
        ok = true;
    }

    Botan::secure_scrub_memory(keyFileData.data.data(), static_cast<std::size_t>(keyFileData.data.capacity()));

    return ok;
}

/**
 * Load fixed 32-bit binary key file.
 *
 * @param device input device
 * @return true on success
 * @deprecated
 */
bool FileKey::loadBinary(QIODevice* device)
{
    if (device->size() != 32) {
        return false;
    }

    Botan::secure_vector<char> data(32);
    if (device->read(data.data(), 32) != 32 || !device->atEnd()) {
        return false;
    }

    m_key = data;
    m_type = FixedBinary;
    return true;
}

/**
 * Load hex-encoded representation of fixed 32-bit binary key file.
 *
 * @param device input device
 * @return true on success
 * @deprecated
 */
bool FileKey::loadHex(QIODevice* device)
{
    if (device->size() != 64) {
        return false;
    }

    QByteArray data;
    if (!Tools::readAllFromDevice(device, data) || data.size() != 64) {
        return false;
    }

    if (!Tools::isHex(data)) {
        return false;
    }

    data = QByteArray::fromHex(data);
    if (data.size() != 32) {
        return false;
    }

    std::memcpy(m_key.data(), data.data(), qMin(SHA256_SIZE, data.size()));
    Botan::secure_scrub_memory(data.data(), static_cast<std::size_t>(data.capacity()));

    m_type = FixedBinaryHex;
    return true;
}

/**
 * Generate SHA-256 hash of arbitrary text or binary key file.
 *
 * @param device input device
 * @return true on success
 */
bool FileKey::loadHashed(QIODevice* device)
{
    CryptoHash cryptoHash(CryptoHash::Sha256);

    QByteArray buffer;
    do {
        if (!Tools::readFromDevice(device, buffer)) {
            return false;
        }
        cryptoHash.addData(buffer);
    } while (!buffer.isEmpty());

    buffer = cryptoHash.result();
    std::memcpy(m_key.data(), buffer.data(), qMin(SHA256_SIZE, buffer.size()));
    Botan::secure_scrub_memory(buffer.data(), static_cast<std::size_t>(buffer.capacity()));

    m_type = Hashed;
    return true;
}

/**
 * @return type of loaded key file
 */
FileKey::Type FileKey::type() const
{
    return m_type;
}
