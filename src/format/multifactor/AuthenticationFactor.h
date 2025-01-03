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

#ifndef KEEPASSXC_AUTHENTICATIONFACTOR_H
#define KEEPASSXC_AUTHENTICATIONFACTOR_H

#include "AuthenticationFactorGroup.h"
#include "FactorKeyDerivation.h"
#include "core/AuthenticationFactorUserData.h"

#include <QCoreApplication>

class AuthenticationFactorGroup;

#define FACTOR_TYPE_PASSWORD_SHA256 "c127a67f-be51-4bba-ac6f-7351e8a70ba0"
#define FACTOR_TYPE_KEY_FILE "6b9746c7-ca8d-430b-986d-1afaf689c4e4"
#define FACTOR_TYPE_FIDO_ES256 "15f77f9d-a65c-4a2e-b2b5-171f7b2df41a"
#define FACTOR_TYPE_YK_CHALRESP "0e6803a0-915e-4ebf-95ee-f9ddd8c97eea"
#define FACTOR_TYPE_NULL "618636bf-e202-4e0b-bb7c-e2514be00f5a"

enum class AuthenticationFactorKeyType
{
    NONE,
    AES_CBC,
};

class AuthenticationFactor : public QObject
{
    Q_OBJECT

public:
    explicit AuthenticationFactor() = default;
    ~AuthenticationFactor() override = default;

    virtual QSharedPointer<QByteArray> unwrapKey(const QSharedPointer<AuthenticationFactorUserData>& userData) const;

    const QString& getFactorType() const;

    const QString& getName() const;
    void setName(const QString& name);
    AuthenticationFactorKeyType getKeyType() const;
    void setKeyType(AuthenticationFactorKeyType type);
    const QByteArray& getKeySalt() const;
    void setKeySalt(const QByteArray& salt);
    const QByteArray& getWrappedKey() const;
    void setWrappedKey(const QByteArray& key);

protected:
    virtual QByteArray getUnwrappingKey(const QSharedPointer<AuthenticationFactorUserData>& userData) const;

    QString m_name = "<Unnamed factor>";
    AuthenticationFactorKeyType m_keyType = AuthenticationFactorKeyType::NONE;
    QByteArray m_keySalt = QByteArray();
    QByteArray m_wrappedKey = QByteArray();

    QString m_factorType = FACTOR_TYPE_NULL;
    QSharedPointer<FactorKeyDerivation> m_derivation;
};

#endif // KEEPASSXC_AUTHENTICATIONFACTOR_H
