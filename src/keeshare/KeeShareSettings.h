/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_KEESHARESETTINGS_H
#define KEEPASSXC_KEESHARESETTINGS_H

#include <QSharedPointer>
#include <QUuid>

namespace Botan
{
    class Private_Key;
}

class CustomData;
class QXmlStreamWriter;
class QXmlStreamReader;

namespace KeeShareSettings
{
    struct Certificate
    {
        QSharedPointer<Botan::Private_Key> key;
        QString signer;

        bool operator==(const Certificate& other) const;
        bool operator!=(const Certificate& other) const;

        bool isNull() const;
        QString fingerprint() const;

        static void serialize(QXmlStreamWriter& writer, const Certificate& certificate);
        static Certificate deserialize(QXmlStreamReader& reader);
    };

    struct Key
    {
        QSharedPointer<Botan::Private_Key> key;

        bool operator==(const Key& other) const;
        bool operator!=(const Key& other) const;

        bool isNull() const;

        static void serialize(QXmlStreamWriter& writer, const Key& key);
        static Key deserialize(QXmlStreamReader& reader);
    };

    struct Active
    {
        bool in;
        bool out;

        Active()
            : in(false)
            , out(false)
        {
        }

        bool isNull() const
        {
            return !in && !out;
        }

        static QString serialize(const Active& active);
        static Active deserialize(const QString& raw);
    };

    struct Own
    {
        Key key;
        Certificate certificate;

        bool operator==(const Own& other) const;
        bool operator!=(const Own& other) const;

        bool isNull() const
        {
            return key.isNull() && certificate.isNull();
        }

        static QString serialize(const Own& own);
        static Own deserialize(const QString& raw);
        static Own generate();
    };

    struct Sign
    {
        QString signature;
        Certificate certificate;

        bool isNull() const
        {
            return signature.isEmpty() && certificate.isNull();
        }

        static QString serialize(const Sign& sign);
    };

    enum TypeFlag
    {
        Inactive = 0,
        ImportFrom = 1 << 0,
        ExportTo = 1 << 1,
        SynchronizeWith = ImportFrom | ExportTo
    };
    Q_DECLARE_FLAGS(Type, TypeFlag)

    struct Reference
    {
        Type type;
        QUuid uuid;
        QString path;
        QString password;
        bool recurse;

        Reference();
        bool isNull() const;
        bool isValid() const;
        bool isExporting() const;
        bool isImporting() const;
        bool operator<(const Reference& other) const;
        bool operator==(const Reference& other) const;

        static QString serialize(const Reference& reference);
        static Reference deserialize(const QString& raw);
    };
}; // namespace KeeShareSettings

#endif // KEEPASSXC_KEESHARESETTINGS_H
