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

#include "KdbxXmlWriter.h"

#include <QBuffer>
#include <QFile>
#include <QMap>

#include "core/Endian.h"
#include "crypto/CryptoHash.h"
#include "format/KeePass2RandomStream.h"
#include "streams/qtiocompressor.h"

/**
 * @param version KDBX version
 */
KdbxXmlWriter::KdbxXmlWriter(quint32 version)
    : m_kdbxVersion(version)
{
    Q_ASSERT_X(m_kdbxVersion < KeePass2::FILE_VERSION_4,
               "KDBX version",
               "KDBX version >= 4 requires explicit binary index map.");
}

KdbxXmlWriter::KdbxXmlWriter(quint32 version, KdbxXmlWriter::BinaryIdxMap binaryIdxMap)
    : m_kdbxVersion(version)
    , m_binaryIdxMap(std::move(binaryIdxMap))
{
}

void KdbxXmlWriter::writeDatabase(QIODevice* device,
                                  const Database* db,
                                  KeePass2RandomStream* randomStream,
                                  const QByteArray& headerHash)
{
    m_db = db;
    m_meta = db->metadata();
    m_randomStream = randomStream;
    m_headerHash = headerHash;

    m_xml.setAutoFormatting(true);
    m_xml.setAutoFormattingIndent(-1); // 1 tab
    m_xml.setCodec("UTF-8");

    if (m_kdbxVersion < KeePass2::FILE_VERSION_4) {
        fillBinaryIdxMap();
    }

    m_xml.setDevice(device);
    m_xml.writeStartDocument("1.0", true);
    m_xml.writeStartElement("KeePassFile");

    writeMetadata();
    writeRoot();

    m_xml.writeEndElement();
    m_xml.writeEndDocument();

    if (m_xml.hasError()) {
        raiseError(device->errorString());
    }
}

void KdbxXmlWriter::writeDatabase(const QString& filename, Database* db)
{
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    writeDatabase(&file, db);
}

bool KdbxXmlWriter::hasError()
{
    return m_error;
}

QString KdbxXmlWriter::errorString()
{
    return m_errorStr;
}

/**
 * Generate a map of entry attachments to deduplicated attachment index IDs.
 * This is basically duplicated code from Kdbx4Writer.cpp for KDBX 3 compatibility.
 * I don't have a good solution for getting rid of this duplication without getting rid of KDBX 3.
 */
void KdbxXmlWriter::fillBinaryIdxMap()
{
    const QList<Entry*> allEntries = m_db->rootGroup()->entriesRecursive(true);
    QHash<QByteArray, qint64> writtenAttachments;
    qint64 nextIdx = 0;

    for (Entry* entry : allEntries) {
        const QList<QString> attachmentKeys = entry->attachments()->keys();
        for (const QString& key : attachmentKeys) {
            QByteArray data = entry->attachments()->value(key);
            CryptoHash hash(CryptoHash::Sha256);
            // Namespace KeeShare attachments so they don't get deduplicated together with attachments
            // from other databases. Prevents potential filesize side channels.
            auto group = entry->group();
            if (!group && entry->historyOwner()) {
                group = entry->historyOwner()->group();
            }
            if (group && group->isShared()) {
                hash.addData(group->uuid().toByteArray());
            } else {
                hash.addData(m_db->uuid().toByteArray());
            }
            hash.addData(data);

            const auto hashResult = hash.result();
            if (!writtenAttachments.contains(hashResult)) {
                writtenAttachments.insert(hashResult, nextIdx++);
            }
            m_binaryIdxMap.insert(qMakePair(entry, key), writtenAttachments.value(hashResult));
        }
    }
}

