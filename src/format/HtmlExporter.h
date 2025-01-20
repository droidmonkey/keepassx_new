/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSX_HTMLEXPORTER_H
#define KEEPASSX_HTMLEXPORTER_H

#include <QSharedPointer>
#include <QString>

#include "core/Group.h"

class Database;
class Group;
class QIODevice;

class HtmlExporter
{
public:
    bool exportDatabase(const QString& filename,
                        const QSharedPointer<const Database>& db,
                        bool sorted = true,
                        bool ascending = true);
    bool exportDatabase(QIODevice* device,
                        const QSharedPointer<const Database>& db,
                        bool sorted = true,
                        bool ascending = true);
    QString exportDatabase(const QSharedPointer<const Database>& db, bool sorted = true, bool ascending = true);
    QString errorString() const;

    virtual ~HtmlExporter() = default;

protected:
    virtual QString groupIconToHtml(const Group* group);
    virtual QString entryIconToHtml(const Entry* entry);

private:
    QString exportGroup(const Group& group, QString path = QString(), bool sorted = true, bool ascending = true);
    QString exportHeader(const QSharedPointer<const Database>& db);
    QString exportFooter();

    QString m_error;
};

#endif // KEEPASSX_HTMLEXPORTER_H
