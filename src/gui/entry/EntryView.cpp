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

#include "EntryView.h"

#include <QAccessible>
#include <QDrag>
#include <QGuiApplication>
#include <QHeaderView>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QWindow>

#include "gui/Icons.h"
#include "gui/SortFilterHideProxyModel.h"

#define ICON_ONLY_SECTION_SIZE 26

class PasswordStrengthItemDelegate : public QStyledItemDelegate
{
public:
    explicit PasswordStrengthItemDelegate(QObject* parent)
        : QStyledItemDelegate(parent){};

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        auto value = index.data(Qt::DecorationRole);
        if (value.isValid() && value.type() == QVariant::Color && option->rect.width() > 0) {
            // Rebuild the password strength icon to add a dark border
            QColor pen(Qt::black);
            if (option->widget) {
                pen = option->widget->palette().color(QPalette::Shadow);
            }
            auto size = option->decorationSize;
            QImage image(size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
            QPainter p(&image);
            p.setBrush(value.value<QColor>());
            p.setPen(pen);
            p.drawRect(0, 0, size.width() - 1, size.height() - 1);
            option->icon = QIcon(QPixmap::fromImage(image));
        }
    }
};

EntryView::EntryView(QWidget* parent)
    : QTreeView(parent)
    , m_model(new EntryModel(this))
    , m_sortModel(new SortFilterHideProxyModel(this))
    , m_lastIndex(-1)
    , m_lastOrder(Qt::AscendingOrder)
    , m_headerMenu(new QMenu(this))
{
    m_sortModel->setSourceModel(m_model);
    m_sortModel->setDynamicSortFilter(true);
    m_sortModel->setSortLocaleAware(true);
    m_sortModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    // Use Qt::UserRole as sort role, see EntryModel::data()
    m_sortModel->setSortRole(Qt::UserRole);
    QTreeView::setModel(m_sortModel);
    QTreeView::setItemDelegateForColumn(EntryModel::PasswordStrength, new PasswordStrengthItemDelegate(this));

    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setAlternatingRowColors(true);
    setDragEnabled(true);
    setSortingEnabled(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    // QAbstractItemView::startDrag() uses this property as the default drag action
    setDefaultDropAction(Qt::MoveAction);

    connect(this, SIGNAL(doubleClicked(QModelIndex)), SLOT(emitEntryActivated(QModelIndex)));
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        emit entrySelectionChanged(currentEntry());
    });

    new QShortcut(Qt::CTRL + Qt::Key_F10, this, SLOT(contextMenuShortcutPressed()), nullptr, Qt::WidgetShortcut);

    resetViewToDefaults();

    // Actions to toggle column visibility, each carrying the corresponding
    // column index as data
    m_columnActions = new QActionGroup(this);
    m_columnActions->setExclusive(false);
    for (int visualIndex = 0; visualIndex < header()->count(); ++visualIndex) {
        int logicalIndex = header()->logicalIndex(visualIndex);
        QString caption = m_model->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole).toString();
        if (caption.isEmpty()) {
            caption = m_model->headerData(logicalIndex, Qt::Horizontal, Qt::ToolTipRole).toString();
        }

        auto action = m_headerMenu->addAction(caption);
        action->setCheckable(true);
        action->setData(logicalIndex);
        m_columnActions->addAction(action);
    }
    connect(m_columnActions, SIGNAL(triggered(QAction*)), this, SLOT(toggleColumnVisibility(QAction*)));

    m_headerMenu->addSeparator();
    m_headerMenu->addAction(tr("Fit to window"), this, SLOT(fitColumnsToWindow()));
    m_headerMenu->addAction(tr("Fit to contents"), this, SLOT(fitColumnsToContents()));
    m_headerMenu->addSeparator();
    m_headerMenu->addAction(tr("Reset to defaults"), this, SLOT(resetViewToDefaults()));

    header()->setDefaultSectionSize(100);
    header()->setStretchLastSection(false);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(header(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(showHeaderMenu(QPoint)));
    connect(header(), SIGNAL(sectionCountChanged(int, int)), SIGNAL(viewStateChanged()));
    connect(header(), SIGNAL(sectionMoved(int, int, int)), SIGNAL(viewStateChanged()));
    connect(header(), SIGNAL(sectionResized(int, int, int)), SIGNAL(viewStateChanged()));
    connect(header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), SLOT(sortIndicatorChanged(int, Qt::SortOrder)));
}

