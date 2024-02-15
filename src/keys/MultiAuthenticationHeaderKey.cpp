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

#include "MultiAuthenticationHeaderKey.h"

#include "FileKey.h"
#include "PasswordKey.h"
#include "core/AsyncTask.h"

#include <QCoreApplication>

QUuid MultiAuthenticationHeaderKey::UUID("e31ab20b-ee50-45af-99bc-ab4c4d34f4cc");

MultiAuthenticationHeaderKey::MultiAuthenticationHeaderKey(
    const QSharedPointer<const AuthenticationFactorInfo>& authenticationFactorInfo,
    const QSharedPointer<const CompositeKey>& existingKey)
    : Key(UUID)
{
    m_authenticationFactorInfo = authenticationFactorInfo;
    m_userData = QSharedPointer<AuthenticationFactorUserData>::create();

    for (const auto& keyPart : existingKey->keys()) {
        const auto& uuid = keyPart->uuid();

        m_userData->addDataItem(uuid.toString(), QSharedPointer<QByteArray>::create(keyPart->rawKey()));
    }
}

QByteArray MultiAuthenticationHeaderKey::rawKey() const
{
    return {m_key.data(), int(m_key.size())};
}

bool MultiAuthenticationHeaderKey::process()
{
    qDebug() << QObject::tr("Attempting to add key material from extra authentication factors");

    auto userData = QSharedPointer<AuthenticationFactorUserData>::create();

    if (m_key.empty()) {
        // Construct key from parts
        for (const auto& group : m_authenticationFactorInfo->getGroups()) {
            auto groupPart = group->getRawKey(m_userData);
            if (groupPart.isNull()) {
                m_error = QObject::tr("Unable to get keying material from an authentication factor group");
                return false;
            }
            m_key.insert(m_key.end(), groupPart->begin(), groupPart->end());
        }
    }

    return true;
}

void MultiAuthenticationHeaderKey::setRawKey(const QByteArray& data)
{
    m_key.assign(data.begin(), data.end());
    m_error = "";
}

QString MultiAuthenticationHeaderKey::error() const
{
    return m_error;
}

QByteArray MultiAuthenticationHeaderKey::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << uuid().toRfc4122();
    return data;
}

void MultiAuthenticationHeaderKey::deserialize(const QByteArray& data)
{
    QDataStream stream(data);
    QByteArray uuidData;
    stream >> uuidData;
}
