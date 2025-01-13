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

#ifndef KEEPASSX_GROUPVIEW_H
#define KEEPASSX_GROUPVIEW_H

#include <QTreeView>

class Database;
class Group;
class GroupModel;

class GroupView : public QTreeView
{
    Q_OBJECT

public:
    explicit GroupView(Database* db, QWidget* parent = nullptr);
    void changeDatabase(const QSharedPointer<Database>& newDb);
    void setModel(QAbstractItemModel* model) override;
    Group* currentGroup();
    void setCurrentGroup(Group* group);
    void expandGroup(Group* group, bool expand = true);
    void sortGroups(bool reverse = false);

signals:
    void groupSelectionChanged();
    void groupFocused();

private slots:
    void expandedChanged(const QModelIndex& index);
    void syncExpandedState(const QModelIndex& parent, int start, int end);
    void modelReset();
    void contextMenuShortcutPressed();
    void selectPreviousGroup();
    void selectNextGroup();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;

private:
    void recInitExpanded(Group* group);

    GroupModel* const m_model;
    bool m_updatingExpanded;
    bool m_isDragEventSrcFromOtherDb;
    Qt::DropAction m_lastAcceptedDropAction;
};

#endif // KEEPASSX_GROUPVIEW_H
