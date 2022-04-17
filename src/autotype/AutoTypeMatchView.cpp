/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2015 David Wu <lightvector@gmail.com>
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

#include "AutoTypeMatchView.h"
#include "AutoTypeMatchModel.h"
#include "core/Entry.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QSortFilterProxyModel>

class CustomSortFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit CustomSortFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent){};
    ~CustomSortFilterProxyModel() override = default;

    // Only search the first three columns (ie, ignore sequence column)
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        auto index0 = sourceModel()->index(sourceRow, 0, sourceParent);
        auto index1 = sourceModel()->index(sourceRow, 1, sourceParent);
        auto index2 = sourceModel()->index(sourceRow, 2, sourceParent);

        return sourceModel()->data(index0).toString().contains(filterRegularExpression())
               || sourceModel()->data(index1).toString().contains(filterRegularExpression())
               || sourceModel()->data(index2).toString().contains(filterRegularExpression());
    }
};

AutoTypeMatchView::AutoTypeMatchView(QWidget* parent)
    : QTableView(parent)
    , m_model(new AutoTypeMatchModel(this))
    , m_sortModel(new CustomSortFilterProxyModel(this))
{
    m_sortModel->setSourceModel(m_model);
    m_sortModel->setDynamicSortFilter(true);
    m_sortModel->setSortLocaleAware(true);
    m_sortModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_sortModel->setFilterKeyColumn(-1);
    m_sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    setModel(m_sortModel);

    sortByColumn(0, Qt::AscendingOrder);

    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        emit matchActivated(matchFromIndex(index));
    });
}

void AutoTypeMatchView::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) && currentIndex().isValid()) {
        emit matchActivated(matchFromIndex(currentIndex()));
    } else if (event->key() == Qt::Key_PageUp) {
        moveSelection(-5);
    } else if (event->key() == Qt::Key_PageDown) {
        moveSelection(5);
    } else {
        QTableView::keyPressEvent(event);
    }
}

void AutoTypeMatchView::setMatchList(const QList<AutoTypeMatch>& matches)
{
    m_model->setMatchList(matches);
    m_sortModel->setFilterWildcard({});

    horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    selectionModel()->clear();
    emit currentMatchChanged(currentMatch());
}

void AutoTypeMatchView::selectFirstMatch()
{
    selectionModel()->setCurrentIndex(m_sortModel->index(0, 0),
                                      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    emit currentMatchChanged(currentMatch());
}

bool AutoTypeMatchView::selectMatch(const AutoTypeMatch& match)
{
    QModelIndex index = m_model->closestIndexFromMatch(match);

    if (index.isValid()) {
        selectionModel()->setCurrentIndex(m_sortModel->mapFromSource(index),
                                          QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        emit currentMatchChanged(currentMatch());
        return true;
    }

    return false;
}

void AutoTypeMatchView::filterList(const QString& filter)
{
    m_sortModel->setFilterWildcard(filter);
    setCurrentIndex(m_sortModel->index(0, 0));
}

void AutoTypeMatchView::moveSelection(int offset)
{
    auto index = currentIndex();
    auto row = index.isValid() ? index.row() : -1;
    selectRow(qBound(0, row + offset, model()->rowCount() - 1));
}

AutoTypeMatch AutoTypeMatchView::currentMatch()
{
    QModelIndexList list = selectionModel()->selectedRows();
    if (list.size() == 1) {
        return m_model->matchFromIndex(m_sortModel->mapToSource(list.first()));
    }
    return {};
}

AutoTypeMatch AutoTypeMatchView::matchFromIndex(const QModelIndex& index)
{
    if (index.isValid()) {
        return m_model->matchFromIndex(m_sortModel->mapToSource(index));
    }
    return {};
}

void AutoTypeMatchView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    auto match = matchFromIndex(current);
    emit currentMatchChanged(match);
    QTableView::currentChanged(current, previous);
}