void KdbxXmlWriter::writeMetadata()
{
    m_xml.writeStartElement("Meta");
    writeString("Generator", m_meta->generator());
    if (m_kdbxVersion < KeePass2::FILE_VERSION_4 && !m_headerHash.isEmpty()) {
        writeBinary("HeaderHash", m_headerHash);
    }
    writeString("DatabaseName", m_meta->name());
    writeDateTime("DatabaseNameChanged", m_meta->nameChanged());
    writeString("DatabaseDescription", m_meta->description());
    writeDateTime("DatabaseDescriptionChanged", m_meta->descriptionChanged());
    writeString("DefaultUserName", m_meta->defaultUserName());
    writeDateTime("DefaultUserNameChanged", m_meta->defaultUserNameChanged());
    writeNumber("MaintenanceHistoryDays", m_meta->maintenanceHistoryDays());
    writeString("Color", m_meta->color());
    writeDateTime("MasterKeyChanged", m_meta->databaseKeyChanged());
    writeNumber("MasterKeyChangeRec", m_meta->databaseKeyChangeRec());
    writeNumber("MasterKeyChangeForce", m_meta->databaseKeyChangeForce());
    writeMemoryProtection();
    writeCustomIcons();
    writeBool("RecycleBinEnabled", m_meta->recycleBinEnabled());
    writeUuid("RecycleBinUUID", m_meta->recycleBin());
    writeDateTime("RecycleBinChanged", m_meta->recycleBinChanged());
    writeUuid("EntryTemplatesGroup", m_meta->entryTemplatesGroup());
    writeDateTime("EntryTemplatesGroupChanged", m_meta->entryTemplatesGroupChanged());
    writeUuid("LastSelectedGroup", m_meta->lastSelectedGroup());
    writeUuid("LastTopVisibleGroup", m_meta->lastTopVisibleGroup());
    writeNumber("HistoryMaxItems", m_meta->historyMaxItems());
    writeNumber("HistoryMaxSize", m_meta->historyMaxSize());
    if (m_kdbxVersion >= KeePass2::FILE_VERSION_4) {
        writeDateTime("SettingsChanged", m_meta->settingsChanged());
    }
    if (m_kdbxVersion < KeePass2::FILE_VERSION_4) {
        writeBinaries();
    }
    writeCustomData(m_meta->customData(), true);

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeMemoryProtection()
{
    m_xml.writeStartElement("MemoryProtection");

    writeBool("ProtectTitle", m_meta->protectTitle());
    writeBool("ProtectUserName", m_meta->protectUsername());
    writeBool("ProtectPassword", m_meta->protectPassword());
    writeBool("ProtectURL", m_meta->protectUrl());
    writeBool("ProtectNotes", m_meta->protectNotes());

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeCustomIcons()
{
    m_xml.writeStartElement("CustomIcons");

    const QList<QUuid> customIconsOrder = m_meta->customIconsOrder();
    for (const QUuid& uuid : customIconsOrder) {
        writeIcon(uuid, m_meta->customIcon(uuid));
    }

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeIcon(const QUuid& uuid, const Metadata::CustomIconData& iconData)
{
    m_xml.writeStartElement("Icon");

    writeUuid("UUID", uuid);
    if (m_kdbxVersion >= KeePass2::FILE_VERSION_4_1) {
        if (!iconData.name.isEmpty()) {
            writeString("Name", iconData.name);
        }
        if (iconData.lastModified.isValid()) {
            writeDateTime("LastModificationTime", iconData.lastModified);
        }
    }
    writeBinary("Data", iconData.data);

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeBinaries()
{
    // Reverse binary index map
    QMap<qint64, QByteArray> binaries;
    for (auto i = m_binaryIdxMap.constBegin(); i != m_binaryIdxMap.constEnd(); ++i) {
        if (!binaries.contains(i.value())) {
            binaries.insert(i.value(), i.key().first->attachments()->value(i.key().second));
        }
    }

    m_xml.writeStartElement("Binaries");

    for (auto i = binaries.constBegin(); i != binaries.constEnd(); ++i) {
        m_xml.writeStartElement("Binary");
        m_xml.writeAttribute("ID", QString::number(i.key()));

        QByteArray data;
        if (m_db->compressionAlgorithm() == Database::CompressionGZip) {
            m_xml.writeAttribute("Compressed", "True");

            QBuffer buffer;
            buffer.open(QIODevice::ReadWrite);

            QtIOCompressor compressor(&buffer);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            compressor.open(QIODevice::WriteOnly);

            qint64 bytesWritten = compressor.write(i.value());
            Q_ASSERT(bytesWritten == i.value().size());
            Q_UNUSED(bytesWritten);
            compressor.close();

            buffer.seek(0);
            data = buffer.readAll();
        } else {
            data = i.value();
        }

        if (!data.isEmpty()) {
            m_xml.writeCharacters(QString::fromLatin1(data.toBase64()));
        }
        m_xml.writeEndElement();
    }

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeCustomData(const CustomData* customData, bool writeItemLastModified)
{
    if (customData->isEmpty()) {
        return;
    }
    m_xml.writeStartElement("CustomData");

    const QList<QString> keyList = customData->keys();
    for (const QString& key : keyList) {
        writeCustomDataItem(key, customData->item(key), writeItemLastModified);
    }

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeCustomDataItem(const QString& key,
                                        const CustomData::CustomDataItem& item,
                                        bool writeLastModified)
{
    m_xml.writeStartElement("Item");

    writeString("Key", key);
    writeString("Value", item.value);
    if (writeLastModified && m_kdbxVersion >= KeePass2::FILE_VERSION_4_1 && item.lastModified.isValid()) {
        writeDateTime("LastModificationTime", item.lastModified);
    }

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeRoot()
{
    Q_ASSERT(m_db->rootGroup());

    m_xml.writeStartElement("Root");

    writeGroup(m_db->rootGroup());
    writeDeletedObjects();

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeGroup(const Group* group)
{
    Q_ASSERT(!group->uuid().isNull());

    m_xml.writeStartElement("Group");

    writeUuid("UUID", group->uuid());
    writeString("Name", group->name());
    writeString("Notes", group->notes());
    if (!group->tags().isEmpty()) {
        writeString("Tags", group->tags());
    }
    writeNumber("IconID", group->iconNumber());

    if (!group->iconUuid().isNull()) {
        writeUuid("CustomIconUUID", group->iconUuid());
    }
    writeTimes(group->timeInfo());
    writeBool("IsExpanded", group->isExpanded());
    writeString("DefaultAutoTypeSequence", group->defaultAutoTypeSequence());

    writeTriState("EnableAutoType", group->autoTypeEnabled());

    writeTriState("EnableSearching", group->searchingEnabled());

    writeUuid("LastTopVisibleEntry", group->lastTopVisibleEntry());

    if (m_kdbxVersion >= KeePass2::FILE_VERSION_4) {
        writeCustomData(group->customData());
    }
    if (m_kdbxVersion >= KeePass2::FILE_VERSION_4_1 && !group->previousParentGroupUuid().isNull()) {
        writeUuid("PreviousParentGroup", group->previousParentGroupUuid());
    }

    const QList<Entry*>& entryList = group->entries();
    for (const Entry* entry : entryList) {
        writeEntry(entry);
    }

    const QList<Group*>& children = group->children();
    for (const Group* child : children) {
        writeGroup(child);
    }

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeTimes(const TimeInfo& ti)
{
    m_xml.writeStartElement("Times");

    writeDateTime("LastModificationTime", ti.lastModificationTime());
    writeDateTime("CreationTime", ti.creationTime());
    writeDateTime("LastAccessTime", ti.lastAccessTime());
    writeDateTime("ExpiryTime", ti.expiryTime());
    writeBool("Expires", ti.expires());
    writeNumber("UsageCount", ti.usageCount());
    writeDateTime("LocationChanged", ti.locationChanged());

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeDeletedObjects()
{
    m_xml.writeStartElement("DeletedObjects");

    const QList<DeletedObject> delObjList = m_db->deletedObjects();
    for (const DeletedObject& delObj : delObjList) {
        writeDeletedObject(delObj);
    }

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeDeletedObject(const DeletedObject& delObj)
{
    m_xml.writeStartElement("DeletedObject");

    writeUuid("UUID", delObj.uuid);
    writeDateTime("DeletionTime", delObj.deletionTime);

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeEntry(const Entry* entry)
{
    Q_ASSERT(!entry->uuid().isNull());

    m_xml.writeStartElement("Entry");

    writeUuid("UUID", entry->uuid());
    writeNumber("IconID", entry->iconNumber());
    if (!entry->iconUuid().isNull()) {
        writeUuid("CustomIconUUID", entry->iconUuid());
    }
    writeString("ForegroundColor", entry->foregroundColor());
    writeString("BackgroundColor", entry->backgroundColor());
    writeString("OverrideURL", entry->overrideUrl());
    writeString("Tags", entry->tags());
    writeTimes(entry->timeInfo());

    if (m_kdbxVersion >= KeePass2::FILE_VERSION_4_1) {
        if (entry->excludeFromReports()) {
            writeBool("QualityCheck", false);
        }
        if (!entry->previousParentGroupUuid().isNull()) {
            writeUuid("PreviousParentGroup", entry->previousParentGroupUuid());
        }
    }

    const QList<QString> attributesKeyList = entry->attributes()->keys();
    for (const QString& key : attributesKeyList) {
        m_xml.writeStartElement("String");

        // clang-format off
        bool protect =
            (((key == "Title") && m_meta->protectTitle()) || ((key == "UserName") && m_meta->protectUsername())
            || ((key == "Password") && m_meta->protectPassword())
            || ((key == "URL") && m_meta->protectUrl())
            || ((key == "Notes") && m_meta->protectNotes())
            || entry->attributes()->isProtected(key));
        // clang-format on

        writeString("Key", key);

        m_xml.writeStartElement("Value");
        QString value;

        if (protect) {
            if (!m_innerStreamProtectionDisabled && m_randomStream) {
                m_xml.writeAttribute("Protected", "True");
                bool ok;
                QByteArray rawData = m_randomStream->process(entry->attributes()->value(key).toUtf8(), &ok);
                if (!ok) {
                    raiseError(m_randomStream->errorString());
                }
                value = QString::fromLatin1(rawData.toBase64());
            } else {
                m_xml.writeAttribute("ProtectInMemory", "True");
                value = entry->attributes()->value(key);
            }
        } else {
            value = entry->attributes()->value(key);
        }

        if (!value.isEmpty()) {
            m_xml.writeCharacters(stripInvalidXml10Chars(value));
        }
        m_xml.writeEndElement();

        m_xml.writeEndElement();
    }

    const QList<QString> attachmentsKeyList = entry->attachments()->keys();
    for (const QString& key : attachmentsKeyList) {
        m_xml.writeStartElement("Binary");

        writeString("Key", key);

        m_xml.writeStartElement("Value");
        m_xml.writeAttribute("Ref", QString::number(m_binaryIdxMap[qMakePair(entry, key)]));
        m_xml.writeEndElement();

        m_xml.writeEndElement();
    }

    writeAutoType(entry);

    if (m_kdbxVersion >= KeePass2::FILE_VERSION_4) {
        writeCustomData(entry->customData());
    }

    // write history only for entries that are not history items
    if (entry->parent()) {
        writeEntryHistory(entry);
    }

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeAutoType(const Entry* entry)
{
    m_xml.writeStartElement("AutoType");

    writeBool("Enabled", entry->autoTypeEnabled());
    writeNumber("DataTransferObfuscation", entry->autoTypeObfuscation());
    writeString("DefaultSequence", entry->defaultAutoTypeSequence());

    const QList<AutoTypeAssociations::Association> autoTypeAssociations = entry->autoTypeAssociations()->getAll();
    for (const AutoTypeAssociations::Association& assoc : autoTypeAssociations) {
        writeAutoTypeAssoc(assoc);
    }

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeAutoTypeAssoc(const AutoTypeAssociations::Association& assoc)
{
    m_xml.writeStartElement("Association");

    writeString("Window", assoc.window);
    writeString("KeystrokeSequence", assoc.sequence);

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeEntryHistory(const Entry* entry)
{
    m_xml.writeStartElement("History");

    const QList<Entry*>& historyItems = entry->historyItems();
    for (const Entry* item : historyItems) {
        writeEntry(item);
    }

    m_xml.writeEndElement();
}

void KdbxXmlWriter::writeString(const QString& qualifiedName, const QString& string)
{
    if (string.isEmpty()) {
        m_xml.writeEmptyElement(qualifiedName);
    } else {
        m_xml.writeTextElement(qualifiedName, stripInvalidXml10Chars(string));
    }
}

void KdbxXmlWriter::writeNumber(const QString& qualifiedName, int number)
{
    writeString(qualifiedName, QString::number(number));
}

void KdbxXmlWriter::writeBool(const QString& qualifiedName, bool b)
{
    if (b) {
        writeString(qualifiedName, "True");
    } else {
        writeString(qualifiedName, "False");
    }
}

void KdbxXmlWriter::writeDateTime(const QString& qualifiedName, const QDateTime& dateTime)
{
    Q_ASSERT(dateTime.isValid());
    Q_ASSERT(dateTime.timeSpec() == Qt::UTC);

    QString dateTimeStr;
    if (m_kdbxVersion < KeePass2::FILE_VERSION_4) {
        dateTimeStr = dateTime.toString(Qt::ISODate);

        // Qt < 4.8 doesn't append a 'Z' at the end
        if (!dateTimeStr.isEmpty() && dateTimeStr[dateTimeStr.size() - 1] != 'Z') {
            dateTimeStr.append('Z');
        }
    } else {
        qint64 secs = QDateTime(QDate(1, 1, 1), QTime(0, 0, 0, 0), Qt::UTC).secsTo(dateTime);
        QByteArray secsBytes = Endian::sizedIntToBytes(secs, KeePass2::BYTEORDER);
        dateTimeStr = QString::fromLatin1(secsBytes.toBase64());
    }
    writeString(qualifiedName, dateTimeStr);
}

void KdbxXmlWriter::writeUuid(const QString& qualifiedName, const QUuid& uuid)
{
    writeString(qualifiedName, uuid.toRfc4122().toBase64());
}

void KdbxXmlWriter::writeUuid(const QString& qualifiedName, const Group* group)
{
    if (group) {
        writeUuid(qualifiedName, group->uuid());
    } else {
        writeUuid(qualifiedName, QUuid());
    }
}

void KdbxXmlWriter::writeUuid(const QString& qualifiedName, const Entry* entry)
{
    if (entry) {
        writeUuid(qualifiedName, entry->uuid());
    } else {
        writeUuid(qualifiedName, QUuid());
    }
}

void KdbxXmlWriter::writeBinary(const QString& qualifiedName, const QByteArray& ba)
{
    writeString(qualifiedName, QString::fromLatin1(ba.toBase64()));
}

void KdbxXmlWriter::writeTriState(const QString& qualifiedName, Group::TriState triState)
{
    QString value;

    if (triState == Group::Inherit) {
        value = "null";
    } else if (triState == Group::Enable) {
        value = "true";
    } else {
        value = "false";
    }

    writeString(qualifiedName, value);
}

QString KdbxXmlWriter::colorPartToString(int value)
{
    QString str = QString::number(value, 16).toUpper();
    if (str.length() == 1) {
        str.prepend("0");
    }

    return str;
}

QString KdbxXmlWriter::stripInvalidXml10Chars(QString str)
{
    for (int i = str.size() - 1; i >= 0; i--) {
        const QChar ch = str.at(i);
        const ushort uc = ch.unicode();

        if (ch.isLowSurrogate() && i != 0 && str.at(i - 1).isHighSurrogate()) {
            // keep valid surrogate pair
            i--;
        } else if ((uc < 0x20 && uc != 0x09 && uc != 0x0A && uc != 0x0D) // control characters
                   || (uc >= 0x7F && uc <= 0x84) // control characters, valid but discouraged by XML
                   || (uc >= 0x86 && uc <= 0x9F) // control characters, valid but discouraged by XML
                   || (uc > 0xFFFD) // noncharacter
                   || ch.isLowSurrogate() // single low surrogate
                   || ch.isHighSurrogate()) // single high surrogate
        {
            qWarning("Stripping invalid XML 1.0 codepoint %x", uc);
            str.remove(i, 1);
        }
    }

    return str;
}

void KdbxXmlWriter::raiseError(const QString& errorMessage)
{
    m_error = true;
    m_errorStr = errorMessage;
}

/**
 * Disable inner stream protection and write protected fields
 * in plaintext instead. This is useful for plaintext XML exports
 * where the inner stream key is not available.
 *
 * @param disable true to disable protection
 */
void KdbxXmlWriter::disableInnerStreamProtection(bool disable)
{
    m_innerStreamProtectionDisabled = disable;
}

/**
 * @return true if inner stream protection is disabled and protected
 *         fields will be saved in plaintext
 */
bool KdbxXmlWriter::innerStreamProtectionDisabled() const
{
    return m_innerStreamProtectionDisabled;
}
