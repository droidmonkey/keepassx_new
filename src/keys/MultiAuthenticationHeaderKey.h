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

#ifndef KPXC_MULTI_AUTHENTICATION_HEADER_KEY_H
#define KPXC_MULTI_AUTHENTICATION_HEADER_KEY_H

#include "CompositeKey.h"
#include "Key.h"
#include "format/multifactor/AuthenticationFactorInfo.h"

#include <QCoreApplication>
#include <botan/secmem.h>

class MultiAuthenticationHeaderKey : public Key
{
public:
    explicit MultiAuthenticationHeaderKey(
        const QSharedPointer<const AuthenticationFactorInfo>& authenticationFactorInfo,
        const QSharedPointer<const CompositeKey>& existingKey);
    ~MultiAuthenticationHeaderKey() override = default;

    bool process();

    QByteArray rawKey() const override;
    void setRawKey(const QByteArray&) override;

    QString error() const;

    QByteArray serialize() const override;
    void deserialize(const QByteArray& data) override;

    static QUuid UUID;

private:
    Q_DISABLE_COPY(MultiAuthenticationHeaderKey);

    QSharedPointer<const AuthenticationFactorInfo> m_authenticationFactorInfo;
    QSharedPointer<AuthenticationFactorUserData> m_userData;

    QString m_error;
    Botan::secure_vector<char> m_key;
};

#endif // KPXC_MULTI_AUTHENTICATION_HEADER_KEY_H
