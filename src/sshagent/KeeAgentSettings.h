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

#ifndef KEEAGENTSETTINGS_H
#define KEEAGENTSETTINGS_H

#include <QProcessEnvironment>

class Entry;
class EntryAttachments;
class OpenSSHKey;
class QXmlStreamReader;

class KeeAgentSettings
{
public:
    struct KeySpec
    {
        QString key;
        bool isCertificateAuthority;

        bool operator==(const KeySpec& other) const;

        QByteArray getKeyBlob() const;
    };

    struct DestinationConstraint
    {
        QString fromHost;
        QList<KeySpec> fromHostKeys;
        QString toUser;
        QString toHost;
        QList<KeySpec> toHostKeys;

        bool operator==(const DestinationConstraint& other) const;
    };

    KeeAgentSettings();
    bool operator==(const KeeAgentSettings& other) const;
    bool operator!=(const KeeAgentSettings& other) const;
    bool isDefault() const;
    void reset();

    bool fromXml(const QByteArray& ba);
    QByteArray toXml() const;

    static bool inEntryAttachments(const EntryAttachments* attachments);
    bool fromEntry(const Entry* entry);
    void toEntry(Entry* entry) const;
    bool keyConfigured() const;
    bool toOpenSSHKey(const Entry* entry, OpenSSHKey& key, bool decrypt);
    bool toOpenSSHKey(const QString& username,
                      const QString& password,
                      const QString& databasePath,
                      const EntryAttachments* attachments,
                      OpenSSHKey& key,
                      bool decrypt);

    const QString errorString() const;

    bool allowUseOfSshKey() const;
    bool addAtDatabaseOpen() const;
    bool removeAtDatabaseClose() const;
    bool useConfirmConstraintWhenAdding() const;
    bool useLifetimeConstraintWhenAdding() const;
    int lifetimeConstraintDuration() const;
    bool useDestinationConstraintsWhenAdding() const;
    QList<DestinationConstraint> destinationConstraints() const;

    const QString selectedType() const;
    const QString attachmentName() const;
    bool saveAttachmentToTempFile() const;
    const QString fileName() const;
    const QString fileNameEnvSubst(QProcessEnvironment environment = QProcessEnvironment::systemEnvironment()) const;

    void setAllowUseOfSshKey(bool allowUseOfSshKey);
    void setAddAtDatabaseOpen(bool addAtDatabaseOpen);
    void setRemoveAtDatabaseClose(bool removeAtDatabaseClose);
    void setUseConfirmConstraintWhenAdding(bool useConfirmConstraintWhenAdding);
    void setUseLifetimeConstraintWhenAdding(bool useLifetimeConstraintWhenAdding);
    void setLifetimeConstraintDuration(int lifetimeConstraintDuration);
    void setUseDestinationConstraintsWhenAdding(bool useDestinationConstraintsWhenAdding);
    void setDestinationConstraints(const QList<DestinationConstraint>& destinationConstraints);

    void setSelectedType(const QString& type);
    void setAttachmentName(const QString& attachmentName);
    void setSaveAttachmentToTempFile(bool);
    void setFileName(const QString& fileName);

private:
    bool readBool(QXmlStreamReader& reader);
    int readInt(QXmlStreamReader& reader);

    bool m_allowUseOfSshKey;
    bool m_addAtDatabaseOpen;
    bool m_removeAtDatabaseClose;
    bool m_useConfirmConstraintWhenAdding;
    bool m_useLifetimeConstraintWhenAdding;
    int m_lifetimeConstraintDuration;
    bool m_useDestinationConstraintsWhenAdding;
    QList<DestinationConstraint> m_destinationConstraints;

    // location
    QString m_selectedType;
    QString m_attachmentName;
    bool m_saveAttachmentToTempFile;
    QString m_fileName;
    QString m_error;
};

#endif // KEEAGENTSETTINGS_H
