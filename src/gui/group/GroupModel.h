/*
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

#ifndef KEEPASSX_GROUPMODEL_H
#define KEEPASSX_GROUPMODEL_H

#include <QAbstractItemModel>

class Database;
class Group;

class GroupModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit GroupModel(Database* db, QObject* parent = nullptr);
    const Database* database() const
    {
        return m_db;
    }
    void changeDatabase(Database* newDb);
    QModelIndex index(Group* group) const;
    Group* groupFromIndex(const QModelIndex& index) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags flags(const QModelIndex& modelIndex) const override;
    bool
    dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    void sortChildren(Group* rootGroup, bool reverse = false);

private:
    QModelIndex parent(Group* group) const;
    void collectIndexesRecursively(QList<QModelIndex>& indexes, QList<Group*> groups);

private slots:
    void groupDataChanged(Group* group);
    void groupAboutToRemove(Group* group);
    void groupRemoved();
    void groupAboutToAdd(Group* group, int index);
    void groupAdded();
    void groupAboutToMove(Group* group, Group* toGroup, int pos);
    void groupMoved();

private:
    Database* m_db;
};

#endif // KEEPASSX_GROUPMODEL_H
