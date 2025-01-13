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

#include "GroupModel.h"

#include <QMimeData>

#include "core/Database.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "gui/DatabaseIcons.h"
#include "gui/Icons.h"
#include "gui/MainWindow.h"
#include "keeshare/KeeShare.h"

GroupModel::GroupModel(Database* db, QObject* parent)
    : QAbstractItemModel(parent)
    , m_db(nullptr)
{
    changeDatabase(db);
}

void GroupModel::changeDatabase(Database* newDb)
{
    beginResetModel();

    m_db = newDb;

    // clang-format off
    connect(m_db, SIGNAL(groupDataChanged(Group*)), SLOT(groupDataChanged(Group*)));
    connect(m_db, SIGNAL(groupAboutToAdd(Group*,int)), SLOT(groupAboutToAdd(Group*,int)));
    connect(m_db, SIGNAL(groupAdded()), SLOT(groupAdded()));
    connect(m_db, SIGNAL(groupAboutToRemove(Group*)), SLOT(groupAboutToRemove(Group*)));
    connect(m_db, SIGNAL(groupRemoved()), SLOT(groupRemoved()));
    connect(m_db, SIGNAL(groupAboutToMove(Group*,Group*,int)), SLOT(groupAboutToMove(Group*,Group*,int)));
    connect(m_db, SIGNAL(groupMoved()), SLOT(groupMoved()));
    // clang-format on

    endResetModel();
}

int GroupModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        // we have exactly 1 root item
        return 1;
    } else {
        const Group* group = groupFromIndex(parent);
        return group->children().size();
    }
}

int GroupModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);

    return 1;
}

QModelIndex GroupModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return {};
    }

    Group* group;

    if (!parent.isValid()) {
        group = m_db->rootGroup();
    } else {
        group = groupFromIndex(parent)->children().at(row);
    }

    return createIndex(row, column, group);
}

QModelIndex GroupModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return {};
    }

    return parent(groupFromIndex(index));
}

QModelIndex GroupModel::parent(Group* group) const
{
    Group* parentGroup = group->parentGroup();

    if (!parentGroup) {
        // index is already the root group
        return {};
    } else {
        const Group* grandParentGroup = parentGroup->parentGroup();
        if (!grandParentGroup) {
            // parent is the root group
            return createIndex(0, 0, parentGroup);
        } else {
            return createIndex(grandParentGroup->children().indexOf(parentGroup), 0, parentGroup);
        }
    }
}

QVariant GroupModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    Group* group = groupFromIndex(index);

    if (role == Qt::DisplayRole) {
        QString nameTemplate = "%1";
#if defined(WITH_XC_KEESHARE)
        nameTemplate = KeeShare::indicatorSuffix(group, nameTemplate);
#endif
        return nameTemplate.arg(group->name());
    } else if (role == Qt::DecorationRole) {
        return Icons::groupIconPixmap(group);
    } else if (role == Qt::FontRole) {
        QFont font;
        if (group->isExpired()) {
            font.setStrikeOut(true);
        }
        return font;
    } else if (role == Qt::ToolTipRole) {
        QString tooltip;
        if (!group->parentGroup()) {
            // only show a tooltip for the root group
            tooltip = m_db->filePath();
        }
        return tooltip;
    } else {
        return {};
    }
}

QVariant GroupModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section);
    Q_UNUSED(orientation);
    Q_UNUSED(role);

    return {};
}

QModelIndex GroupModel::index(Group* group) const
{
    int row;

    if (!group->parentGroup()) {
        row = 0;
    } else {
        row = group->parentGroup()->children().indexOf(group);
    }

    return createIndex(row, 0, group);
}

Group* GroupModel::groupFromIndex(const QModelIndex& index) const
{
    Q_ASSERT(index.internalPointer());

    return static_cast<Group*>(index.internalPointer());
}

Qt::DropActions GroupModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction;
}

Qt::ItemFlags GroupModel::flags(const QModelIndex& modelIndex) const
{
    if (!modelIndex.isValid()) {
        return Qt::NoItemFlags;
    } else if (modelIndex == index(0, 0)) {
        return QAbstractItemModel::flags(modelIndex) | Qt::ItemIsDropEnabled;
    } else {
        return QAbstractItemModel::flags(modelIndex) | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled;
    }
}

