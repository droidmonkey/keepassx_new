/*
 *  Copyright (C) 2023 Felix Geyer <debfx@fobos.de>
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

#include "KeePass1Reader.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtCore5Compat/QTextCodec>

#include "core/Endian.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "crypto/CryptoHash.h"
#include "format/KeePass1.h"
#include "keys/FileKey.h"
#include "streams/SymmetricCipherStream.h"

class KeePass1Key : public CompositeKey
{
public:
    QByteArray rawKey() const override;
    virtual void clear();
    void setPassword(const QByteArray& password);
    void setKeyfileData(const QByteArray& keyfileData);

private:
    QByteArray m_password;
    QByteArray m_keyfileData;
};

KeePass1Reader::KeePass1Reader()
    : m_tmpParent(nullptr)
    , m_device(nullptr)
    , m_encryptionFlags(0)
    , m_transformRounds(0)
    , m_error(false)
{
}

QSharedPointer<Database>
KeePass1Reader::readDatabase(QIODevice* device, const QString& password, QIODevice* keyfileDevice)
{
    m_error = false;
    m_errorStr.clear();

    QByteArray keyfileData;
    auto newFileKey = QSharedPointer<FileKey>::create();

    if (keyfileDevice) {
        keyfileData = readKeyfile(keyfileDevice);

        if (keyfileData.isEmpty()) {
            raiseError(tr("Unable to read keyfile.").append("\n").append(keyfileDevice->errorString()));
            return {};
        }
        if (!keyfileDevice->seek(0)) {
            raiseError(tr("Unable to read keyfile.").append("\n").append(keyfileDevice->errorString()));
            return {};
        }

        if (!newFileKey->load(keyfileDevice)) {
            raiseError(tr("Unable to read keyfile.").append("\n").append(keyfileDevice->errorString()));
            return {};
        }
    }

    auto db = QSharedPointer<Database>::create();
    QScopedPointer<Group> tmpParent(new Group());
    m_db = db;
    m_tmpParent = tmpParent.data();
    m_device = device;

    bool ok;

    auto signature1 = Endian::readSizedInt<quint32>(m_device, KeePass1::BYTEORDER, &ok);
    if (!ok || signature1 != KeePass1::SIGNATURE_1) {
        raiseError(tr("Not a KeePass database."));
        return {};
    }

    auto signature2 = Endian::readSizedInt<quint32>(m_device, KeePass1::BYTEORDER, &ok);
    if (!ok || signature2 != KeePass1::SIGNATURE_2) {
        raiseError(tr("Not a KeePass database."));
        return {};
    }

    m_encryptionFlags = Endian::readSizedInt<quint32>(m_device, KeePass1::BYTEORDER, &ok);
    if (!ok || !(m_encryptionFlags & KeePass1::Rijndael || m_encryptionFlags & KeePass1::Twofish)) {
        raiseError(tr("Unsupported encryption algorithm."));
        return {};
    }

    auto version = Endian::readSizedInt<quint32>(m_device, KeePass1::BYTEORDER, &ok);
    if (!ok
        || (version & KeePass1::FILE_VERSION_CRITICAL_MASK)
               != (KeePass1::FILE_VERSION & KeePass1::FILE_VERSION_CRITICAL_MASK)) {
        raiseError(tr("Unsupported KeePass database version."));
        return {};
    }

    m_masterSeed = m_device->read(16);
    if (m_masterSeed.size() != 16) {
        raiseError("Unable to read master seed");
        return {};
    }

    m_encryptionIV = m_device->read(16);
    if (m_encryptionIV.size() != 16) {
        raiseError(tr("Unable to read encryption IV", "IV = Initialization Vector for symmetric cipher"));
        return {};
    }

    auto numGroups = Endian::readSizedInt<quint32>(m_device, KeePass1::BYTEORDER, &ok);
    if (!ok) {
        raiseError(tr("Invalid number of groups"));
        return {};
    }

    auto numEntries = Endian::readSizedInt<quint32>(m_device, KeePass1::BYTEORDER, &ok);
    if (!ok) {
        raiseError(tr("Invalid number of entries"));
        return {};
    }

    m_contentHashHeader = m_device->read(32);
    if (m_contentHashHeader.size() != 32) {
        raiseError(tr("Invalid content hash size"));
        return {};
    }

    m_transformSeed = m_device->read(32);
    if (m_transformSeed.size() != 32) {
        raiseError(tr("Invalid transform seed size"));
        return {};
    }

    m_transformRounds = Endian::readSizedInt<quint32>(m_device, KeePass1::BYTEORDER, &ok);
    if (!ok) {
        raiseError(tr("Invalid number of transform rounds"));
        return {};
    }
    auto kdf = QSharedPointer<AesKdf>::create(true);
    kdf->setRounds(m_transformRounds);
    kdf->setSeed(m_transformSeed);
    db->setKdf(kdf);

    qint64 contentPos = m_device->pos();

    QScopedPointer<SymmetricCipherStream> cipherStream(testKeys(password, keyfileData, contentPos));

    if (!cipherStream) {
        return {};
    }

    QList<Group*> groups;
    for (quint32 i = 0; i < numGroups; i++) {
        Group* group = readGroup(cipherStream.data());
        if (!group) {
            return {};
        }
        groups.append(group);
    }

    QList<Entry*> entries;
    for (quint32 i = 0; i < numEntries; i++) {
        Entry* entry = readEntry(cipherStream.data());
        if (!entry) {
            return {};
        }
        entries.append(entry);
    }

    if (!constructGroupTree(groups)) {
        raiseError(tr("Unable to construct group tree"));
        return {};
    }

    for (Entry* entry : asConst(entries)) {
        if (isMetaStream(entry)) {
            parseMetaStream(entry);

            delete entry;
        } else {
            quint32 groupId = m_entryGroupIds.value(entry);
            if (!m_groupIds.contains(groupId)) {
                qWarning("Orphaned entry found, assigning to root group.");
                entry->setGroup(m_db->rootGroup());
            } else {
                entry->setGroup(m_groupIds.value(groupId));
            }
            entry->setUuid(QUuid::createUuid());
        }
    }

    db->rootGroup()->setName(tr("Root"));

    const QList<Group*> children = db->rootGroup()->children();
    for (Group* group : children) {
        if (group->name() == "Backup") {
            group->setSearchingEnabled(Group::Disable);
            group->setAutoTypeEnabled(Group::Disable);
        }
    }

    Q_ASSERT(m_tmpParent->children().isEmpty());
    Q_ASSERT(m_tmpParent->entries().isEmpty());

    for (Group* group : asConst(groups)) {
        group->setUpdateTimeinfo(true);
    }

    const QList<Entry*> dbEntries = m_db->rootGroup()->entriesRecursive();
    for (Entry* entry : dbEntries) {
        entry->setUpdateTimeinfo(true);
    }

    auto key = QSharedPointer<CompositeKey>::create();
    if (!password.isEmpty()) {
        key->addKey(QSharedPointer<PasswordKey>::create(password));
    }
    if (keyfileDevice) {
        key->addKey(newFileKey);
    }

    if (!db->setKey(key)) {
        raiseError(tr("Unable to calculate database key"));
        return {};
    }

    return db;
}

QSharedPointer<Database>
KeePass1Reader::readDatabase(QIODevice* device, const QString& password, const QString& keyfileName)
{
    QScopedPointer<QFile> keyFile;
    if (!keyfileName.isEmpty()) {
        keyFile.reset(new QFile(keyfileName));
        if (!keyFile->open(QFile::ReadOnly)) {
            raiseError(keyFile->errorString());
            return {};
        }
    }

    return QSharedPointer<Database>(readDatabase(device, password, keyFile.data()));
}

QSharedPointer<Database>
KeePass1Reader::readDatabase(const QString& filename, const QString& password, const QString& keyfileName)
{
    QFile dbFile(filename);
    if (!dbFile.open(QFile::ReadOnly)) {
        raiseError(dbFile.errorString());
        return {};
    }

    auto db = readDatabase(&dbFile, password, keyfileName);

    if (dbFile.error() != QFile::NoError) {
        raiseError(dbFile.errorString());
        return {};
    }

    if (db) {
        db->metadata()->setName(QFileInfo(filename).completeBaseName());
    }

    return db;
}

bool KeePass1Reader::hasError()
{
    return m_error;
}

QString KeePass1Reader::errorString()
{
    return m_errorStr;
}

SymmetricCipherStream*
KeePass1Reader::testKeys(const QString& password, const QByteArray& keyfileData, qint64 contentPos)
{
    const QList<PasswordEncoding> encodings = {Windows1252, Latin1, UTF8};

    QScopedPointer<SymmetricCipherStream> cipherStream;
    QByteArray passwordData;
    QTextCodec* codec = QTextCodec::codecForName("Windows-1252");
    QByteArray passwordDataCorrect = codec->fromUnicode(password);

    for (PasswordEncoding encoding : encodings) {
        if (encoding == Windows1252) {
            passwordData = passwordDataCorrect;
        } else if (encoding == Latin1) {
            // KeePassX used Latin-1 encoding for passwords until version 0.3.1
            // but KeePass/Win32 uses Windows Codepage 1252.
            passwordData = password.toLatin1();

            if (passwordData == passwordDataCorrect) {
                continue;
            } else {
                qWarning("Testing password encoded as Latin-1.");
            }
        } else if (encoding == UTF8) {
            // KeePassX used UTF-8 encoding for passwords until version 0.2.2
            // but KeePass/Win32 uses Windows Codepage 1252.
            passwordData = password.toUtf8();

            if (passwordData == passwordDataCorrect) {
                continue;
            } else {
                qWarning("Testing password encoded as UTF-8.");
            }
        }

        QByteArray finalKey = key(passwordData, keyfileData);
        if (finalKey.isEmpty()) {
            return nullptr;
        }

        cipherStream.reset(new SymmetricCipherStream(m_device));

        auto mode = SymmetricCipher::Aes256_CBC;
        if (m_encryptionFlags & KeePass1::Twofish) {
            mode = SymmetricCipher::Twofish_CBC;
        }
        if (!cipherStream->init(mode, SymmetricCipher::Decrypt, finalKey, m_encryptionIV)) {
            raiseError(cipherStream->errorString());
            return nullptr;
        }
        if (!cipherStream->open(QIODevice::ReadOnly)) {
            raiseError(cipherStream->errorString());
            return nullptr;
        }

        bool success = verifyKey(cipherStream.data());

        cipherStream->reset();
        cipherStream->close();
        if (!m_device->seek(contentPos)) {
            QString msg = tr("unable to seek to content position");
            if (!m_device->errorString().isEmpty()) {
                msg.append("\n").append(m_device->errorString());
            }
            raiseError(msg);

            return nullptr;
        }

        if (success) {
            if (!cipherStream->init(mode, SymmetricCipher::Decrypt, finalKey, m_encryptionIV)) {
                raiseError(cipherStream->errorString());
                return nullptr;
            }
            cipherStream->open(QIODevice::ReadOnly);
            break;
        } else {
            cipherStream.reset();
        }
    }

    if (!cipherStream) {
        raiseError(tr("Invalid credentials were provided, please try again.\n"
                      "If this reoccurs, then your database file may be corrupt."));
    }

    return cipherStream.take();
}

QByteArray KeePass1Reader::key(const QByteArray& password, const QByteArray& keyfileData)
{
    Q_ASSERT(!m_masterSeed.isEmpty());
    Q_ASSERT(!m_transformSeed.isEmpty());

    KeePass1Key key;
    key.setPassword(password);
    key.setKeyfileData(keyfileData);

    QByteArray transformedKey;
    bool result = key.transform(*m_db->kdf(), transformedKey);

    if (!result) {
        raiseError(tr("Key transformation failed"));
        return {};
    }

    CryptoHash hash(CryptoHash::Sha256);
    hash.addData(m_masterSeed);
    hash.addData(transformedKey);
    return hash.result();
}

bool KeePass1Reader::verifyKey(SymmetricCipherStream* cipherStream)
{
    CryptoHash contentHash(CryptoHash::Sha256);
    QByteArray buffer;

    do {
        if (!Tools::readFromDevice(cipherStream, buffer)) {
            return false;
        }
        contentHash.addData(buffer);
    } while (!buffer.isEmpty());

    return contentHash.result() == m_contentHashHeader;
}

Group* KeePass1Reader::readGroup(QIODevice* cipherStream)
{
    QScopedPointer<Group> group(new Group());
    group->setUpdateTimeinfo(false);
    group->setParent(m_tmpParent);

    TimeInfo timeInfo;

    quint32 groupId = 0;
    quint32 groupLevel = 0;
    bool groupIdSet = false;
    bool groupLevelSet = false;

    bool ok;
    bool reachedEnd = false;

    do {
        auto fieldType = Endian::readSizedInt<quint16>(cipherStream, KeePass1::BYTEORDER, &ok);
        if (!ok) {
            raiseError(tr("Invalid group field type number"));
            return nullptr;
        }

        auto fieldSize = static_cast<int>(Endian::readSizedInt<quint32>(cipherStream, KeePass1::BYTEORDER, &ok));
        if (!ok) {
            raiseError(tr("Invalid group field size"));
            return nullptr;
        }

        QByteArray fieldData = cipherStream->read(fieldSize);
        if (fieldData.size() != fieldSize) {
            raiseError(tr("Read group field data doesn't match size"));
            return nullptr;
        }

        switch (fieldType) {
        case 0x0000:
            // ignore field
            break;
        case 0x0001:
            if (fieldSize != 4) {
                raiseError(tr("Incorrect group id field size"));
                return nullptr;
            }
            groupId = Endian::bytesToSizedInt<quint32>(fieldData, KeePass1::BYTEORDER);
            groupIdSet = true;
            break;
        case 0x0002:
            group->setName(QString::fromUtf8(fieldData.toStdString().c_str()));
            break;
        case 0x0003: {
            if (fieldSize != 5) {
                raiseError(tr("Incorrect group creation time field size"));
                return nullptr;
            }
            QDateTime dateTime = dateFromPackedStruct(fieldData);
            if (dateTime.isValid()) {
                timeInfo.setCreationTime(dateTime);
            }
            break;
        }
        case 0x0004: {
            if (fieldSize != 5) {
                raiseError(tr("Incorrect group modification time field size"));
                return nullptr;
            }
            QDateTime dateTime = dateFromPackedStruct(fieldData);
            if (dateTime.isValid()) {
                timeInfo.setLastModificationTime(dateTime);
            }
            break;
        }
        case 0x0005: {
            if (fieldSize != 5) {
                raiseError(tr("Incorrect group access time field size"));
            }
            QDateTime dateTime = dateFromPackedStruct(fieldData);
            if (dateTime.isValid()) {
                timeInfo.setLastAccessTime(dateTime);
            }
            break;
        }
        case 0x0006: {
            if (fieldSize != 5) {
                raiseError(tr("Incorrect group expiry time field size"));
            }
            QDateTime dateTime = dateFromPackedStruct(fieldData);
            if (dateTime.isValid()) {
                timeInfo.setExpires(true);
                timeInfo.setExpiryTime(dateTime);
            }
            break;
        }
        case 0x0007: {
            if (fieldSize != 4) {
                raiseError(tr("Incorrect group icon field size"));
                return nullptr;
            }
            auto iconNumber = Endian::bytesToSizedInt<quint32>(fieldData, KeePass1::BYTEORDER);
            group->setIcon(iconNumber);
            break;
        }
        case 0x0008: {
            if (fieldSize != 2) {
                raiseError(tr("Incorrect group level field size"));
                return nullptr;
            }
            groupLevel = Endian::bytesToSizedInt<quint16>(fieldData, KeePass1::BYTEORDER);
            groupLevelSet = true;
            break;
        }
        case 0x0009:
            // flags, ignore field
            break;
        case 0xFFFF:
            reachedEnd = true;
            break;
        default:
            // invalid field
            raiseError(tr("Invalid group field type"));
            return nullptr;
        }
    } while (!reachedEnd);

    if (!groupIdSet || !groupLevelSet) {
        raiseError(tr("Missing group id or level"));
        return nullptr;
    }

    group->setUuid(QUuid::createUuid());
    group->setTimeInfo(timeInfo);
    m_groupIds.insert(groupId, group.data());
    m_groupLevels.insert(group.data(), groupLevel);

    return group.take();
}

Entry* KeePass1Reader::readEntry(QIODevice* cipherStream)
{
    QScopedPointer<Entry> entry(new Entry());
    entry->setUpdateTimeinfo(false);
    entry->setGroup(m_tmpParent);

    TimeInfo timeInfo;
    QString binaryName;
    bool ok;
    bool reachedEnd = false;

    do {
        auto fieldType = Endian::readSizedInt<quint16>(cipherStream, KeePass1::BYTEORDER, &ok);
        if (!ok) {
            raiseError(tr("Missing entry field type number"));
            return nullptr;
        }

        auto fieldSize = static_cast<int>(Endian::readSizedInt<quint32>(cipherStream, KeePass1::BYTEORDER, &ok));
        if (!ok) {
            raiseError(tr("Invalid entry field size"));
            return nullptr;
        }

        QByteArray fieldData = cipherStream->read(fieldSize);
        if (fieldData.size() != fieldSize) {
            raiseError(tr("Read entry field data doesn't match size"));
            return nullptr;
        }

        switch (fieldType) {
        case 0x0000:
            // ignore field
            break;
        case 0x0001:
            if (fieldSize != 16) {
                raiseError(tr("Invalid entry UUID field size"));
                return nullptr;
            }
            m_entryUuids.insert(fieldData, entry.data());
            break;
        case 0x0002: {
            if (fieldSize != 4) {
                raiseError(tr("Invalid entry group id field size"));
                return nullptr;
            }
            auto groupId = Endian::bytesToSizedInt<quint32>(fieldData, KeePass1::BYTEORDER);
            m_entryGroupIds.insert(entry.data(), groupId);
            break;
        }
        case 0x0003: {
            if (fieldSize != 4) {
                raiseError(tr("Invalid entry icon field size"));
                return nullptr;
            }
            auto iconNumber = Endian::bytesToSizedInt<quint32>(fieldData, KeePass1::BYTEORDER);
            entry->setIcon(iconNumber);
            break;
        }
        case 0x0004:
            entry->setTitle(QString::fromUtf8(fieldData.toStdString().c_str()));
            break;
        case 0x0005:
            entry->setUrl(QString::fromUtf8(fieldData.toStdString().c_str()));
            break;
        case 0x0006:
            entry->setUsername(QString::fromUtf8(fieldData.toStdString().c_str()));
            break;
        case 0x0007:
            entry->setPassword(QString::fromUtf8(fieldData.toStdString().c_str()));
            break;
        case 0x0008:
            parseNotes(QString::fromUtf8(fieldData.toStdString().c_str()), entry.data());
            break;
        case 0x0009: {
            if (fieldSize != 5) {
                raiseError(tr("Invalid entry creation time field size"));
                return nullptr;
            }
            QDateTime dateTime = dateFromPackedStruct(fieldData);
            if (dateTime.isValid()) {
                timeInfo.setCreationTime(dateTime);
            }
            break;
        }
        case 0x000A: {
            if (fieldSize != 5) {
                raiseError(tr("Invalid entry modification time field size"));
                return nullptr;
            }
            QDateTime dateTime = dateFromPackedStruct(fieldData);
            if (dateTime.isValid()) {
                timeInfo.setLastModificationTime(dateTime);
            }
            break;
        }
        case 0x000B: {
            if (fieldSize != 5) {
                raiseError(tr("Invalid entry creation time field size"));
                return nullptr;
            }
            QDateTime dateTime = dateFromPackedStruct(fieldData);
            if (dateTime.isValid()) {
                timeInfo.setLastAccessTime(dateTime);
            }
            break;
        }
        case 0x000C: {
            if (fieldSize != 5) {
                raiseError(tr("Invalid entry expiry time field size"));
                return nullptr;
            }
            QDateTime dateTime = dateFromPackedStruct(fieldData);
            if (dateTime.isValid()) {
                timeInfo.setExpires(true);
                timeInfo.setExpiryTime(dateTime);
            }
            break;
        }
        case 0x000D:
            binaryName = QString::fromUtf8(fieldData.toStdString().c_str());
            break;
        case 0x000E:
            if (fieldSize != 0) {
                entry->attachments()->set(binaryName, fieldData);
            }
            break;
        case 0xFFFF:
            reachedEnd = true;
            break;
        default:
            // invalid field
            raiseError(tr("Invalid entry field type"));
            return nullptr;
        }
    } while (!reachedEnd);

    entry->setTimeInfo(timeInfo);

    return entry.take();
}

void KeePass1Reader::parseNotes(const QString& rawNotes, Entry* entry)
{
    QRegularExpression sequenceRegexp("Auto-Type(?:-(\\d+))?: (.+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression windowRegexp("Auto-Type-Window(?:-(\\d+))?: (.+)", QRegularExpression::CaseInsensitiveOption);
    QHash<int, QString> sequences;
    QMap<int, QStringList> windows;

    QStringList notes;

    bool lastLineAutoType = false;
    const QStringList rawNotesLines = rawNotes.split("\n");
    for (QString line : rawNotesLines) {
        line.remove("\r");

        auto sequenceMatch = sequenceRegexp.match(line);
        auto windowMatch = windowRegexp.match(line);
        if (sequenceMatch.hasMatch()) {
            if (sequenceMatch.captured(1).isEmpty()) {
                entry->setDefaultAutoTypeSequence(sequenceMatch.captured(2));
            } else {
                sequences[sequenceMatch.captured(1).toInt()] = sequenceMatch.captured(2);
            }

            lastLineAutoType = true;
        } else if (windowMatch.hasMatch()) {
            int nr;
            if (windowMatch.captured(1).isEmpty()) {
                nr = -1; // special number that matches no other sequence
            } else {
                nr = windowMatch.captured(1).toInt();
            }

            windows[nr].append(windowMatch.captured(2));

            lastLineAutoType = true;
        } else {
            // don't add empty lines following a removed auto-type line
            if (!lastLineAutoType || !line.isEmpty()) {
                notes.append(line);
            }
            lastLineAutoType = false;
        }
    }

    entry->setNotes(notes.join("\n"));

    QMapIterator<int, QStringList> i(windows);
    while (i.hasNext()) {
        i.next();

        QString sequence = sequences.value(i.key());
        const QStringList windowList = i.value();

        for (const QString& window : windowList) {
            AutoTypeAssociations::Association assoc;
            assoc.window = window;
            assoc.sequence = sequence;
            entry->autoTypeAssociations()->add(assoc);
        }
    }
}

bool KeePass1Reader::constructGroupTree(const QList<Group*>& groups)
{
    for (int i = 0; i < groups.size(); i++) {
        quint32 level = m_groupLevels.value(groups[i]);

        if (level == 0) {
            groups[i]->setParent(m_db->rootGroup());
        } else {
            for (int j = (i - 1); j >= 0; j--) {
                if (m_groupLevels.value(groups[j]) < level) {
                    if ((level - m_groupLevels.value(groups[j])) != 1) {
                        return false;
                    }

                    groups[i]->setParent(groups[j]);
                    break;
                }
            }
        }

        if (groups[i]->parentGroup() == m_tmpParent) {
            return false;
        }
    }

    return true;
}

void KeePass1Reader::parseMetaStream(const Entry* entry)
{
    QByteArray data = entry->attachments()->value("bin-stream");

    if (entry->notes() == "KPX_GROUP_TREE_STATE") {
        if (!parseGroupTreeState(data)) {
            qWarning("Unable to parse group tree state metastream.");
        }
    } else if (entry->notes() == "KPX_CUSTOM_ICONS_4") {
        if (!parseCustomIcons4(data)) {
            qWarning("Unable to parse custom icons metastream.");
        }
    } else {
        qWarning("Ignoring unknown metastream \"%s\".", entry->notes().toLocal8Bit().constData());
    }
}

bool KeePass1Reader::parseGroupTreeState(const QByteArray& data)
{
    if (data.size() < 4) {
        return false;
    }

    int pos = 0;
    auto num = Endian::bytesToSizedInt<quint32>(data.mid(pos, 4), KeePass1::BYTEORDER);
    pos += 4;

    if (static_cast<quint32>(data.size() - 4) != (num * 5)) {
        return false;
    }

    for (quint32 i = 0; i < num; i++) {
        auto groupId = Endian::bytesToSizedInt<quint32>(data.mid(pos, 4), KeePass1::BYTEORDER);
        pos += 4;

        bool expanded = data.at(pos);
        pos += 1;

        if (m_groupIds.contains(groupId)) {
            m_groupIds[groupId]->setExpanded(expanded);
        }
    }

    return true;
}

bool KeePass1Reader::parseCustomIcons4(const QByteArray& data)
{
    if (data.size() < 12) {
        return false;
    }

    int pos = 0;

    auto numIcons = Endian::bytesToSizedInt<quint32>(data.mid(pos, 4), KeePass1::BYTEORDER);
    pos += 4;

    auto numEntries = Endian::bytesToSizedInt<quint32>(data.mid(pos, 4), KeePass1::BYTEORDER);
    pos += 4;

    auto numGroups = Endian::bytesToSizedInt<quint32>(data.mid(pos, 4), KeePass1::BYTEORDER);
    pos += 4;

    QList<QUuid> iconUuids;

    for (quint32 i = 0; i < numIcons; i++) {
        if (data.size() < (pos + 4)) {
            return false;
        }
        auto iconSize = Endian::bytesToSizedInt<quint32>(data.mid(pos, 4), KeePass1::BYTEORDER);
        pos += 4;

        if (static_cast<quint32>(data.size()) < (pos + iconSize)) {
            return false;
        }
        QByteArray icon = data.mid(pos, iconSize);
        pos += iconSize;

        QUuid uuid = QUuid::createUuid();
        iconUuids.append(uuid);
        m_db->metadata()->addCustomIcon(uuid, icon);
    }

    if (static_cast<quint32>(data.size()) < (pos + numEntries * 20)) {
        return false;
    }

    for (quint32 i = 0; i < numEntries; i++) {
        QByteArray entryUuid = data.mid(pos, 16);
        pos += 16;

        auto iconId = Endian::bytesToSizedInt<quint32>(data.mid(pos, 4), KeePass1::BYTEORDER);
        pos += 4;

        if (m_entryUuids.contains(entryUuid) && (iconId < static_cast<quint32>(iconUuids.size()))) {
            m_entryUuids[entryUuid]->setIcon(iconUuids[iconId]);
        }
    }

    if (static_cast<quint32>(data.size()) < (pos + numGroups * 8)) {
        return false;
    }

    for (quint32 i = 0; i < numGroups; i++) {
        auto groupId = Endian::bytesToSizedInt<quint32>(data.mid(pos, 4), KeePass1::BYTEORDER);
        pos += 4;

        auto iconId = Endian::bytesToSizedInt<quint32>(data.mid(pos, 4), KeePass1::BYTEORDER);
        pos += 4;

        if (m_groupIds.contains(groupId) && (iconId < static_cast<quint32>(iconUuids.size()))) {
            m_groupIds[groupId]->setIcon(iconUuids[iconId]);
        }
    }

    return true;
}

void KeePass1Reader::raiseError(const QString& errorMessage)
{
    m_error = true;
    m_errorStr = errorMessage;
}

QDateTime KeePass1Reader::dateFromPackedStruct(const QByteArray& data)
{
    Q_ASSERT(data.size() == 5);

    quint32 dw1 = static_cast<uchar>(data.at(0));
    quint32 dw2 = static_cast<uchar>(data.at(1));
    quint32 dw3 = static_cast<uchar>(data.at(2));
    quint32 dw4 = static_cast<uchar>(data.at(3));
    quint32 dw5 = static_cast<uchar>(data.at(4));

    int y = (dw1 << 6) | (dw2 >> 2);
    int mon = ((dw2 & 0x00000003) << 2) | (dw3 >> 6);
    int d = (dw3 >> 1) & 0x0000001F;
    int h = ((dw3 & 0x00000001) << 4) | (dw4 >> 4);
    int min = ((dw4 & 0x0000000F) << 2) | (dw5 >> 6);
    int s = dw5 & 0x0000003F;

    QDateTime dateTime = QDateTime(QDate(y, mon, d), QTime(h, min, s), Qt::UTC);

    // check for the special "never" datetime
    if (dateTime == QDateTime(QDate(2999, 12, 28), QTime(23, 59, 59), Qt::UTC)) {
        return {};
    } else {
        return dateTime;
    }
}

bool KeePass1Reader::isMetaStream(const Entry* entry)
{
    return entry->attachments()->keys().contains("bin-stream") && !entry->notes().isEmpty()
           && entry->title() == "Meta-Info" && entry->username() == "SYSTEM" && entry->url() == "$"
           && entry->iconNumber() == 0;
}

QByteArray KeePass1Reader::readKeyfile(QIODevice* device)
{
    if (device->size() == 0) {
        return {};
    }

    if (device->size() == 32) {
        QByteArray data = device->read(32);
        if (data.size() != 32) {
            return {};
        }

        return data;
    }

    if (device->size() == 64) {
        QByteArray data = device->read(64);

        if (data.size() != 64) {
            return {};
        }

        if (Tools::isHex(data)) {
            return QByteArray::fromHex(data);
        } else {
            device->seek(0);
        }
    }

    CryptoHash cryptoHash(CryptoHash::Sha256);
    QByteArray buffer;

    do {
        if (!Tools::readFromDevice(device, buffer)) {
            return {};
        }
        cryptoHash.addData(buffer);
    } while (!buffer.isEmpty());

    return cryptoHash.result();
}

QByteArray KeePass1Key::rawKey() const
{
    if (m_keyfileData.isEmpty()) {
        return CryptoHash::hash(m_password, CryptoHash::Sha256);
    } else if (m_password.isEmpty()) {
        return m_keyfileData;
    } else {
        CryptoHash keyHash(CryptoHash::Sha256);
        keyHash.addData(CryptoHash::hash(m_password, CryptoHash::Sha256));
        keyHash.addData(m_keyfileData);
        return keyHash.result();
    }
}

void KeePass1Key::clear()
{
    CompositeKey::clear();

    m_password.clear();
    m_keyfileData.clear();
}

void KeePass1Key::setPassword(const QByteArray& password)
{
    m_password = password;
}

void KeePass1Key::setKeyfileData(const QByteArray& keyfileData)
{
    m_keyfileData = keyfileData;
}
