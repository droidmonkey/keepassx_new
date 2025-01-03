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

#include "AESCBCFactorKeyDerivation.h"
#include "crypto/SymmetricCipher.h"

#include <QDebug>

bool AESCBCFactorKeyDerivation::derive(QByteArray& data, const QByteArray& key, const QByteArray& salt)
{
    qDebug() << tr("Performing AES-CBC decryption on wrapped key");

    SymmetricCipher aes256;
    if (!aes256.init(SymmetricCipher::Aes256_CBC_UNPADDED, SymmetricCipher::Decrypt, key, salt)) {
        m_error = aes256.errorString();
        return false;
    }
    if (!aes256.finish(data)) {
        m_error = aes256.errorString();
        return false;
    }

    return true;
}
