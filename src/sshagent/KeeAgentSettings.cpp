/*
 *  Copyright (C) 2017 Toni Spets <toni.spets@iki.fi>
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

#include "KeeAgentSettings.h"

#include "OpenSSHKey.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Tools.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>
#include <QTextCodec>
#include <QXmlStreamReader>

KeeAgentSettings::KeeAgentSettings()
{
    reset();
}

bool KeeAgentSettings::KeySpec::operator==(const KeeAgentSettings::KeySpec& other) const
{
    return (key == other.key && isCertificateAuthority == other.isCertificateAuthority);
}

QByteArray KeeAgentSettings::KeySpec::getKeyBlob() const
{
    // In KeeAgent the key data is the second word in the string. First is the
    // key type. Third is the key comment which is optional.
    auto words = key.split(" ");
    if (words.length() >= 2) {
        return QByteArray::fromBase64(words[1].toLatin1(), QByteArray::Base64Encoding);
    } else {
        return QByteArray();
    }
}

bool KeeAgentSettings::DestinationConstraint::operator==(const KeeAgentSettings::DestinationConstraint& other) const
{
    return (fromHost == other.fromHost && fromHostKeys == other.fromHostKeys && toUser == other.toUser
            && toHost == other.toHost && toHostKeys == other.toHostKeys);
}

bool KeeAgentSettings::operator==(const KeeAgentSettings& other) const
{
    // clang-format off
    return (m_allowUseOfSshKey == other.m_allowUseOfSshKey && m_addAtDatabaseOpen == other.m_addAtDatabaseOpen
            && m_removeAtDatabaseClose == other.m_removeAtDatabaseClose
            && m_useConfirmConstraintWhenAdding == other.m_useConfirmConstraintWhenAdding
            && m_useLifetimeConstraintWhenAdding == other.m_useLifetimeConstraintWhenAdding
            && m_lifetimeConstraintDuration == other.m_lifetimeConstraintDuration
            && m_useDestinationConstraintsWhenAdding == other.m_useDestinationConstraintsWhenAdding
            && m_destinationConstraints == other.m_destinationConstraints
            && m_selectedType == other.m_selectedType
            && m_attachmentName == other.m_attachmentName
            && m_saveAttachmentToTempFile == other.m_saveAttachmentToTempFile
            && m_fileName == other.m_fileName);
    // clang-format on
}

bool KeeAgentSettings::operator!=(const KeeAgentSettings& other) const
{
    return !(*this == other);
}

/**
 * Test if this instance is at default settings.
 *
 * @return true if is at default settings
 */
bool KeeAgentSettings::isDefault() const
{
    KeeAgentSettings defaultSettings;
    return (*this == defaultSettings);
}

/**
 * Reset this instance to default settings
 */
void KeeAgentSettings::reset()
{
    m_allowUseOfSshKey = false;
    m_addAtDatabaseOpen = false;
    m_removeAtDatabaseClose = false;
    m_useConfirmConstraintWhenAdding = false;
    m_useLifetimeConstraintWhenAdding = false;
    m_lifetimeConstraintDuration = 600;
    m_useDestinationConstraintsWhenAdding = false;
    m_destinationConstraints.clear();

    m_selectedType = QStringLiteral("file");
    m_attachmentName.clear();
    m_saveAttachmentToTempFile = false;
    m_fileName.clear();
    m_error.clear();
}

/**
 * Get last error as a QString.
 *
 * @return translated error message
 */
const QString KeeAgentSettings::errorString() const
{
    return m_error;
}

bool KeeAgentSettings::allowUseOfSshKey() const
{
    return m_allowUseOfSshKey;
}

bool KeeAgentSettings::addAtDatabaseOpen() const
{
    return m_addAtDatabaseOpen;
}

bool KeeAgentSettings::removeAtDatabaseClose() const
{
    return m_removeAtDatabaseClose;
}

bool KeeAgentSettings::useConfirmConstraintWhenAdding() const
{
    return m_useConfirmConstraintWhenAdding;
}

bool KeeAgentSettings::useLifetimeConstraintWhenAdding() const
{
    return m_useLifetimeConstraintWhenAdding;
}

int KeeAgentSettings::lifetimeConstraintDuration() const
{
    return m_lifetimeConstraintDuration;
}

