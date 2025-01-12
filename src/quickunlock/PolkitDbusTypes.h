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

#ifndef KEEPASSXC_POLKITDBUSTYPES_H
#define KEEPASSXC_POLKITDBUSTYPES_H

#include <QtDBus>

class PolkitSubject
{
public:
    QString kind;
    QVariantMap details;

    static void registerMetaType();

    friend QDBusArgument& operator<<(QDBusArgument& argument, const PolkitSubject& subject);

    friend const QDBusArgument& operator>>(const QDBusArgument& argument, PolkitSubject& subject);
};

class PolkitAuthorizationResults
{
public:
    bool is_authorized;
    bool is_challenge;
    QMap<QString, QString> details;

    static void registerMetaType();

    friend QDBusArgument& operator<<(QDBusArgument& argument, const PolkitAuthorizationResults& subject);

    friend const QDBusArgument& operator>>(const QDBusArgument& argument, PolkitAuthorizationResults& subject);
};

Q_DECLARE_METATYPE(PolkitSubject);
Q_DECLARE_METATYPE(PolkitAuthorizationResults);

#endif // KEEPASSXC_POLKITDBUSTYPES_H
