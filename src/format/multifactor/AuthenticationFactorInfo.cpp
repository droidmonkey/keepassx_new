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

#include "AuthenticationFactorInfo.h"

void AuthenticationFactorInfo::setComprehensive(bool comprehensive)
{
    m_comprehensive = comprehensive;
}

bool AuthenticationFactorInfo::isComprehensive() const
{
    return m_comprehensive;
}

void AuthenticationFactorInfo::addGroup(const QSharedPointer<AuthenticationFactorGroup>& group)
{
    m_groups.append(group);
    group->setParent(this);
}

const QList<QSharedPointer<AuthenticationFactorGroup>>& AuthenticationFactorInfo::getGroups() const
{
    return m_groups;
}