bool KeeAgentSettings::useDestinationConstraintsWhenAdding() const
{
    return m_useDestinationConstraintsWhenAdding;
}

QList<KeeAgentSettings::DestinationConstraint> KeeAgentSettings::destinationConstraints() const
{
    return m_destinationConstraints;
}

const QString KeeAgentSettings::selectedType() const
{
    return m_selectedType;
}

const QString KeeAgentSettings::attachmentName() const
{
    return m_attachmentName;
}

bool KeeAgentSettings::saveAttachmentToTempFile() const
{
    return m_saveAttachmentToTempFile;
}

const QString KeeAgentSettings::fileName() const
{
    return m_fileName;
}

const QString KeeAgentSettings::fileNameEnvSubst(QProcessEnvironment environment) const
{
    return Tools::envSubstitute(m_fileName, environment);
}

void KeeAgentSettings::setAllowUseOfSshKey(bool allowUseOfSshKey)
{
    m_allowUseOfSshKey = allowUseOfSshKey;
}

void KeeAgentSettings::setAddAtDatabaseOpen(bool addAtDatabaseOpen)
{
    m_addAtDatabaseOpen = addAtDatabaseOpen;
}

void KeeAgentSettings::setRemoveAtDatabaseClose(bool removeAtDatabaseClose)
{
    m_removeAtDatabaseClose = removeAtDatabaseClose;
}

void KeeAgentSettings::setUseConfirmConstraintWhenAdding(bool useConfirmConstraintWhenAdding)
{
    m_useConfirmConstraintWhenAdding = useConfirmConstraintWhenAdding;
}

void KeeAgentSettings::setUseLifetimeConstraintWhenAdding(bool useLifetimeConstraintWhenAdding)
{
    m_useLifetimeConstraintWhenAdding = useLifetimeConstraintWhenAdding;
}

void KeeAgentSettings::setLifetimeConstraintDuration(int lifetimeConstraintDuration)
{
    m_lifetimeConstraintDuration = lifetimeConstraintDuration;
}

void KeeAgentSettings::setUseDestinationConstraintsWhenAdding(bool useDestinationConstraintsWhenAdding)
{
    m_useDestinationConstraintsWhenAdding = useDestinationConstraintsWhenAdding;
}

void KeeAgentSettings::setDestinationConstraints(const QList<DestinationConstraint>& destinationConstraints)
{
    m_destinationConstraints = destinationConstraints;
}

void KeeAgentSettings::setSelectedType(const QString& selectedType)
{
    m_selectedType = selectedType;
}

void KeeAgentSettings::setAttachmentName(const QString& attachmentName)
{
    m_attachmentName = attachmentName;
}

void KeeAgentSettings::setSaveAttachmentToTempFile(bool saveAttachmentToTempFile)
{
    m_saveAttachmentToTempFile = saveAttachmentToTempFile;
}

void KeeAgentSettings::setFileName(const QString& fileName)
{
    m_fileName = fileName;
}

bool KeeAgentSettings::readBool(QXmlStreamReader& reader)
{
    reader.readNext();
    bool ret = (reader.text().startsWith("t", Qt::CaseInsensitive));
    reader.readNext(); // tag end
    return ret;
}

int KeeAgentSettings::readInt(QXmlStreamReader& reader)
{
    reader.readNext();
    int ret = reader.text().toInt();
    reader.readNext(); // tag end
    return ret;
}

/**
 * Read settings from an XML document.
 *
 * Sets error string on error.
 *
 * @param ba XML document
 * @return success
 */