void EntryView::contextMenuShortcutPressed()
{
    auto index = currentIndex();
    if (hasFocus() && index.isValid()) {
        emit customContextMenuRequested(visualRect(index).bottomLeft());
    }
}

void EntryView::sortIndicatorChanged(int logicalIndex, Qt::SortOrder order)
{
    int oldIndex = m_lastIndex;
    m_lastIndex = logicalIndex;
    Qt::SortOrder oldOrder = m_lastOrder;
    m_lastOrder = order;

    if (oldIndex == logicalIndex // same index
        && oldOrder == Qt::DescendingOrder // old order is descending
        && order == Qt::AscendingOrder) // new order is ascending
    {
        // a change from descending to ascending on the same column occurred
        // this sets the header into no sort order
        header()->setSortIndicator(-1, Qt::AscendingOrder);
        // do not emit any signals,  header()->setSortIndicator recursively calls this
        // function and the signals are emitted in the else part
    } else {
        // emit entrySelectionChanged even though the selection did not really change
        // this triggers the evaluation of the menu activation and anyway, the position
        // of the selected entry within the widget did change
        emit entrySelectionChanged(currentEntry());
        emit viewStateChanged();
    }

    header()->setSortIndicatorShown(true);
    resetFixedColumns();
}

void EntryView::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) && currentIndex().isValid()) {
        emitEntryActivated(currentIndex());
#ifdef Q_OS_MACOS
        // Pressing return does not emit the QTreeView::activated signal on mac os
        emit activated(currentIndex());
#endif
    }

    int last = m_model->rowCount() - 1;
    if (last > 0) {
        QAccessibleEvent accessibleEvent(this, QAccessible::PageChanged);
        if (event->key() == Qt::Key_Up && currentIndex().row() == 0) {
            QModelIndex index = m_sortModel->mapToSource(m_sortModel->index(last, 0));
            setCurrentEntry(m_model->entryFromIndex(index));
            QAccessible::updateAccessibility(&accessibleEvent);
            return;
        }

        if (event->key() == Qt::Key_Down && currentIndex().row() == last) {
            QModelIndex index = m_sortModel->mapToSource(m_sortModel->index(0, 0));
            setCurrentEntry(m_model->entryFromIndex(index));
            QAccessible::updateAccessibility(&accessibleEvent);
            return;
        }
    }

    QTreeView::keyPressEvent(event);
}

void EntryView::focusInEvent(QFocusEvent* event)
{
    emit entrySelectionChanged(currentEntry());
    QTreeView::focusInEvent(event);
}

void EntryView::displayGroup(Group* group)
{
    m_model->setGroup(group);
    header()->hideSection(EntryModel::ParentGroup);
    setFirstEntryActive();
    m_inSearchMode = false;
}

void EntryView::displaySearch(const QList<Entry*>& entries)
{
    m_model->setEntries(entries);
    header()->showSection(EntryModel::ParentGroup);

    setFirstEntryActive();

    // Reset sort column to 'Group', overrides DatabaseWidgetStateSync
    m_sortModel->sort(EntryModel::ParentGroup, Qt::AscendingOrder);
    sortByColumn(EntryModel::ParentGroup, Qt::AscendingOrder);

    m_inSearchMode = true;
}

void EntryView::setFirstEntryActive()
{
    if (m_model->rowCount() > 0) {
        QModelIndex index = m_sortModel->mapToSource(m_sortModel->index(0, 0));
        setCurrentEntry(m_model->entryFromIndex(index));
    } else {
        emit entrySelectionChanged(currentEntry());
    }
}

