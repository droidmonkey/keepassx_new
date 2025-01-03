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

#include "AuthenticationFactorGroup.h"

#include <QDebug>

void AuthenticationFactorGroup::setValidationIn(const QByteArray& validationIn)
{
    m_validationIn = validationIn;
}

void AuthenticationFactorGroup::setValidationOut(const QByteArray& validationOut)
{
    m_validationOut = validationOut;
}

void AuthenticationFactorGroup::setChallenge(const QByteArray& challenge)
{
    m_challenge = challenge;
}

void AuthenticationFactorGroup::setValidationType(const AuthenticationFactorGroupValidationType validationType)
{
    m_validationType = validationType;
}

void AuthenticationFactorGroup::addFactor(const QSharedPointer<AuthenticationFactor>& factor)
{
    m_factors.append(factor);
    factor->setParent(this);
}

const QList<QSharedPointer<AuthenticationFactor>>& AuthenticationFactorGroup::getFactors() const
{
    return m_factors;
}

const QByteArray& AuthenticationFactorGroup::getValidationIn() const
{
    return m_validationIn;
}

const QByteArray& AuthenticationFactorGroup::getValidationOut() const
{
    return m_validationOut;
}

const QByteArray& AuthenticationFactorGroup::getChallenge() const
{
    return m_challenge;
}

AuthenticationFactorGroupValidationType AuthenticationFactorGroup::getValidationType() const
{
    return m_validationType;
}

QSharedPointer<QByteArray>
AuthenticationFactorGroup::getRawKey(const QSharedPointer<AuthenticationFactorUserData>& userData)
{
    bool groupContributed = false;

    for (const auto& factor : getFactors()) {
        auto unwrappedKey = factor->unwrapKey(userData);

        if (unwrappedKey == nullptr) {
            qDebug() << QObject::tr("Factor '%1' did not contribute key material").arg(factor->getName());
            continue;
        }

        qDebug() << QObject::tr("Got a key part from factor '%1'").arg(factor->getName());

        m_key.insert(m_key.end(), unwrappedKey->begin(), unwrappedKey->end());
        groupContributed = true;
        break;
    }

    if (!groupContributed) {
        return {nullptr};
    }

    return QSharedPointer<QByteArray>::create(m_key.data(), m_key.size());
}