bool KeeAgentSettings::fromXml(const QByteArray& ba)
{
    QXmlStreamReader reader;
    reader.addData(ba);

    reset();

    if (reader.error() || !reader.readNextStartElement()) {
        m_error = reader.errorString();
        return false;
    }

    if (reader.qualifiedName() != "EntrySettings") {
        m_error = QCoreApplication::translate("KeeAgentSettings", "Invalid KeeAgent settings file structure.");
        return false;
    }

    while (!reader.error() && reader.readNextStartElement()) {
        if (reader.name() == "AllowUseOfSshKey") {
            m_allowUseOfSshKey = readBool(reader);
        } else if (reader.name() == "AddAtDatabaseOpen") {
            m_addAtDatabaseOpen = readBool(reader);
        } else if (reader.name() == "RemoveAtDatabaseClose") {
            m_removeAtDatabaseClose = readBool(reader);
        } else if (reader.name() == "UseConfirmConstraintWhenAdding") {
            m_useConfirmConstraintWhenAdding = readBool(reader);
        } else if (reader.name() == "UseLifetimeConstraintWhenAdding") {
            m_useLifetimeConstraintWhenAdding = readBool(reader);
        } else if (reader.name() == "LifetimeConstraintDuration") {
            m_lifetimeConstraintDuration = readInt(reader);
        } else if (reader.name() == "Location") {
            while (!reader.error() && reader.readNextStartElement()) {
                if (reader.name() == "SelectedType") {
                    reader.readNext();
                    m_selectedType = reader.text().toString();
                    reader.readNext();
                } else if (reader.name() == "AttachmentName") {
                    reader.readNext();
                    m_attachmentName = reader.text().toString();
                    reader.readNext();
                } else if (reader.name() == "SaveAttachmentToTempFile") {
                    m_saveAttachmentToTempFile = readBool(reader);
                } else if (reader.name() == "FileName") {
                    reader.readNext();
                    m_fileName = reader.text().toString();
                    reader.readNext();
                } else {
                    qWarning() << "Skipping location element" << reader.name();
                    reader.skipCurrentElement();
                }
            }
            if (!reader.error())
                reader.readNext();
        } else if (reader.name() == "UseDestinationConstraintWhenAdding") {
            m_useDestinationConstraintsWhenAdding = readBool(reader);
        } else if (reader.name() == "DestinationConstraints") {
            while (!reader.error() && reader.readNextStartElement()) {
                if (reader.name() == "Constraint") {
                    KeeAgentSettings::DestinationConstraint constraint;
                    while (!reader.error() && reader.readNextStartElement()) {
                        if (reader.name() == "FromHostKeys" || reader.name() == "ToHostKeys") {
                            QString section = reader.name().toString();
                            while (!reader.error() && reader.readNextStartElement()) {
                                if (reader.name() == "KeySpec") {
                                    KeeAgentSettings::KeySpec keyspec;
                                    while (!reader.error() && reader.readNextStartElement()) {
                                        if (reader.name() == "HostKey") {
                                            reader.readNext();
                                            keyspec.key = reader.text().toString();
                                            reader.readNext();
                                        } else if (reader.name() == "IsCA") {
                                            keyspec.isCertificateAuthority = readBool(reader);
                                        } else {
                                            qWarning() << "Skipping KeySpec element" << reader.name();
                                            reader.skipCurrentElement();
                                        }
                                    }

                                    if (keyspec.getKeyBlob().isEmpty()) {
                                        return false;
                                    }

                                    if (section == "FromHostKeys") {
                                        constraint.fromHostKeys.append(std::move(keyspec));
                                    } else {
                                        constraint.toHostKeys.append(std::move(keyspec));
                                    }
                                    if (!reader.error())
                                        reader.readNext();
                                } else {
                                    qWarning() << "Skipping " << section << " element" << reader.name();
                                    reader.skipCurrentElement();
                                }
                            }
                            if (!reader.error())
                                reader.readNext();
                        } else if (reader.name() == "FromHost") {
                            reader.readNext();
                            constraint.fromHost = reader.text().toString();
                            reader.readNext();
                        } else if (reader.name() == "ToUser") {
                            reader.readNext();
                            constraint.toUser = reader.text().toString();
                            reader.readNext();
                        } else if (reader.name() == "ToHost") {
                            reader.readNext();
                            constraint.toHost = reader.text().toString();
                            reader.readNext();
                        } else {
                            qWarning() << "Skipping Constraint element" << reader.name();
                            reader.skipCurrentElement();
                        }
                    }

                    if ((constraint.fromHost.isEmpty() && !constraint.fromHostKeys.isEmpty())
                        || (!constraint.fromHost.isEmpty() && constraint.fromHostKeys.isEmpty())) {
                        return false;
                    }
                    if (constraint.toHost.isEmpty() || constraint.toHostKeys.isEmpty()) {
                        return false;
                    }

                    m_destinationConstraints.append(std::move(constraint));

                    if (!reader.error())
                        reader.readNext();
                } else {
                    qWarning() << "Skipping DestinationConstraints element" << reader.name();
                    reader.skipCurrentElement();
                }
            }
            if (!reader.error())
                reader.readNext();
        } else {
            qWarning() << "Skipping element" << reader.name();
            reader.skipCurrentElement();
        }
    }

    return true;
}