bool EntryView::inSearchMode()
{
    return m_inSearchMode;
}

bool EntryView::isSorted()
{
    return header()->sortIndicatorSection() != -1;
}

void EntryView::emitEntryActivated(const QModelIndex& index)
{
    Entry* entry = entryFromIndex(index);
    emit entryActivated(entry, static_cast<EntryModel::ModelColumn>(m_sortModel->mapToSource(index).column()));
}

void EntryView::setModel(QAbstractItemModel* model)
{
    Q_UNUSED(model);
    Q_ASSERT(false);
}

Entry* EntryView::currentEntry()
{
    QModelIndexList list = selectionModel()->selectedRows();
    if (list.size() == 1) {
        return m_model->entryFromIndex(m_sortModel->mapToSource(list.first()));
    } else {
        return nullptr;
    }
}

QList<Entry*> EntryView::selectedEntries()
{
    QList<Entry*> list;
    for (auto row : selectionModel()->selectedRows()) {
        list.append(m_model->entryFromIndex(m_sortModel->mapToSource(row)));
    }
    return list;
}

int EntryView::numberOfSelectedEntries()
{
    return selectionModel()->selectedRows().size();
}

void EntryView::setCurrentEntry(Entry* entry)
{
    auto index = m_model->indexFromEntry(entry);
    if (index.isValid()) {
        selectionModel()->setCurrentIndex(m_sortModel->mapFromSource(index),
                                          QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}

Entry* EntryView::entryFromIndex(const QModelIndex& index)
{
    if (index.isValid()) {
        return m_model->entryFromIndex(m_sortModel->mapToSource(index));
    } else {
        return nullptr;
    }
}

QModelIndex EntryView::indexFromEntry(Entry* entry)
{
    return m_sortModel->mapFromSource(m_model->indexFromEntry(entry));
}

int EntryView::currentEntryIndex()
{
    QModelIndexList list = selectionModel()->selectedRows();
    if (list.size() == 1) {
        auto index = m_sortModel->mapToSource(list.first());
        return index.row();
    } else {
        return -1;
    }
}

/**
 * Get current view state
 */
QByteArray EntryView::viewState() const
{
    return header()->saveState();
}

/**
 * Set view state
 */
bool EntryView::setViewState(const QByteArray& state)
{
    // Reset to unsorted first (https://bugreports.qt.io/browse/QTBUG-86694)
    header()->setSortIndicator(-1, Qt::AscendingOrder);
    bool status = header()->restoreState(state);
    resetFixedColumns();
    m_columnsNeedRelayout = state.isEmpty();
    onHeaderChanged();
    return status;
}

/**
 * Sync checkable menu actions to current state and display header context
 * menu at specified position
 */
void EntryView::showHeaderMenu(const QPoint& position)
{
    const QList<QAction*> actions = m_columnActions->actions();
    for (auto& action : actions) {
        Q_ASSERT(static_cast<QMetaType::Type>(action->data().type()) == QMetaType::Int);
        if (static_cast<QMetaType::Type>(action->data().type()) != QMetaType::Int) {
            continue;
        }
        int columnIndex = action->data().toInt();
        action->setChecked(!isColumnHidden(columnIndex));
    }
    actions[EntryModel::ParentGroup]->setVisible(inSearchMode());

    m_headerMenu->popup(mapToGlobal(position));
}

/**
 * Toggle visibility of column referenced by triggering action
 */
void EntryView::toggleColumnVisibility(QAction* action)
{
    // Verify action carries a column index as data. Since QVariant.toInt()
    // below will accept anything that's interpretable as int, perform a type
    // check here to make sure data actually IS int
    Q_ASSERT(static_cast<QMetaType::Type>(action->data().type()) == QMetaType::Int);
    if (static_cast<QMetaType::Type>(action->data().type()) != QMetaType::Int) {
        return;
    }

    // Toggle column visibility. Visible columns will only be hidden if at
    // least one visible column remains, as the table header will disappear
    // entirely when all columns are hidden
    int columnIndex = action->data().toInt();
    if (columnIndex == EntryModel::Color) {
        m_model->setBackgroundColorVisible(!action->isChecked());
    }
    if (action->isChecked()) {
        header()->showSection(columnIndex);
        if (header()->sectionSize(columnIndex) == 0) {
            header()->resizeSection(columnIndex, header()->defaultSectionSize());
        }
        resetFixedColumns();
        return;
    }
    if ((header()->count() - header()->hiddenSectionCount()) > 1) {
        header()->hideSection(columnIndex);
        return;
    }
    action->setChecked(true);
}

/**
 * Resize columns to fit all visible columns within the available space
 *
 * NOTE:
 * If EntryView::resizeEvent() is overridden at some point in the future,
 * its implementation MUST call the corresponding parent method using
 * 'QTreeView::resizeEvent(event)'. Without this, fitting to window will
 * be broken and/or work unreliably (stumbled upon during testing)
 *
 * NOTE:
 * Testing showed that it is absolutely necessary to emit signal 'viewState
 * Changed' here. Without this, an incomplete view state might get saved by
 * 'DatabaseWidgetStateSync' (e.g. only some columns resized)
 */
void EntryView::fitColumnsToWindow()
{
    header()->setSectionResizeMode(QHeaderView::Stretch);
    resetFixedColumns();
    QCoreApplication::processEvents();
    header()->setSectionResizeMode(QHeaderView::Interactive);
    resetFixedColumns();
    emit viewStateChanged();
}

/**
 * Resize columns to fit current table contents, i.e. make all contents
 * entirely visible
 */
void EntryView::fitColumnsToContents()
{
    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    resetFixedColumns();
    QCoreApplication::processEvents();
    header()->setSectionResizeMode(QHeaderView::Interactive);
    resetFixedColumns();
    emit viewStateChanged();
}

/**
 * Mark icon-only columns as fixed and resize them to icon-only section size
 */
void EntryView::resetFixedColumns()
{
    for (const auto& col : {EntryModel::Paperclip, EntryModel::Totp, EntryModel::PasswordStrength}) {
        if (!isColumnHidden(col)) {
            header()->setSectionResizeMode(col, QHeaderView::Fixed);

            // Increase column width, if sorting, to accommodate icon and arrow
            auto width = ICON_ONLY_SECTION_SIZE;
            if (header()->sortIndicatorSection() == col
                && config()->get(Config::GUI_ApplicationTheme).toString() != "classic") {
                width += 18;
            }
            header()->resizeSection(col, width);
        }
    }
    header()->setMinimumSectionSize(1);
    header()->resizeSection(EntryModel::Color, ICON_ONLY_SECTION_SIZE);
}

/**
 * Reset item view to defaults.
 */
void EntryView::resetViewToDefaults()
{
    // Reduce number of columns that are shown by default
    if (m_inSearchMode) {
        header()->showSection(EntryModel::ParentGroup);
    } else {
        header()->hideSection(EntryModel::ParentGroup);
    }
    header()->showSection(EntryModel::Title);
    header()->showSection(EntryModel::Username);
    header()->showSection(EntryModel::Url);
    header()->showSection(EntryModel::Notes);
    header()->showSection(EntryModel::Modified);
    header()->showSection(EntryModel::Paperclip);
    header()->showSection(EntryModel::Totp);

    header()->hideSection(EntryModel::Password);
    header()->hideSection(EntryModel::Expires);
    header()->hideSection(EntryModel::Created);
    header()->hideSection(EntryModel::Accessed);
    header()->hideSection(EntryModel::Attachments);
    header()->hideSection(EntryModel::Size);
    header()->hideSection(EntryModel::PasswordStrength);
    header()->hideSection(EntryModel::Color);
    onHeaderChanged();

    // Reset column order to logical indices
    for (int i = 0; i < header()->count(); ++i) {
        header()->moveSection(header()->visualIndex(i), i);
    }

    // Reorder some columns
    header()->moveSection(header()->visualIndex(EntryModel::Paperclip), 1);
    header()->moveSection(header()->visualIndex(EntryModel::Totp), 2);

    // Sort by title or group (depending on the mode)
    m_sortModel->sort(EntryModel::Title, Qt::AscendingOrder);
    sortByColumn(EntryModel::Title, Qt::AscendingOrder);

    if (m_inSearchMode) {
        m_sortModel->sort(EntryModel::ParentGroup, Qt::AscendingOrder);
        sortByColumn(EntryModel::ParentGroup, Qt::AscendingOrder);
    }

    // The following call only relayouts reliably if the widget has been shown
    // already, so only do it if the widget is visible and let showEvent() handle
    // the initial default layout.
    if (isVisible()) {
        fitColumnsToWindow();
    }
}

void EntryView::onHeaderChanged()
{
    m_model->setBackgroundColorVisible(isColumnHidden(EntryModel::Color));
}

void EntryView::showEvent(QShowEvent* event)
{
    QTreeView::showEvent(event);

    // Check if header columns need to be resized to sensible defaults.
    // This is only needed if no previous view state has been loaded.
    if (m_columnsNeedRelayout) {
        fitColumnsToWindow();
        m_columnsNeedRelayout = false;
    }
}

void EntryView::startDrag(Qt::DropActions supportedActions)
{
    auto selectedIndexes = selectionModel()->selectedRows(EntryModel::Title);
    if (selectedIndexes.isEmpty()) {
        return;
    }

    // Create a mime data object for the selected rows
    auto mimeData = m_sortModel->mimeData(selectedIndexes);
    if (!mimeData) {
        return;
    }

    // Create a temporary list widget to display the dragged items
    int i = 0;
    QListWidget listWidget;
    for (auto& index : selectedIndexes) {
        if (++i > 4) {
            int remaining = selectedIndexes.size() - i + 1;
            listWidget.addItem(tr("+ %1 entry(s)...", nullptr, remaining).arg(remaining));
            break;
        }

        QIcon icon;
        icon.addPixmap(m_sortModel->data(index, Qt::DecorationRole).value<QPixmap>());

        auto item = new QListWidgetItem;
        item->setText(m_sortModel->data(index, Qt::DisplayRole).toString());
        item->setIcon(icon);
        listWidget.addItem(item);
    }

    listWidget.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget.setStyleSheet("QListWidget { background-color: palette(highlight); border: 1px solid palette(dark); "
                             "padding: 4px; color: palette(highlighted-text); }");
    auto width = listWidget.sizeHintForColumn(0) + 2 * listWidget.frameWidth();
    auto height = listWidget.sizeHintForRow(0) * listWidget.count() + 2 * listWidget.frameWidth();
    listWidget.setFixedWidth(width);
    listWidget.setFixedHeight(height);

    // Grab the screen pixel ratio where the window resides
    // TODO: Use direct call to screen() when moving to Qt 6
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    auto screen = QGuiApplication::screenAt(window()->geometry().center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
#else
    auto screen = QGuiApplication::primaryScreen();
    if (windowHandle()) {
        screen = windowHandle()->screen();
    }
#endif

    auto pixelRatio = screen->devicePixelRatio();

    // Render the list widget to a pixmap
    QPixmap pixmap(QSize(width, height) * pixelRatio);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(pixelRatio);
    listWidget.render(&pixmap);

    // Create a drag object and start the drag
    auto drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(pixmap);
    drag->exec(supportedActions, defaultDropAction());
}

bool EntryView::isColumnHidden(int logicalIndex)
{
    return header()->isSectionHidden(logicalIndex) || header()->sectionSize(logicalIndex) == 0;
}
