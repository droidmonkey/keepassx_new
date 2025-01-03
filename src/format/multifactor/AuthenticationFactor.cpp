/*
 * Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "AuthenticationFactor.h"
#include "AESCBCFactorKeyDerivation.h"

#include <QDebug>
#include <QSharedPointer>

void AuthenticationFactor::setName(const QString& name)
{
    m_name = name;
}

const QString& AuthenticationFactor::getName() const
{
    return m_name;
}

void AuthenticationFactor::setKeyType(AuthenticationFactorKeyType type)
{
    m_keyType = type;

    if (m_keyType == AuthenticationFactorKeyType::AES_CBC) {
        m_derivation = QSharedPointer<AESCBCFactorKeyDerivation>::create();
    } else {
        m_derivation = QSharedPointer<AESCBCFactorKeyDerivation>(nullptr);
    }
}

void AuthenticationFactor::setKeySalt(const QByteArray& salt)
{
    m_keySalt = salt;
}

void AuthenticationFactor::setWrappedKey(const QByteArray& key)
{
    m_wrappedKey = key;
}

const QByteArray& AuthenticationFactor::getWrappedKey() const
{
    return m_wrappedKey;
}

const QByteArray& AuthenticationFactor::getKeySalt() const
{
    return m_keySalt;
}

AuthenticationFactorKeyType AuthenticationFactor::getKeyType() const
{
    return m_keyType;
}

const QString& AuthenticationFactor::getFactorType() const
{
    return m_factorType;
}

QSharedPointer<QByteArray>
AuthenticationFactor::unwrapKey(const QSharedPointer<AuthenticationFactorUserData>& userData) const
{
    auto unwrappingKey = getUnwrappingKey(userData);

    auto wrappedKey = getWrappedKey();

    if (m_derivation->derive(wrappedKey, unwrappingKey, getKeySalt())) {
        // "wrappedKey" is now unwrapped!
        return QSharedPointer<QByteArray>::create(wrappedKey);
    } else {
        qWarning() << tr("Validation failed when unwrapping factor '%1': %2").arg(getName(), m_derivation->getError());
    }

    return {nullptr};
}

QByteArray AuthenticationFactor::getUnwrappingKey(const QSharedPointer<AuthenticationFactorUserData>& userData) const
{
    Q_UNUSED(userData);
    // This shouldn't happen - it means we didn't understand the factor type?
    qWarning() << "Attempted to get unwrapping key from generic AuthenticationFactor";
    return QByteArray();
}
