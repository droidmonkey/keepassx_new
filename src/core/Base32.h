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

/* Conforms to RFC 4648. For details, see: https://tools.ietf.org/html/rfc4648
 * Use the functions Base32::addPadding/1, Base32::removePadding/1 or
 * Base32::sanitizeInput/1 to fix input or output for a particular
 * applications (e.g. to use with Google Authenticator).
 */

#ifndef BASE32_H
#define BASE32_H

#include <QByteArray>
#include <QVariant>

class Base32
{
public:
    Base32() = default;
    Q_REQUIRED_RESULT static QByteArray decode(const QByteArray&);
    Q_REQUIRED_RESULT static QByteArray encode(const QByteArray&);
    Q_REQUIRED_RESULT static QByteArray addPadding(const QByteArray&);
    Q_REQUIRED_RESULT static QByteArray removePadding(const QByteArray&);
    Q_REQUIRED_RESULT static QByteArray sanitizeInput(const QByteArray&);
};

#endif // BASE32_H