/**
 * Write settings to an XML document.
 *
 * @return XML document
 */
QByteArray KeeAgentSettings::toXml() const
{
    QByteArray ba;
    QXmlStreamWriter writer(&ba);

    // real KeeAgent can only read UTF-16
    writer.setCodec(QTextCodec::codecForName("UTF-16"));
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);

    writer.writeStartDocument();

    writer.writeStartElement("EntrySettings");
    writer.writeAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");
    writer.writeAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");

    writer.writeTextElement("AllowUseOfSshKey", m_allowUseOfSshKey ? "true" : "false");
    writer.writeTextElement("AddAtDatabaseOpen", m_addAtDatabaseOpen ? "true" : "false");
    writer.writeTextElement("RemoveAtDatabaseClose", m_removeAtDatabaseClose ? "true" : "false");
    writer.writeTextElement("UseConfirmConstraintWhenAdding", m_useConfirmConstraintWhenAdding ? "true" : "false");
    writer.writeTextElement("UseLifetimeConstraintWhenAdding", m_useLifetimeConstraintWhenAdding ? "true" : "false");
    writer.writeTextElement("LifetimeConstraintDuration", QString::number(m_lifetimeConstraintDuration));
    writer.writeTextElement("UseDestinationConstraintWhenAdding",
                            m_useDestinationConstraintsWhenAdding ? "true" : "false");

    writer.writeStartElement("DestinationConstraints");

    foreach (const auto& constraint, m_destinationConstraints) {
        writer.writeStartElement("Constraint");

        if (constraint.fromHost.isEmpty()) {
            writer.writeEmptyElement("FromHost");
        } else {
            writer.writeTextElement("FromHost", constraint.fromHost);
        }

        writer.writeStartElement("FromHostKeys");
        foreach (const auto& keyspec, constraint.fromHostKeys) {
            writer.writeStartElement("KeySpec");
            writer.writeTextElement("HostKey", keyspec.key);
            writer.writeTextElement("IsCA", keyspec.isCertificateAuthority ? "true" : "false");
            writer.writeEndElement(); // KeySpec
        }
        writer.writeEndElement(); // FromHostKeys

        if (constraint.toUser.isEmpty()) {
            writer.writeEmptyElement("ToUser");
        } else {
            writer.writeTextElement("ToUser", constraint.toUser);
        }
        if (constraint.toHost.isEmpty()) {
            writer.writeEmptyElement("ToHost");
        } else {
            writer.writeTextElement("ToHost", constraint.toHost);
        }

        writer.writeStartElement("ToHostKeys");
        foreach (const auto& keyspec, constraint.toHostKeys) {
            writer.writeStartElement("KeySpec");
            writer.writeTextElement("HostKey", keyspec.key);
            writer.writeTextElement("IsCA", keyspec.isCertificateAuthority ? "true" : "false");
            writer.writeEndElement(); // KeySpec
        }
        writer.writeEndElement(); // ToHostKeys

        writer.writeEndElement(); // Constraint
    }

    writer.writeEndElement(); // DestinationConstraints

    writer.writeStartElement("Location");
    writer.writeTextElement("SelectedType", m_selectedType);

    if (!m_attachmentName.isEmpty()) {
        writer.writeTextElement("AttachmentName", m_attachmentName);
    } else {
        writer.writeEmptyElement("AttachmentName");
    }

    writer.writeTextElement("SaveAttachmentToTempFile", m_saveAttachmentToTempFile ? "true" : "false");

    if (!m_fileName.isEmpty()) {
        writer.writeTextElement("FileName", m_fileName);
    } else {
        writer.writeEmptyElement("FileName");
    }

    writer.writeEndElement(); // Location
    writer.writeEndElement(); // EntrySettings
    writer.writeEndDocument();

    return ba;
}

/**
 * Check if entry attachments have KeeAgent settings configured
 *
 * @param attachments EntryAttachments to check the key
 * @return true if XML document exists
 */
bool KeeAgentSettings::inEntryAttachments(const EntryAttachments* attachments)
{
    return attachments->hasKey("KeeAgent.settings");
}

