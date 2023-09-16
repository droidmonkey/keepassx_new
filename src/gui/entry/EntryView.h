/*
 * Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 * Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_ENTRYVIEW_H
#define KEEPASSX_ENTRYVIEW_H

#include <QTreeView>

#include "gui/entry/EntryModel.h"

class Entry;
class EntryModel;
class Group;
class SortFilterHideProxyModel;
class QActionGroup;

class EntryView : public QTreeView
{
    Q_OBJECT

public:
    explicit EntryView(QWidget* parent = nullptr);
    void setModel(QAbstractItemModel* model) override;
    Entry* currentEntry();
    void setCurrentEntry(Entry* entry);
    QList<Entry*> selectedEntries();
    Entry* entryFromIndex(const QModelIndex& index);
    QModelIndex indexFromEntry(Entry* entry);
    int currentEntryIndex();
    bool inSearchMode();
    bool isSorted();
    int numberOfSelectedEntries();
    void setFirstEntryActive();
    QByteArray viewState() const;
    bool setViewState(const QByteArray& state);

    void displayGroup(Group* group);
    void displaySearch(const QList<Entry*>& entries);

signals:
    void entryActivated(Entry* entry, EntryModel::ModelColumn column);
    void entrySelectionChanged(Entry* entry);
    void viewStateChanged();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private slots:
    void emitEntryActivated(const QModelIndex& index);
    void showHeaderMenu(const QPoint& position);
    void toggleColumnVisibility(QAction* action);
    void fitColumnsToWindow();
    void fitColumnsToContents();
    void resetViewToDefaults();
    void contextMenuShortcutPressed();
    void sortIndicatorChanged(int logicalIndex, Qt::SortOrder order);

private:
    void resetFixedColumns();
    bool isColumnHidden(int logicalIndex);
    void onHeaderChanged();

    EntryModel* const m_model;
    SortFilterHideProxyModel* const m_sortModel;
    int m_lastIndex;
    Qt::SortOrder m_lastOrder;
    bool m_inSearchMode = false;
    bool m_columnsNeedRelayout = true;

    QMenu* m_headerMenu;
    QActionGroup* m_columnActions;
};

#endif // KEEPASSX_ENTRYVIEW_H
