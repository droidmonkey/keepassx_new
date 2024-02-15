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

#include "FidoAuthenticationFactor.h"

FidoAuthenticationFactor::FidoAuthenticationFactor(const QSharedPointer<AuthenticationFactor>& factor)
{
    m_name = factor->getName();
    m_keyType = factor->getKeyType();
    m_keySalt = factor->getKeySalt();
    m_wrappedKey = factor->getWrappedKey();
    m_factorType = FACTOR_TYPE_FIDO_ES256;
}

void FidoAuthenticationFactor::setCredentialID(const QByteArray& credentialID)
{
    m_credentialID = credentialID;
}

const QByteArray& FidoAuthenticationFactor::getCredentialID() const
{
    return m_credentialID;
}