bool GroupModel::dropMimeData(const QMimeData* data,
                              Qt::DropAction action,
                              int row,
                              int column,
                              const QModelIndex& parent)
{
    Q_UNUSED(column);

    if (action == Qt::IgnoreAction) {
        return true;
    } else if (action != Qt::MoveAction && action != Qt::CopyAction && action != ::Qt::LinkAction) {
        return false;
    }

    if (!data || !parent.isValid()) {
        return false;
    }

    // check if the format is supported
    QStringList types = mimeTypes();
    Q_ASSERT(types.size() == 2);
    bool isGroup = data->hasFormat(types.at(0));
    bool isEntry = data->hasFormat(types.at(1));
    if (!isGroup && !isEntry) {
        return false;
    }

    if (row > rowCount(parent)) {
        row = rowCount(parent);
    }

    auto showErrorMessage = [](const QString& errorMessage){
        if(auto dbWidget = getMainWindow()->currentDatabaseWidget()) {
            dbWidget->showErrorMessage(errorMessage);
        }
    };

    // decode and insert
    QByteArray encoded = data->data(isGroup ? types.at(0) : types.at(1));
    QDataStream stream(&encoded, QIODevice::ReadOnly);

    Group* parentGroup = groupFromIndex(parent);

    if (isGroup) {
        QUuid dbUuid;
        QUuid groupUuid;
        stream >> dbUuid >> groupUuid;

        Database* sourceDb = Database::databaseByUuid(dbUuid);
        if (!sourceDb) {
            return false;
        }

        Group* dragGroup = sourceDb->rootGroup()->findGroupByUuid(groupUuid);
        if (!dragGroup || dragGroup == sourceDb->rootGroup()) {
            return false;
        }

        if (dragGroup == parentGroup || parentGroup->isDescendantOf(dragGroup)) {
            return false;
        }

        if (parentGroup == dragGroup->parent() && row > parentGroup->children().indexOf(dragGroup)) {
            row--;
        }

        Database* targetDb = parentGroup->database();
        Group* group = dragGroup;

        if (sourceDb != targetDb) {
            if (action == Qt::MoveAction || action == Qt::LinkAction) { // clang-format off

                Group* binGroup = sourceDb->metadata()->recycleBin();
                if(binGroup && binGroup->uuid() == dragGroup->uuid()) {
                    showErrorMessage(tr("Move error: \"%1\" group cannot be moved").arg(binGroup->name()));
                    return true;
                }
                
                // Collect all UUID(s) or short-circuit when UUID is deleted in targetDb
                QSet<QUuid> uuidSet;
                bool complexMove = group->walk(true,
                    [&](const Group* group) {
                        uuidSet.insert(group->uuid());
                        return targetDb->containsDeletedObject(group->uuid());
                    },
                    [&](const Entry* entry) { 
                        uuidSet.insert(entry->uuid()); 
                        return targetDb->containsDeletedObject(entry->uuid());
                    }
                );

                // Unable to handle complex moves until the Merger interface supports single group/entry merging
                if (complexMove || targetDb->rootGroup()->walk(true,
                    [&](const Group* group)-> bool {
                        return uuidSet.contains(group->uuid());
                    },
                    [&](const Entry* entry) -> bool {
                        return uuidSet.contains(entry->uuid());
                    }
                )) {
                    showErrorMessage(tr("Move error: the group or one of it's descendants is already present in this database"));
                    return true;
                }
            } // clang-format on

            if (action == Qt::MoveAction) { // -- Tracked move

                // A clone with new UUID but original CreationTime
                group = dragGroup->clone(Entry::CloneFlags(Entry::CloneCopy & ~Entry::CloneResetCreationTime),
                                         Group::CloneFlags(Group::CloneCopy & ~Group::CloneResetCreationTime));
                // Original UUID is marked as deleted to propagate the move to dbs that merge with this one
                delete dragGroup;
            } else if (action == Qt::LinkAction) { // -- Untracked move

                QList<DeletedObject> deletedObjects(sourceDb->deletedObjects());
                group = dragGroup->clone(Entry::CloneExactCopy, Group::CloneExactCopy);
                delete dragGroup;
                // Unmark UUID(s) as deleted by restoring the previous list
                sourceDb->setDeletedObjects(deletedObjects);
            } else {
                group = dragGroup->clone(Entry::CloneCopy);
            }

            targetDb->metadata()->copyCustomIcons(group->customIconsRecursive(), sourceDb->metadata());
        } else if (action == Qt::CopyAction) {
            group = dragGroup->clone(Entry::CloneCopy);
        }

        group->setParent(parentGroup, row);
    } else {
        if (row != -1) {
            return false;
        }

        int entries{0}, entriesNotMoved{0};
        while (!stream.atEnd()) {
            QUuid dbUuid;
            QUuid entryUuid;
            stream >> dbUuid >> entryUuid;
            ++entries;

            Database* sourceDb = Database::databaseByUuid(dbUuid);
            if (!sourceDb) {
                continue;
            }

            Entry* dragEntry = sourceDb->rootGroup()->findEntryByUuid(entryUuid);
            if (!dragEntry) {
                continue;
            }

            Database* targetDb = parentGroup->database();
            Entry* entry = dragEntry;

            if (sourceDb != targetDb) {
                if (action == Qt::MoveAction || action == Qt::LinkAction) { // clang-format off

                    // Unable to handle complex moves until the Merger interface supports single group/entry merging
                    if (targetDb->containsDeletedObject(dragEntry->uuid()) || 
                        targetDb->rootGroup()->walkEntries([=](const Entry* entry) { 
                            return dragEntry->uuid() == entry->uuid(); 
                        }
                    )) {
                        ++entriesNotMoved;
                        continue;
                    }
                } // clang-format on

                if (action == Qt::MoveAction) { // -- Tracked move

                    // A clone with new UUID but original CreationTime
                    entry = dragEntry->clone(Entry::CloneFlags(Entry::CloneCopy & ~Entry::CloneResetCreationTime));
                    // Original UUID is marked as deleted to propagate the move to dbs that merge with this one
                    delete dragEntry;
                } else if (action == Qt::LinkAction) { // -- Untracked move

                    QList<DeletedObject> deletedObjects(sourceDb->deletedObjects());
                    entry = dragEntry->clone(Entry::CloneExactCopy);
                    delete dragEntry;
                    // Unmark UUID as deleted by restoring the previous list
                    sourceDb->setDeletedObjects(deletedObjects);
                } else {
                    entry = dragEntry->clone(Entry::CloneCopy);
                }

                targetDb->metadata()->copyCustomIcon(entry->iconUuid(), sourceDb->metadata());
            } else if (action == Qt::CopyAction) {
                entry = dragEntry->clone(Entry::CloneCopy);
            }

            entry->setGroup(parentGroup);
        }

        if (entriesNotMoved) {
            showErrorMessage(
                tr("Move error: %1 of %2 entry(s) are already present in this database").arg(entriesNotMoved).arg(entries));
        }
    }

    return true;
}