/**
 * Read settings from an entry as an XML attachment.
 *
 * Sets error string on error.
 *
 * @param entry Entry to read the attachment from
 * @return true if XML document was loaded
 */
bool KeeAgentSettings::fromEntry(const Entry* entry)
{
    const auto attachments = entry->attachments();
    if (attachments->hasKey("KeeAgent.settings")) {
        return fromXml(attachments->value("KeeAgent.settings"));
    }
    return false;
}

/**
 * Write settings to an entry as an XML attachment.
 *
 * @param entry Entry to create the attachment to
 */
void KeeAgentSettings::toEntry(Entry* entry) const
{
    if (isDefault()) {
        if (entry->attachments()->hasKey("KeeAgent.settings")) {
            entry->attachments()->remove("KeeAgent.settings");
        }
    } else {
        entry->attachments()->set("KeeAgent.settings", toXml());
    }
}

/**
 * Test if a SSH key is currently set to be used
 *
 * @return true if key is configured
 */
bool KeeAgentSettings::keyConfigured() const
{
    if (m_selectedType == "attachment") {
        return !m_attachmentName.isEmpty();
    } else {
        return !m_fileName.isEmpty();
    }
}

/**
 * Read a SSH key based on settings from entry to key.
 *
 * Sets error string on error.
 *
 * @param entry input entry to read attachment and decryption key
 * @param key output key object
 * @param decrypt avoid private key decryption if possible (old RSA keys are always decrypted)
 * @return true if key was properly opened
 */
bool KeeAgentSettings::toOpenSSHKey(const Entry* entry, OpenSSHKey& key, bool decrypt)
{
    return toOpenSSHKey(
        entry->username(), entry->password(), entry->database()->filePath(), entry->attachments(), key, decrypt);
}

/**
 * Read a SSH key based on settings to key.
 *
 * Sets error string on error.
 *
 * @param username username to set on key if empty
 * @param password password to decrypt key if needed
 * @param databasePath path to database file this key is loaded from
 * @param attachments attachments to read an attachment key from
 * @param key output key object
 * @param decrypt avoid private key decryption if possible (old RSA keys are always decrypted)
 * @return true if key was properly opened
 */
bool KeeAgentSettings::toOpenSSHKey(const QString& username,
                                    const QString& password,
                                    const QString& databasePath,
                                    const EntryAttachments* attachments,
                                    OpenSSHKey& key,
                                    bool decrypt)
{
    QString fileName;
    QByteArray privateKeyData;

    if (m_selectedType == "attachment") {
        if (!attachments) {
            m_error = QCoreApplication::translate("KeeAgentSettings",
                                                  "Private key is an attachment but no attachments provided.");
            return false;
        }

        fileName = m_attachmentName;
        privateKeyData = attachments->value(fileName);
    } else {
        QString fileNameSubst = fileNameEnvSubst();
        QFileInfo localFileInfo(fileNameSubst);

        // resolve relative private key path from database location
        if (localFileInfo.isRelative()) {
            QFileInfo databaseFileInfo(databasePath);
            localFileInfo = QFileInfo(databaseFileInfo.absolutePath() + QDir::separator() + fileNameSubst);
        }

        fileName = localFileInfo.fileName();

        QFile localFile(localFileInfo.absoluteFilePath());

        if (localFile.fileName().isEmpty()) {
            m_error = QCoreApplication::translate("KeeAgentSettings", "Private key is empty");
            return false;
        }

        if (localFile.size() > 1024 * 1024) {
            m_error = QCoreApplication::translate("KeeAgentSettings", "File too large to be a private key");
            return false;
        }

        if (!localFile.open(QIODevice::ReadOnly)) {
            m_error = QCoreApplication::translate("KeeAgentSettings", "Failed to open private key");
            return false;
        }

        privateKeyData = localFile.readAll();
    }

    if (privateKeyData.isEmpty()) {
        m_error = QCoreApplication::translate("KeeAgentSettings", "Private key is empty");
        return false;
    }

    if (!key.parsePKCS1PEM(privateKeyData)) {
        m_error = key.errorString();
        return false;
    }

    if (key.encrypted() && (decrypt || key.publicKey().isEmpty())) {
        if (!key.openKey(password)) {
            m_error = key.errorString();
            return false;
        }
    }

    if (key.comment().isEmpty()) {
        key.setComment(QString("%1@%2").arg(username, fileName));
    }

    return true;
}
