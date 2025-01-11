/*
 * Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_KDBXXMLREADER_H
#define KEEPASSXC_KDBXXMLREADER_H

#include "core/Database.h"
#include "core/Metadata.h"

#include <QCoreApplication>
#include <QMultiHash>
#include <QStringView>
#include <QXmlStreamReader>

class QIODevice;
class Group;
class Entry;
class KeePass2RandomStream;
class TimeInfo;

/**
 * KDBX XML payload reader.
 */
class KdbxXmlReader
{
    Q_DECLARE_TR_FUNCTIONS(KdbxXmlReader)

public:
    explicit KdbxXmlReader(quint32 version);
    explicit KdbxXmlReader(quint32 version, QHash<QString, QByteArray> binaryPool);
    virtual ~KdbxXmlReader() = default;

    virtual QSharedPointer<Database> readDatabase(const QString& filename);
    virtual QSharedPointer<Database> readDatabase(QIODevice* device);
    virtual void readDatabase(QIODevice* device, Database* db, KeePass2RandomStream* randomStream = nullptr);

    bool hasError() const;
    QString errorString() const;

    QByteArray headerHash() const;

    bool strictMode() const;
    void setStrictMode(bool strictMode);

protected:
    typedef QPair<QString, QString> StringPair;

    virtual bool parseKeePassFile();
    virtual void parseMeta();
    virtual void parseMemoryProtection();
    virtual void parseCustomIcons();
    virtual void parseIcon();
    virtual void parseBinaries();
    virtual void parseCustomData(CustomData* customData);
    virtual void parseCustomDataItem(CustomData* customData);
    virtual bool parseRoot();
    virtual Group* parseGroup();
    virtual void parseDeletedObjects();
    virtual void parseDeletedObject();
    virtual Entry* parseEntry(bool history);
    virtual void parseEntryString(Entry* entry);
    virtual QPair<QString, QString> parseEntryBinary(Entry* entry);
    virtual void parseAutoType(Entry* entry);
    virtual void parseAutoTypeAssoc(Entry* entry);
    virtual QList<Entry*> parseEntryHistory();
    virtual TimeInfo parseTimes();

    virtual QString readString();
    virtual QString readString(bool& isProtected, bool& protectInMemory);
    virtual bool readBool();
    virtual QDateTime readDateTime();
    virtual QString readColor();
    virtual int readNumber();
    virtual QUuid readUuid();
    virtual QByteArray readBinary();
    virtual QByteArray readCompressedBinary();

    virtual void skipCurrentElement();

    virtual Group* getGroup(const QUuid& uuid);
    virtual Entry* getEntry(const QUuid& uuid);

    virtual bool isTrueValue(const QStringView value);
    virtual void raiseError(const QString& errorMessage);

    const quint32 m_kdbxVersion;

    bool m_strictMode = false;

    QPointer<Database> m_db;
    QPointer<Metadata> m_meta;
    KeePass2RandomStream* m_randomStream = nullptr;
    QXmlStreamReader m_xml;

    QScopedPointer<Group> m_tmpParent;
    QHash<QUuid, Group*> m_groups;
    QHash<QUuid, Entry*> m_entries;

    QHash<QString, QByteArray> m_binaryPool;
    QMultiHash<QString, QPair<Entry*, QString>> m_binaryMap;
    QByteArray m_headerHash;

    bool m_error = false;
    QString m_errorStr = "";
};

#endif // KEEPASSXC_KDBXXMLREADER_H