QStringList GroupModel::mimeTypes() const
{
    QStringList types;
    types << "application/x-keepassx-group";
    types << "application/x-keepassx-entry";
    return types;
}

QMimeData* GroupModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.isEmpty()) {
        return nullptr;
    }

    auto data = new QMimeData();
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);

    QSet<Group*> seenGroups;

    for (const QModelIndex& index : indexes) {
        if (!index.isValid()) {
            continue;
        }

        Group* group = groupFromIndex(index);
        if (!seenGroups.contains(group)) {
            // make sure we don't add groups multiple times when we get indexes
            // with the same row but different columns
            stream << m_db->uuid() << group->uuid();
            seenGroups.insert(group);
        }
    }

    if (seenGroups.isEmpty()) {
        delete data;
        return nullptr;
    } else {
        data->setData(mimeTypes().at(0), encoded);
        return data;
    }
}

void GroupModel::groupDataChanged(Group* group)
{
    QModelIndex ix = index(group);
    emit dataChanged(ix, ix);
}

void GroupModel::groupAboutToRemove(Group* group)
{
    Q_ASSERT(group->parentGroup());

    QModelIndex parentIndex = parent(group);
    Q_ASSERT(parentIndex.isValid());
    int pos = group->parentGroup()->children().indexOf(group);
    Q_ASSERT(pos != -1);

    beginRemoveRows(parentIndex, pos, pos);
}

void GroupModel::groupRemoved()
{
    endRemoveRows();
}

void GroupModel::groupAboutToAdd(Group* group, int index)
{
    Q_ASSERT(group->parentGroup());

    QModelIndex parentIndex = parent(group);

    beginInsertRows(parentIndex, index, index);
}

void GroupModel::groupAdded()
{
    endInsertRows();
}

void GroupModel::groupAboutToMove(Group* group, Group* toGroup, int pos)
{
    Q_ASSERT(group->parentGroup());

    QModelIndex oldParentIndex = parent(group);
    QModelIndex newParentIndex = index(toGroup);
    int oldPos = group->parentGroup()->children().indexOf(group);
    if (group->parentGroup() == toGroup && pos > oldPos) {
        // beginMoveRows() has a bit different semantics than Group::setParent() and
        // QList::move() when the new position is greater than the old
        pos++;
    }

    bool moveResult = beginMoveRows(oldParentIndex, oldPos, oldPos, newParentIndex, pos);
    Q_UNUSED(moveResult);
    Q_ASSERT(moveResult);
}

void GroupModel::groupMoved()
{
    endMoveRows();
}

void GroupModel::sortChildren(Group* rootGroup, bool reverse)
{
    emit layoutAboutToBeChanged();

    QList<QModelIndex> oldIndexes;
    collectIndexesRecursively(oldIndexes, rootGroup->children());

    rootGroup->sortChildrenRecursively(reverse);

    QList<QModelIndex> newIndexes;
    collectIndexesRecursively(newIndexes, rootGroup->children());

    for (int i = 0; i < oldIndexes.count(); i++) {
        changePersistentIndex(oldIndexes[i], newIndexes[i]);
    }

    emit layoutChanged();
}

void GroupModel::collectIndexesRecursively(QList<QModelIndex>& indexes, QList<Group*> groups)
{
    for (auto group : groups) {
        indexes.append(index(group));
        collectIndexesRecursively(indexes, group->children());
    }
}
