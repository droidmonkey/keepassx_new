/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#include "PasswordKey.h"

#include "crypto/CryptoHash.h"

#include <QDataStream>
#include <QIODevice>
#include <QSharedPointer>

QUuid PasswordKey::UUID("77e90411-303a-43f2-b773-853b05635ead");

constexpr int PasswordKey::SHA256_SIZE;

PasswordKey::PasswordKey()
    : Key(UUID)
    , m_key(SHA256_SIZE)
{
}

PasswordKey::PasswordKey(const QString& password)
    : Key(UUID)
    , m_key(SHA256_SIZE)
{
    setPassword(password);
}

QByteArray PasswordKey::rawKey() const
{
    if (!m_isInitialized) {
        return {};
    }
    return {m_key.data(), int(m_key.size())};
}

void PasswordKey::setRawKey(const QByteArray& data)
{
    if (data.isEmpty()) {
        m_key.clear();
        m_isInitialized = false;
    } else {
        Q_ASSERT(data.size() == SHA256_SIZE);
        m_key.assign(data.begin(), data.end());
        m_isInitialized = true;
    }
}

void PasswordKey::setPassword(const QString& password)
{
    setRawKey(CryptoHash::hash(password.toUtf8(), CryptoHash::Sha256));
}

QSharedPointer<PasswordKey> PasswordKey::fromRawKey(const QByteArray& rawKey)
{
    auto result = QSharedPointer<PasswordKey>::create();
    result->setRawKey(rawKey);
    return result;
}

QByteArray PasswordKey::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << uuid().toRfc4122() << rawKey();
    return data;
}

void PasswordKey::deserialize(const QByteArray& data)
{
    QByteArray uuidData, key;
    QDataStream stream(data);
    stream >> uuidData >> key;
    if (uuid().toRfc4122() == uuidData) {
        setRawKey(key);
    }
}
