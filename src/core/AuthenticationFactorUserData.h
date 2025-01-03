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

#ifndef KEEPASSXC_AUTHENTICATION_FACTOR_USER_DATA_H
#define KEEPASSXC_AUTHENTICATION_FACTOR_USER_DATA_H

#include <QCoreApplication>
#include <QHash>
#include <QSharedPointer>

class AuthenticationFactorUserData : public QObject
{
    Q_OBJECT

public:
    explicit AuthenticationFactorUserData() = default;
    ~AuthenticationFactorUserData() override = default;

    void addDataItem(const QString& key, const QSharedPointer<QByteArray>& value);
    QSharedPointer<QByteArray> getDataItem(const QString& key) const;

protected:
    QHash<QString, QSharedPointer<QByteArray>> m_data;
};

#endif // KEEPASSXC_AUTHENTICATION_FACTOR_USER_DATA_H
