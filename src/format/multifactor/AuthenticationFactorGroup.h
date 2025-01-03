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

#ifndef KEEPASSXC_AUTHENTICATIONFACTORGROUP_H
#define KEEPASSXC_AUTHENTICATIONFACTORGROUP_H

#include <QCoreApplication>
#include <QSharedPointer>
#include <botan/secmem.h>

#include "AuthenticationFactorInfo.h"
#include "core/AuthenticationFactorUserData.h"
#include "format/multifactor/AuthenticationFactor.h"

enum class AuthenticationFactorGroupValidationType
{
    NONE,
    HMAC_SHA512,
};

class AuthenticationFactor;
class AuthenticationFactorInfo;

class AuthenticationFactorGroup : public QObject
{
    Q_OBJECT

public:
    AuthenticationFactorGroup() = default;
    ~AuthenticationFactorGroup() override = default;

    QSharedPointer<QByteArray> getRawKey(const QSharedPointer<AuthenticationFactorUserData>& userData);

    void setValidationIn(const QByteArray& validationIn);
    const QByteArray& getValidationIn() const;
    void setValidationOut(const QByteArray& validationOut);
    const QByteArray& getValidationOut() const;
    void setChallenge(const QByteArray& challenge);
    const QByteArray& getChallenge() const;
    void setValidationType(AuthenticationFactorGroupValidationType validationType);
    AuthenticationFactorGroupValidationType getValidationType() const;
    void addFactor(const QSharedPointer<AuthenticationFactor>& factor);
    const QList<QSharedPointer<AuthenticationFactor>>& getFactors() const;

protected:
    QByteArray m_validationIn = QByteArray();
    QByteArray m_validationOut = QByteArray();
    QByteArray m_challenge = QByteArray();
    AuthenticationFactorGroupValidationType m_validationType = AuthenticationFactorGroupValidationType::NONE;

    QList<QSharedPointer<AuthenticationFactor>> m_factors = QList<QSharedPointer<AuthenticationFactor>>();

    Botan::secure_vector<char> m_key;
};

#endif // KEEPASSXC_AUTHENTICATIONFACTORGROUP_H
