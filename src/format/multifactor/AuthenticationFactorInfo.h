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

#ifndef KEEPASSXC_AUTHENTICATIONFACTORINFO_H
#define KEEPASSXC_AUTHENTICATIONFACTORINFO_H

#include "format/multifactor/AuthenticationFactorGroup.h"
#include <QCoreApplication>
#include <QSharedPointer>

class AuthenticationFactorGroup;

class AuthenticationFactorInfo : public QObject
{
    Q_OBJECT

public:
    explicit AuthenticationFactorInfo() = default;
    ~AuthenticationFactorInfo() override = default;

    bool isComprehensive() const;
    void setComprehensive(bool comprehensive);

    void addGroup(const QSharedPointer<AuthenticationFactorGroup>& group);
    const QList<QSharedPointer<AuthenticationFactorGroup>>& getGroups() const;

protected:
    bool m_comprehensive = false;
    QList<QSharedPointer<AuthenticationFactorGroup>> m_groups = QList<QSharedPointer<AuthenticationFactorGroup>>();
};

#endif // KEEPASSXC_AUTHENTICATIONFACTORINFO_H
