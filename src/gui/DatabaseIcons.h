/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_DATABASEICONS_H
#define KEEPASSX_DATABASEICONS_H

#include <QHash>
#include <QIcon>

enum IconSize
{
    Default,
    Medium,
    Large
};

class DatabaseIcons
{
public:
    static DatabaseIcons* instance();

    static constexpr int ExpiredIconIndex = 45;

    enum Badges
    {
        ShareActive = 0,
        ShareInactive,
        Expired
    };

    QPixmap icon(int index, IconSize size = IconSize::Default);
    QPixmap applyBadge(const QPixmap& basePixmap, Badges badgeIndex);
    int count();

    int iconSize(IconSize size);

private:
    DatabaseIcons();

    static DatabaseIcons* m_instance;
    QHash<QString, QIcon> m_iconCache;
    bool m_compactMode;

    Q_DISABLE_COPY(DatabaseIcons)
};

inline DatabaseIcons* databaseIcons()
{
    return DatabaseIcons::instance();
}

#endif // KEEPASSX_DATABASEICONS_H
